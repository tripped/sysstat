#!/bin/sh
# $Id: fix.sh,v 1.6 2002-11-17 20:42:21 robert Exp $

set -e 

if [ "X$1" = "Xsysstat" ] ; then
	FFILES="usr/lib/sysstat/sa[12]
		usr/share/man/man*/*
		usr/share/doc/sysstat/FAQ
		usr/share/doc/sysstat/examples/*"
elif [ "X$1" = "Xisag" ] ; then
	FFILES="usr/share/man/man*/*" 
else
	echo "Usage: $0 [ sysstat | isag ]" 1>&2
	exit 1
fi

dir="debian/$1"

if [ ! -d "$dir" ] ; then
	echo "Directory $dir does not exists!" 1>&2
	exit 1
fi

cd "$dir"

for file in `echo $FFILES`; do
 if grep -q 'l[oi][gb]/sa' "$file" >/dev/null 2>&1 ; then
	echo  " + processing file: $dir/$file"
	mv "$file" _tmp_
	sed -e 's|usr/lib/sa|usr/lib/sysstat|g' \
	    -e 's|var/log/sa|var/log/sysstat|g' \
	    -e 's|usr/local/lib/sa|usr/local/lib/sysstat|g' \
		< _tmp_ > "$file"
	chmod --reference=_tmp_ "$file" 
	touch -r _tmp_	"$file"
	rm -f _tmp_
 fi
done

exit 0

