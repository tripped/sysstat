# Global variables:
#
#  our configuration file
DEFAULT=/etc/default/sysstat
#  default setting, overriden in the above file
ENABLED=false
SA1_OPTIONS=""

# Activity reports every 10 minutes everyday
5-55/10 * * * * root [ -x /usr/lib/sysstat/sa1 ] && { [ -r "$DEFAULT" ] && . "$DEFAULT" ; [ "$ENABLED" = "true" ] && exec /usr/lib/sysstat/sa1 $SA1_OPTIONS 1 1 ; }

# Additional run at 23:59 to rotate the statistics file
59 23 * * * root [ -x /usr/lib/sysstat/sa1 ] && { [ -r "$DEFAULT" ] && . "$DEFAULT" ; [ "$ENABLED" = "true" ] && exec /usr/lib/sysstat/sa1 $SA1_OPTIONS 60 2 ; }
