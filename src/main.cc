#include <poll.h>
#include <xwiimote.h>

#include <iostream>
#include <string>

#include <Eigen/Dense>

#include "logging.h"

using namespace std;

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

static void HandleBalanceBoard( const ::xwii_event &event )
{
//        tl = event.get_abs(2)[0]
//        tr = event.get_abs(0)[0]
//        br = event.get_abs(3)[0]
//        bl = event.get_abs(1)[0]
  Eigen::Vector4d values;
  values <<
    event.v.abs[0].x, event.v.abs[1].x, event.v.abs[2].x, event.v.abs[3].x;

  values = values / 100.0 * 2.20462;

  fmt::print( "Values: {:6.2f} {:6.2f} {:6.2f} {:6.2f}\r",
       values[0], values[1], values[2], values[3] );
  std::cout << std::flush;
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
      HandleBalanceBoard( event );
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

int main(int argc, char **argv) {
  // DELETE THESE.  Used to suppress unused variable warnings.
  (void)argc;
  (void)argv;

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
