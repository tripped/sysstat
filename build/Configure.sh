#!/bin/sh
#

ASK="sh build/Ask.sh"

echo
echo You can enter a ? to display a help message at any time...
echo

PREFIX=`${ASK} 'Installation directory:' "/usr/local" "prefix"`
if [ ! -d ${PREFIX} ]; then
	echo "WARNING: Directory ${PREFIX} not found: Using default (/usr/local)."
	PREFIX=/usr/local
fi

NLS=`${ASK} 'Enable National Language Support (NLS)?' "y" "nls"`

SMPRACE=`${ASK} 'Linux SMP race in serial driver workaround?' "n" "smp-race"`

IOPATCH=`${ASK} 'Support for improved disk accounting? [EXPERIMENTAL]' "n" "iostat-patch"`
grep ^disk_pgin /proc/stat >/dev/null 2>&1
if [ $? -eq 1 -a "${IOPATCH}" = "y" ];
then
	echo "WARNING: kernel patch not applied!"
fi

grep ^man: /etc/group >/dev/null 2>&1
if [ $? -eq 1 ];
then
	GRP=root
else
	GRP=man
fi
MAN=`${ASK} 'Group for manual pages:' "${GRP}" "man-group"`
grep ^${MAN}: /etc/group >/dev/null 2>&1
if [ $? -eq 1 ];
then
	echo WARNING: Group ${MAN} not found: Using ${GRP} instead.
	MAN=${GRP}
fi

if [ -d /sbin/init.d ];
then
	RC_DIR=/sbin/init.d
	INIT_DIR=/sbin/init.d
	INITD_DIR=.
elif [ -d /etc/init.d ];
then
	RC_DIR=/etc
	INIT_DIR=/etc/init.d
	INITD_DIR=init.d
else
	RC_DIR=/etc/rc.d
	INIT_DIR=/etc/rc.d/init.d
	INITD_DIR=init.d
fi
grep ^adm: /etc/passwd >/dev/null 2>&1
if [ $? -eq 1 ];
then
	USR=root
else
	USR=adm
fi
CRON_OWNER=${USR}
CRON=`${ASK} 'Set crontab to start sar automatically?' "n" "start-crontab"`
if [ "${CRON}" = "y" ];
then
	CRON_OWNER=`${ASK} 'Crontab owner (his crontab will be saved in current directory)' "${USR}" "crontab-owner"`

	grep ^${CRON_OWNER}: /etc/passwd >/dev/null 2>&1
	if [ $? -eq 1 ];
	then
		echo WARNING: User ${CRON_OWNER} not found: Using ${USR} instead.
	CRON_OWNER=${USR}
	fi
fi

echo -n Creating CONFIG file now... 

sed <build/CONFIG.in >build/CONFIG \
	-e "s+^\\(PREFIX =\\)\$+\\1 ${PREFIX}+" \
	-e "s+^\\(ENABLE_NLS =\\)\$+\\1 ${NLS}+" \
	-e "s+^\\(ENABLE_PATCH =\\)\$+\\1 ${IOPATCH}+" \
	-e "s+^\\(ENABLE_SMP_WRKARD =\\)\$+\\1 ${SMPRACE}+" \
	-e "s+^\\(MAN_GROUP =\\)\$+\\1 ${MAN}+" \
	-e "s+^\\(RC_DIR =\\)\$+\\1 ${RC_DIR}+" \
	-e "s+^\\(INIT_DIR =\\)\$+\\1 ${INIT_DIR}+" \
	-e "s+^\\(INITD_DIR =\\)\$+\\1 ${INITD_DIR}+" \
	-e "s+^\\(CRON_OWNER =\\)\$+\\1 ${CRON_OWNER}+" \
	-e "s+^\\(INSTALL_CRON =\\)\$+\\1 ${CRON}+"
echo " Done."

echo
echo 'Now enter "make" to build sysstat commands.'
if [ "${CRON}" = "y" ];
then
	echo 'Then edit the crontab file created in current directory ("vi crontab")'
fi
echo 'The last step is to log in as root and enter "make install"'
echo 'to perform installation process.'

