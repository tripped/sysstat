#! /bin/sh
#

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/lib/sysstat/sadc
NAME=sadc
DESC="the system activity data collector"

test -f $DAEMON || exit 0

set -e

case "$1" in
  start|restart|reload|force-reload)
	echo -n "Starting $DESC: "
	start-stop-daemon --start --quiet --exec $DAEMON
	echo "$NAME."
	;;
  stop)
	;;

  *)
	echo "Usage: $N {start|stop|restart|reload|force-reload}" >&2
	exit 1
	;;
esac

exit 0
