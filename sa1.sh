#!/bin/sh
# PREFIX/lib/sa/sa1.sh
# (C) 1999-2001 Sebastien Godard <sebastien.godard@wanadoo.fr>
#
DATE=`date +%d`
ENDIR=PREFIX/lib/sa
DFILE=SA_DIR/sa${DATE}
cd ${ENDIR}
if [ $# = 0 ]
then
	exec ${ENDIR}/sadc 1 1 ${DFILE}
else
	exec ${ENDIR}/sadc $* ${DFILE}
fi

