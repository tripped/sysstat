#!/bin/sh
# PREFIX/lib/sa/sa2.sh
# (C) 1999,2000 Sebastien Godard <sebastien.godard@wanadoo.fr>
#
S_TIME_FORMAT=ISO ; export S_TIME_FORMAT
DATE=`date +%d`
RPT=SA_DIR/sar${DATE}
ENDIR=BIN_DIR
DFILE=SA_DIR/sa${DATE}
cd ${ENDIR}
${ENDIR}/sar $* -f ${DFILE} > ${RPT}
find SA_DIR \( -name 'sar*' -o -name 'sa*' \) -mtime +7 -exec rm -f {} \;

