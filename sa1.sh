#!/bin/sh
# PREFIX/lib/sa/sa1.sh
# (C) 1999-2003 Sebastien Godard <sebastien.godard@wanadoo.fr>
#
umask 0022
DATE=`date +%d`
ENDIR=PREFIX/lib/sa
DFILE=SA_DIR/sa${DATE}
cd ${ENDIR}
if [ $# = 0 ]
then
	exec ${ENDIR}/sadc -F 1 1 ${DFILE}
else
	exec ${ENDIR}/sadc -F $* ${DFILE}
fi

