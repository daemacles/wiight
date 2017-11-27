#include <cassert>
#include <poll.h>

#include <iostream>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <sqlite3.h>
#include <xwiimote.h>

#include "logging.h"

using namespace std;
using namespace Eigen;

#define CHECKSQL(X, db, msg) do { int rc = X; if( rc != SQLITE_OK ) { \
  FATAL( "{}: {}", msg, sqlite3_errmsg( db )); }} while(0)

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

VectorXd GetCalibrationCoefficients ()
{
  // Compute calibration coefficients

  vector<double> scale_values_raw{ 71.5, 52., 193.5, 141., 146.5, 203., 145.5,
    173.5, 143.5 };
  vector<double> wii_values_raw{ 63., 43.65, 185.5, 133.2, 138.6, 194.5, 137.5,
    165.2, 135.6 };

  assert( scale_values_raw.size() == wii_values_raw.size() );

  VectorXd scale_values{ scale_values_raw.size() };
  VectorXd wii_values{ wii_values_raw.size() };

  for( size_t idx=0; idx < scale_values_raw.size(); ++idx )
  {
    scale_values( idx ) = scale_values_raw[ idx ];
    wii_values( idx ) = wii_values_raw[ idx ];
  }

  MatrixXd A = MatrixXd::Ones( scale_values.rows(), 3 );
  A.col(0) = wii_values.asDiagonal()*wii_values;
  A.col(1) = wii_values;

  return (A.adjoint() * A).llt().solve( A.adjoint() * scale_values );
}

static void Run( ::xwii_iface *iface )
{
  ::xwii_event event;
  	int ret = 0, fds_num;
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

  VectorXd coefs = GetCalibrationCoefficients();

	while (true) {
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

    switch (event.type) {
    case XWII_EVENT_GONE:
      INFO("Device gone");
      fds[1].fd = -1;
      fds[1].events = 0;
      fds_num = 1;
      break;
    case XWII_EVENT_WATCH:
      break;
    case XWII_EVENT_KEY:
      INFO( "Key event" );
      break;
    case XWII_EVENT_ACCEL:
      INFO( "Accel event" );
      break;
    case XWII_EVENT_IR:
      INFO( "IR event" );
      break;
    case XWII_EVENT_MOTION_PLUS:
      INFO( "Motion+ event" );
      break;
    case XWII_EVENT_NUNCHUK_KEY:
    case XWII_EVENT_NUNCHUK_MOVE:
      INFO( "Nunchuk event" );
      break;
    case XWII_EVENT_CLASSIC_CONTROLLER_KEY:
    case XWII_EVENT_CLASSIC_CONTROLLER_MOVE:
      INFO( "Classic controller event" );
      break;
    case XWII_EVENT_BALANCE_BOARD:
      HandleBalanceBoard( event, coefs );
      break;
    case XWII_EVENT_PRO_CONTROLLER_KEY:
    case XWII_EVENT_PRO_CONTROLLER_MOVE:
      INFO( "Pro controller event" );
      break;
    case XWII_EVENT_GUITAR_KEY:
    case XWII_EVENT_GUITAR_MOVE:
      INFO( "Guitar event" );
      break;
    case XWII_EVENT_DRUMS_KEY:
    case XWII_EVENT_DRUMS_MOVE:
      INFO( "Drums event" );
      break;
    }
  }
}

void LoadDefaultCalibration ()
{
  sqlite3 *db;

  int rc = sqlite3_open( "/tmp/db.sqlite", &db );
  if( rc != SQLITE_OK )
  {
    FATAL( "Couldn't open database: {}", sqlite3_errmsg( db ));
  }

  string sql = R"(
  DROP TABLE IF EXISTS calibration;
  CREATE TABLE calibration( id INTEGER PRIMARY KEY, scale DOUBLE, wii DOUBLE );
  )";

  char *err_msg = nullptr;
  rc = sqlite3_exec( db, sql.c_str(), 0, 0, &err_msg );
  if( rc != SQLITE_OK )
  {
    FATAL( "Couldn't create table: {}", sqlite3_errmsg( db ));
    sqlite3_free( err_msg );
  }

  vector<double> scale_values{ 71.5, 52., 193.5, 141., 146.5, 203., 145.5,
    173.5, 143.5 };
  vector<double> wii_values{ 63., 43.65, 185.5, 133.2, 138.6, 194.5, 137.5,
    165.2, 135.6 };
  assert( scale_values.size() == wii_values.size() );

  sql = "INSERT INTO calibration(scale, wii) VALUES (?, ?);";
  sqlite3_stmt *res;
  rc = sqlite3_prepare_v2( db, sql.c_str(), -1, &res, 0 );
  if( rc != SQLITE_OK )
  {
    FATAL( "Failed to prepare statement: {}", sqlite3_errmsg( db ));
  }

  for( size_t idx=0; idx < scale_values.size(); ++idx )
  {
    sqlite3_bind_double( res, 1, scale_values[idx] );
    sqlite3_bind_double( res, 2, wii_values[idx] );
    rc = sqlite3_step( res );
    if( rc != SQLITE_DONE )
    {
      FATAL( "Can't insert data: {}", sqlite3_errmsg( db ));
    }
    sqlite3_reset( res );
  }

  sqlite3_finalize( res );
  sqlite3_close( db );
}

void Sqlite()
{
  sqlite3 *db;
  sqlite3_stmt *res;

  int rc = sqlite3_open( "/tmp/db.sqlite", &db );
  if( rc != SQLITE_OK )
  {
    FATAL( "Couldn't open database: {}", sqlite3_errmsg( db ));
  }

  rc = sqlite3_prepare_v2( db, "SELECT SQLITE_VERSION()", -1, &res, 0 );
  if( rc != SQLITE_OK )
  {
    FATAL( "Couldn't fetch data: {}", sqlite3_errmsg( db ));
  }

  rc = sqlite3_step( res );

  if( rc == SQLITE_ROW )
  {
    INFO( "Using SQlite3 version {}", sqlite3_column_text( res, 0 ));
  }

  sqlite3_finalize( res );
  sqlite3_close( db );

}

int main(int argc, char **argv) {
  // DELETE THESE.  Used to suppress unused variable warnings.
  (void)argc;
  (void)argv;

  Sqlite();
  LoadDefaultCalibration();

  auto iface = WaitForBalanceBoard();

  uint64_t available_types = xwii_iface_available( iface );
  INFO( "Available types: {:x}", available_types );

  int ret =
    xwii_iface_open( iface, available_types | XWII_IFACE_BALANCE_BOARD );

  if( ret != 0 )
  {
    FATAL( "Can't open interface" );
  };

  Run( iface );

  return 0;
}
