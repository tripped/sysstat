# Global variables:
#
#  our configuration file
DEFAULT=/etc/default/sysstat
#  default setting, overriden in the above file
ENABLED=false

# Activity reports every 10 minutes everyday
5-55/10 * * * * root [ -x /usr/lib/sysstat/sa1 ] && { [ -r "$DEFAULT" ] && . "$DEFAULT" ; [ "$ENABLED" = "true" ] && exec /usr/lib/sysstat/sa1; }

# Daily summary prepared at 19:05.
5 19 * * * root [ -x /usr/lib/sysstat/sa2 ] && { [ -r "$DEFAULT" ] && . "$DEFAULT" ; [ "$ENABLED" = "true" ]  && exec /usr/lib/sysstat/sa2; }
