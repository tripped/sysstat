#!/bin/sh

set -e 

if [ "x$1" = "x" ] ; then
	echo "Usage: $0 directory"
	exit 1
fi

cd "$1"
mv usr/bin/sar usr/bin/sar.sysstat
mv usr/share/man/man1/sar.1 usr/share/man/man1/sar.sysstat.1
mv usr/lib/sa/* usr/lib/sysstat
rmdir usr/lib/sa
rm -rf usr/doc

for file in usr/lib/sysstat/sa[12] usr/share/man/man*/*; do
 if grep -q 'l[oi][gb]/sa' "$file" >/dev/null 2>&1 ; then
	mv "$file" _tmp_
	sed -e 's|usr/lib/sa|usr/lib/sysstat|' \
	    -e 's|var/log/sa|var/log/sysstat|' \
		< _tmp_ > "$file"
	touch -r _tmp_	"$file"
	rm -f _tmp_
 fi
done

chmod 755 usr/lib/sysstat/sa[12]

exit 0

