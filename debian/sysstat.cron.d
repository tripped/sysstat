# Activity reports every 10 minutes everyday.
5,15,25,35,45,55 * * * * root test -x /usr/lib/sysstat/sa1 && /usr/lib/sysstat/sa1
# Daily summary prepared at 19:05.
5 19 * * * root test -x /usr/lib/sysstat/sa2 && /usr/lib/sysstat/sa2 -A
