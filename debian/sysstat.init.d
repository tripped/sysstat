#! /bin/sh
#

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/lib/sysstat/sadc
NAME=sadc
DESC="the system activity data collector"

test -f "$DAEMON" || exit 0
umask 022

# our configuration file
DEFAULT=/etc/default/sysstat

# default settings...
ENABLED="false"
OPTIONS="-F -L -"

# ...overriden in the configuration file
test -r "$DEFAULT" && . "$DEFAULT"

set -e

case "$1" in
  start|restart|reload|force-reload)
        if "$ENABLED" = "true" ; then
                echo -n "Starting $DESC: "
                start-stop-daemon --start --quiet --exec $DAEMON -- $OPTIONS
                echo "$NAME."
        else
                echo "$NAME not enabled in ${DEFAULT}, not starting."
        fi
        ;;
  stop)
        ;;

  *)
        echo "Usage: $N {start|stop|restart|reload|force-reload}" >&2
        exit 1
        ;;
esac

exit 0
