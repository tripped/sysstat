#! /bin/sh
# vim:ft=sh:et
### BEGIN INIT INFO
# Provides:          sysstat
# Required-Start:    $local_fs $syslog
# Required-Stop:    
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start/stop sysstat's sadc
# Description:       Sysstat contains system performance tools for Linux
#                    The init file runs the sadc command in order to write
#                    the "LINUX RESTART" mark to the daily data file
### END INIT INFO

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
status=0

. /lib/lsb/init-functions

case "$1" in
  start|restart|reload|force-reload)
        if [ "$ENABLED" = "true" ] ; then
                log_daemon_msg "Starting $DESC" "$NAME"
                start-stop-daemon --start --quiet --exec $DAEMON -- $OPTIONS || status=$?
                log_end_msg $status
        else
                log_warning_msg "$NAME not enabled in ${DEFAULT}, not starting."
        fi
        ;;
  stop)
        ;;

  *)
        log_failure_msg "Usage: $0 {start|stop|restart|reload|force-reload}"
        exit 1
        ;;
esac

exit $status
