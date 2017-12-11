#include <cassert>
#include <poll.h>
#include <signal.h>

#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <sqlite3.h>
#include <xwiimote.h>

#include <argh.h>
#include <sqlite_modern_cpp.h>
#include <event2/event.h>
#include <evhtp.h>

#include "logging.h"

using namespace std;
using namespace Eigen;
namespace fs = std::experimental::filesystem;

#define CHECKSQL(X, db, msg) do { int rc = X; if( rc != SQLITE_OK ) { \
  FATAL( "{}: {}", msg, sqlite3_errmsg( db )); }} while(0)

static constexpr const char *DATABASE = "/tmp/db.sqlite";
static bool s_should_quit = false;

static xwii_iface* WaitForBalanceBoard ()
{
  auto monitor = xwii_monitor_new( true, false );

  int fd = ::xwii_monitor_get_fd( monitor, true );
  if( fd == -1 )
  {
    FATAL( "XWii FD is invalid" );
  }

  INFO( "Connected to devices" );
  char *dev_path;
  string dev_name;
  while( (dev_path = ::xwii_monitor_poll( monitor )) != nullptr ) {
    INFO( "{}", dev_path );

    ::xwii_iface *dev;
    if( ::xwii_iface_new( &dev, dev_path ) < 0 )
    {
      FATAL( "Couldn't create interface for {}", dev_path );
    }

    char *dev_type;
    if( ::xwii_iface_get_devtype( dev, &dev_type ) < 0 )
    {
      FATAL( "Couldn't get devtype for {}", dev_path );
    }
    dev_name = dev_type;
    free( dev_type );
    free( dev_path );

    INFO( " is a {}", dev_name );

    if( dev_name == "balanceboard" )
    {
      return dev;
    }
  }

  ::xwii_monitor_unref( monitor );

  FATAL( "No balance boards connected" );

  return nullptr;
}

static void HandleBalanceBoard( const ::xwii_event &event,
                               const VectorXd &coefs )
{
//        tl = event.get_abs(2)[0]
//        tr = event.get_abs(0)[0]
//        br = event.get_abs(3)[0]
//        bl = event.get_abs(1)[0]
  Eigen::Vector4d values;
  values <<
    event.v.abs[0].x, event.v.abs[1].x, event.v.abs[2].x, event.v.abs[3].x;

  values = values / 100.0 * 2.20462;
  double weight = values.sum();
  weight = coefs[0]*weight*weight + coefs[1]*weight + coefs[2];

  fmt::print( "Values: {:6.2f} <== {:6.2f} {:6.2f} {:6.2f} {:6.2f}\r",
       weight, values[0], values[1], values[2], values[3] );
  std::cout << std::flush;
}

VectorXd GetCalibrationCoefficients ( sqlite::database &db )
{
  vector<double> scale_values_raw;
  vector<double> wii_values_raw;

  db << "SELECT scale, wii FROM calibration;"
    >> [&]( double scale, double wii ) {
      scale_values_raw.push_back( scale );
      wii_values_raw.push_back( wii );
    };

  VectorXd scale_values{ scale_values_raw.size() };
  VectorXd wii_values{ wii_values_raw.size() };

  for( size_t idx=0; idx < scale_values_raw.size(); ++idx )
  {
    scale_values( idx ) = scale_values_raw[ idx ];
    wii_values( idx ) = wii_values_raw[ idx ];
  }

  // Compute calibration coefficients
  MatrixXd A = MatrixXd::Ones( scale_values.rows(), 3 );
  A.col(0) = wii_values.asDiagonal()*wii_values;
  A.col(1) = wii_values;

  return (A.adjoint() * A).llt().solve( A.adjoint() * scale_values );
}

static VectorXd Sample( sqlite::database &db, ::xwii_iface *iface, int num )
{
  ::xwii_event event;
  int ret = 0;
  int fds_num;
  struct pollfd fds[2];

  memset(fds, 0, sizeof(fds));
  fds[0].fd = 0;
  fds[0].events = POLLIN;
  fds[1].fd = ::xwii_iface_get_fd(iface);
  fds[1].events = POLLIN;
  fds_num = 2;

  ret = xwii_iface_watch(iface, true);
  if (ret)
  {
    ERROR("Cannot initialize hotplug watch descriptor");
  }

  VectorXd coefs = GetCalibrationCoefficients( db );

  vector<double> raw_weights;
  while (num--) {
    ret = poll(fds, fds_num, -1);
    if (ret < 0) {
      if (errno != EINTR) {
        ret = -errno;
        ERROR("Cannot poll fds: {}", ret);
        break;
      }
    }

    ret = xwii_iface_dispatch(iface, &event, sizeof(event));
    if (ret)
    {
      if (ret != -EAGAIN)
      {
        ERROR("Read failed with err: {}", ret);
        break;
      }
    }

    if( event.type == XWII_EVENT_BALANCE_BOARD )
    {
      Eigen::Vector4d values;
      values <<
        event.v.abs[0].x, event.v.abs[1].x, event.v.abs[2].x, event.v.abs[3].x;

      values = values / 100.0 * 2.20462;
      double weight = values.sum();
      weight = coefs[0]*weight*weight + coefs[1]*weight + coefs[2];

      raw_weights.push_back( weight );

      fmt::print( "Values: {:6.2f} <== {:6.2f} {:6.2f} {:6.2f} {:6.2f}\r",
                 weight, values[0], values[1], values[2], values[3] );
      std::cout << std::flush;

      HandleBalanceBoard( event, coefs );
    }
  }

  VectorXd weights( raw_weights.size() );
  for( size_t i=0; i < raw_weights.size(); ++i )
  {
    weights(i) = raw_weights[i];
  }

  return weights;
}

void LoadDefaultCalibration ( sqlite::database &db )
{
  bool has_calibration_data = false;
  db << "SELECT name FROM sqlite_master WHERE type='table' AND name='calibration';"
    >> [&has_calibration_data]( string name ) {
      (void) name;
      has_calibration_data = true;
    };

  if( has_calibration_data ) {
    return;
  }
  else
  {
    WARN( "Missing calibration data, loading default values." );
  }

  db << "DROP TABLE IF EXISTS calibration;";
  db << "CREATE TABLE calibration( "
    "id INTEGER PRIMARY KEY,"
    "scale DOUBLE,"
    "wii DOUBLE );";

  vector<double> scale_values{ 71.5, 52., 193.5, 141., 146.5, 203., 145.5,
    173.5, 143.5 };
  vector<double> wii_values{ 63., 43.65, 185.5, 133.2, 138.6, 194.5, 137.5,
    165.2, 135.6 };
  assert( scale_values.size() == wii_values.size() );

  for( size_t idx=0; idx < scale_values.size(); ++idx )
  {
    db << "INSERT INTO calibration (scale, wii) values (?, ?);"
      << scale_values[idx]
      << wii_values[idx];
  }
}

sqlite::database Sqlite()
{
  sqlite::database db( DATABASE );
  string version;
  db << "SELECT SQLITE_VERSION();"
    >> []( string version ) {
      INFO( "Using SQLite3 version {}", version );
    };
  return db;
}

struct Header
{
  string key;
  string value;
};

void RootCallback( evhtp_request_t *req, void *data )
{
  (void) data;

  if( req->uri->path->full )
  {
    INFO( "Got a request for '{}'", req->uri->path->full );
  }
  else
  {
    INFO( "Unknown query" );
    evhtp_send_reply( req, EVHTP_RES_NOTFOUND );
    return;
  }

  string response = R"(
  <html>
  <head>
  <title>Unknown Resource</title>
  </head>
  <body>
  <h1>Unknown Resource</h1>
  <p><a href="/">Go Home</a></p>
  </body>
  </html>
  )";

  fs::path file_path{ "."s + req->uri->path->full };
  if( file_path == fs::path( "./" ))
  {
    file_path = { "./index.html" };
  }

  if( fs::exists( file_path ) && fs::is_regular_file( file_path ))
  {
    ifstream file_in{ file_path };
    ostringstream buf;
    buf << file_in.rdbuf();
    response = buf.str();
  }
  else
  {
    WARN( "Unknown path '{}', using default message", file_path );
  }

  evbuffer_add( req->buffer_out, response.c_str(), response.size() );

  Header headers[] = {
    { "Content-Type", "text/html" },
    { "Content-Language", "en" }
  };
  for( const auto& header : headers )
  {
    evhtp_headers_add_header( req->headers_out, evhtp_header_new(
            header.key.c_str(), header.value.c_str(), 1, 1 ));
  }

  evhtp_send_reply( req, EVHTP_RES_OK );
}


void SigintHandler( evutil_socket_t fd, short event, void *arg )
{
  (void) fd;
  (void) event;
  evbase_t *evbase =  reinterpret_cast<evbase_t*>( arg );
  INFO( "Got SIGINT, bye bye!" );
  s_should_quit = true;
  event_base_loopbreak( evbase );
}

void HttpLoop ()
{
  evbase_t *evbase = event_base_new();
  evhtp_t *htp = evhtp_new( evbase, nullptr );
  evhtp_set_glob_cb( htp, "*", RootCallback, nullptr );

  evhtp_bind_socket( htp, "0.0.0.0", 8080, 2048 );

  event *signal_int = evsignal_new( evbase, SIGINT, SigintHandler, evbase );
  event_add( signal_int, nullptr );
  INFO( "Started HTTP server on port 8080" );
  event_base_loop( evbase, 0 );

  evhtp_unbind_socket( htp );
  event_free( signal_int );
  evhtp_free( htp );
  event_base_free( evbase );
  INFO( "Stopped HTTP server" );
}

int main(int argc, char **argv) {
  // DELETE THESE.  Used to suppress unused variable warnings.
  (void)argc;
  (void)argv;

  HttpLoop();
  return 0;

  auto iface = WaitForBalanceBoard();

  uint64_t available_types = xwii_iface_available( iface );
  INFO( "Available types: {:x}", available_types );

  int ret =
    xwii_iface_open( iface, available_types | XWII_IFACE_BALANCE_BOARD );

  if( ret != 0 )
  {
    FATAL( "Can't open interface" );
  };

  auto db = Sqlite();
  LoadDefaultCalibration( db );
  Sample( db, iface, 100 );

  return 0;
}
