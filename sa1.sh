#!/bin/sh
# PREFIX/lib/sa/sa1.sh
# (C) 1999-2004 Sebastien Godard (sysstat <at> wanadoo.fr)
#
umask 0022
ENDIR=PREFIX/lib/sa
cd ${ENDIR}
if [ $# = 0 ]
then
	exec ${ENDIR}/sadc -F -L 1 1 -
else
	exec ${ENDIR}/sadc -F -L $* -
fi

