# Makefile to build sysstat commands
# (C) 1999-2004 Sebastien GODARD (sysstat <at> wanadoo.fr)

# Version
VERSION = 5.1.2

include build/CONFIG

# Compiler to use
CC = gcc
# 'ar' command
AR = ar
# Other commands
SED = sed
CHMOD = chmod
CHOWN = chown
# Full path to prevent from using aliases
CP = /bin/cp

# Directories
ifndef PREFIX
PREFIX = /usr
endif
ifndef SA_LIB_DIR
SA_LIB_DIR = /usr/lib/sa
endif
DESTDIR = $(RPM_BUILD_ROOT)
BIN_DIR = $(PREFIX)/bin
ifndef MAN_DIR
MAN_DIR = $(PREFIX)/man
endif
MAN1_DIR = $(MAN_DIR)/man1
MAN8_DIR = $(MAN_DIR)/man8
DOC_DIR = $(PREFIX)/doc/sysstat-$(VERSION)
NLS_DIR = $(PREFIX)/share/locale

# Compiler flags
CFLAGS = -Wall -Wstrict-prototypes -pipe -g -fno-strength-reduce
LFLAGS = -L. -lsysstat -lsa
SAS_DFLAGS += -DSA_DIR=\"$(SA_DIR)\"

ifneq (,$(findstring noopt,$(DEB_BUILD_OPTIONS)))
  CFLAGS += -O0
else
  CFLAGS += -O2
endif
ifeq (,$(findstring nostrip,$(DEB_BUILD_OPTIONS)))
  LFLAGS += -s
endif


# NLS (National Language Support)
# Package name
PACKAGE = sysstat
# The msgfmt command
MSGFMT = msgfmt

ifndef IGNORE_MAN_GROUP
MANGRPARG = -g $(MAN_GROUP)
else
MANGRPARG =
endif

# Run-command directories
ifndef RC_DIR
RC_DIR = /etc/rc.d
endif
RC2_DIR = $(RC_DIR)/rc2.d
RC3_DIR = $(RC_DIR)/rc3.d
RC5_DIR = $(RC_DIR)/rc5.d
ifndef INIT_DIR
INIT_DIR = /etc/rc.d/init.d
endif
ifndef INITD_DIR
INITD_DIR = init.d
endif

all: sa1 sa2 crontab sysstat sysstat.sysconfig sysstat.crond \
	sadc sar sadf iostat mpstat locales

common.o: common.c common.h
	$(CC) -c -o $@ $(CFLAGS) $(DFLAGS) $<

sa_common.o: sa_common.c sa.h common.h
	$(CC) -c -o $@ $(CFLAGS) $(DFLAGS) $<

libsysstat.a: common.o
	$(AR) r $@ $<
	$(AR) s $@

libsa.a: sa_common.o
	$(AR) r $@ $<
	$(AR) s $@

version.h: version.in
	$(SED) s+VERSION_NUMBER+$(VERSION)+g $< > $@

sapath.h: sapath.in
	$(SED) s+ALTLOC+$(SA_LIB_DIR)+g $< > $@

sadc: sadc.c sa.h common.h version.h libsysstat.a libsa.a
	$(CC) -o $@ $(CFLAGS) $(DFLAGS) $(SAS_DFLAGS) $< $(LFLAGS)

sar: sar.c sa.h common.h version.h sapath.h libsysstat.a libsa.a
	$(CC) -o $@ $(CFLAGS) $(DFLAGS) $(SAS_DFLAGS) $< $(LFLAGS)

sadf: sadf.c sa.h common.h version.h libsysstat.a libsa.a
	$(CC) -o $@ $(CFLAGS) $(DFLAGS) $(SAS_DFLAGS) $< $(LFLAGS)

iostat: iostat.c iostat.h common.h version.h libsysstat.a
	$(CC) -o $@ $(CFLAGS) $(DFLAGS) $(IOS_DFLAGS) $< $(LFLAGS)

mpstat: mpstat.c mpstat.h common.h version.h libsysstat.a
	$(CC) -o $@ $(CFLAGS) $(DFLAGS) $< $(LFLAGS)

sa1: sa1.in
	$(SED) s+SA_LIB_DIR+$(SA_LIB_DIR)+g $< > $@
	$(CHMOD) 755 $@

sa2: sa2.in
	$(SED) -e s+BIN_DIR+$(BIN_DIR)+g -e s+SA_DIR+$(SA_DIR)+g \
		-e s+SA_LIB_DIR+$(SA_LIB_DIR)+g \
		-e s+YESTERDAY+$(YESTERDAY)+g $< > $@
	$(CHMOD) 755 $@

sysstat.sysconfig: sysstat.sysconfig.in
	$(SED) s+CONFIG_HISTORY+$(HISTORY)+g $< > $@

sysstat: sysstat.in
ifeq ($(INSTALL_CRON),y)
ifeq ($(CRON_OWNER),root)
	$(SED) -e 's+SU ++g' -e s+SA_LIB_DIR/+$(SA_LIB_DIR)/+g \
		-e 's+ QUOTE++g' -e s+INIT_DIR/+$(INIT_DIR)/+g $< > sysstat
else
	$(SED) -e 's+SU SA_LIB_DIR/+su $(CRON_OWNER) -c "$(SA_LIB_DIR)/+g' \
		-e 's+ QUOTE+"+g' -e s+INIT_DIR/+$(INIT_DIR)/+g $< > sysstat
endif
else
	$(SED) -e 's+SU ++g' -e s+SA_LIB_DIR/+$(SA_LIB_DIR)/+g \
		-e 's+ QUOTE++g' -e s+INIT_DIR/+$(INIT_DIR)/+g $< > sysstat
endif
	$(CHMOD) 755 sysstat
	
crontab: crontab.sample
	$(SED) s+SA_LIB_DIR/+$(SA_LIB_DIR)/+g $< > $@

sysstat.crond: sysstat.crond.in
	$(SED) -e s+USER+$(CRON_OWNER)+g \
		-e s+SA_LIB_DIR/+$(SA_LIB_DIR)/+g $< > $@

ifdef REQUIRE_NLS
locales: nls/af.gmo nls/de.gmo nls/es.gmo nls/fr.gmo nls/it.gmo nls/ja.gmo nls/nb_NO.gmo nls/nn_NO.gmo nls/pl.gmo nls/pt.gmo nls/ro.gmo nls/ru.gmo nls/sk.gmo
else
locales:
endif

nls/af.gmo: nls/af.po
	$(MSGFMT) -o nls/af.gmo nls/af.po

nls/de.gmo: nls/de.po
	$(MSGFMT) -o nls/de.gmo nls/de.po

nls/es.gmo: nls/es.po
	$(MSGFMT) -o nls/es.gmo nls/es.po

nls/fr.gmo: nls/fr.po
	$(MSGFMT) -o nls/fr.gmo nls/fr.po

nls/it.gmo: nls/it.po
	$(MSGFMT) -o nls/it.gmo nls/it.po

nls/ja.gmo: nls/ja.po
	$(MSGFMT) -o nls/ja.gmo nls/ja.po

nls/nb_NO.gmo: nls/nb_NO.po
	$(MSGFMT) -o nls/nb_NO.gmo nls/nb_NO.po

nls/nn_NO.gmo: nls/nn_NO.po
	$(MSGFMT) -o nls/nn_NO.gmo nls/nn_NO.po

nls/pl.gmo: nls/pl.po
	$(MSGFMT) -o nls/pl.gmo nls/pl.po

nls/pt.gmo: nls/pt.po
	$(MSGFMT) -o nls/pt.gmo nls/pt.po

nls/ro.gmo: nls/ro.po
	$(MSGFMT) -o nls/ro.gmo nls/ro.po

nls/ru.gmo: nls/ru.po
	$(MSGFMT) -o nls/ru.gmo nls/ru.po

nls/sk.gmo: nls/sk.po
	$(MSGFMT) -o nls/sk.gmo nls/sk.po

# Phony targets
.PHONY: clean distclean config install install_base install_all uninstall \
	uninstall_base uninstall_all dist bdist squeeze

uninstall_base:
	rm -f $(DESTDIR)$(SA_LIB_DIR)/sadc
	rm -f $(DESTDIR)$(MAN8_DIR)/sadc.8
	rm -f $(DESTDIR)$(SA_LIB_DIR)/sa1
	rm -f $(DESTDIR)$(MAN8_DIR)/sa1.8
	rm -f $(DESTDIR)$(SA_LIB_DIR)/sa2
	rm -f $(DESTDIR)$(MAN8_DIR)/sa2.8
	rm -f $(DESTDIR)$(BIN_DIR)/sar
	rm -f $(DESTDIR)$(MAN1_DIR)/sar.1
	rm -f $(DESTDIR)$(BIN_DIR)/sadf
	rm -f $(DESTDIR)$(MAN1_DIR)/sadf.1
	rm -f $(DESTDIR)$(BIN_DIR)/iostat
	rm -f $(DESTDIR)$(MAN1_DIR)/iostat.1
	rm -f $(DESTDIR)$(BIN_DIR)/mpstat
	rm -f $(DESTDIR)$(MAN1_DIR)/mpstat.1
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(SA_LIB_DIR)
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(SA_DIR)
#	No need to keep sysstat script, config file and links since
#	the binaries have been deleted.
	rm -f $(DESTDIR)$(INIT_DIR)/sysstat
	rm -f $(DESTDIR)/etc/sysconfig/sysstat
	rm -f $(DESTDIR)$(RC2_DIR)/S03sysstat
	rm -f $(DESTDIR)$(RC3_DIR)/S03sysstat
	rm -f $(DESTDIR)$(RC5_DIR)/S03sysstat
#	Vixie cron entries also can be safely deleted here
	rm -f $(DESTDIR)/etc/cron.d/sysstat
	rm -f $(DESTDIR)$(PREFIX)/share/locale/af/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/de/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/es/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/fr/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/it/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/ja/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/nb_NO/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/nn_NO/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/pl/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/pt/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/ro/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/ru/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/sk/LC_MESSAGES/$(PACKAGE).mo
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/af/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/de/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/es/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/fr/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/it/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/ja/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/nb_NO/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/nn_NO/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/pl/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/pt/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/ro/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/ru/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/sk/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/af
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/de
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/es
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/fr
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/it
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/ja
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/nb_NO
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/nn_NO
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/pl
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/pt
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/ro
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/ru
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/sk
	rm -f $(DESTDIR)$(DOC_DIR)/*
	-rmdir $(DESTDIR)$(DOC_DIR)
	@echo "Please ignore the errors above, if any."

# NB: Leading minus sign tells make to ignore errors...
uninstall_all: uninstall_base
	-su $(CRON_OWNER) -c "crontab -l > /tmp/crontab-$(CRON_OWNER).old"
	-$(CP) -a /tmp/crontab-$(CRON_OWNER).old ./crontab-$(CRON_OWNER).`date '+%Y%m%d.%H%M%S'`.old
	@echo "USER CRONTAB SAVED IN CURRENT DIRECTORY (WITH .old SUFFIX)."
	-su $(CRON_OWNER) -c "crontab -r"

install_base: all man/sadc.8 man/sar.1 man/sadf.1 man/sa1.8 man/sa2.8 man/iostat.1
	mkdir -p $(DESTDIR)$(SA_LIB_DIR)
	mkdir -p $(DESTDIR)$(MAN1_DIR)
	mkdir -p $(DESTDIR)$(MAN8_DIR)
	mkdir -p $(DESTDIR)$(SA_DIR)
ifeq ($(CLEAN_SA_DIR),y)
	rm -f $(DESTDIR)$(SA_DIR)/sa??
endif
	mkdir -p $(DESTDIR)$(BIN_DIR)
	mkdir -p $(DESTDIR)$(DOC_DIR)
	install -m 755 sa1 $(DESTDIR)$(SA_LIB_DIR)
	install -m 644 $(MANGRPARG) man/sa1.8 $(DESTDIR)$(MAN8_DIR)
	install -m 755 sa2 $(DESTDIR)$(SA_LIB_DIR)
	install -m 644 $(MANGRPARG) man/sa2.8 $(DESTDIR)$(MAN8_DIR)
	install -m 755 sadc $(DESTDIR)$(SA_LIB_DIR)
	install -m 644 $(MANGRPARG) man/sadc.8 $(DESTDIR)$(MAN8_DIR)
	install -m 755 sar $(DESTDIR)$(BIN_DIR)
	install -m 644 $(MANGRPARG) man/sar.1 $(DESTDIR)$(MAN1_DIR)
	install -m 755 sadf $(DESTDIR)$(BIN_DIR)
	install -m 644 $(MANGRPARG) man/sadf.1 $(DESTDIR)$(MAN1_DIR)
	install -m 755 iostat $(DESTDIR)$(BIN_DIR)
	install -m 644 $(MANGRPARG) man/iostat.1 $(DESTDIR)$(MAN1_DIR)
	install -m 755 mpstat $(DESTDIR)$(BIN_DIR)
	install -m 644 $(MANGRPARG) man/mpstat.1 $(DESTDIR)$(MAN1_DIR)
#	install -m 644 CHANGES $(DESTDIR)$(DOC_DIR)
#	install -m 644 COPYING $(DESTDIR)$(DOC_DIR)
#	install -m 644 CREDITS $(DESTDIR)$(DOC_DIR)
#	install -m 644 README  $(DESTDIR)$(DOC_DIR)
#	install -m 644 FAQ     $(DESTDIR)$(DOC_DIR)
#	install -m 644 *.lsm   $(DESTDIR)$(DOC_DIR)
ifdef REQUIRE_NLS
	mkdir -p $(DESTDIR)$(NLS_DIR)/af/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/de/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/es/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/fr/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/it/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/ja/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/nb_NO/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/nn_NO/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/pl/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/pt/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/ro/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/ru/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/sk/LC_MESSAGES
	install -m 644 nls/af.gmo $(DESTDIR)$(NLS_DIR)/af/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/de.gmo $(DESTDIR)$(NLS_DIR)/de/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/es.gmo $(DESTDIR)$(NLS_DIR)/es/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/fr.gmo $(DESTDIR)$(NLS_DIR)/fr/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/it.gmo $(DESTDIR)$(NLS_DIR)/it/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/ja.gmo $(DESTDIR)$(NLS_DIR)/ja/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/nb_NO.gmo $(DESTDIR)$(NLS_DIR)/nb_NO/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/nn_NO.gmo $(DESTDIR)$(NLS_DIR)/nn_NO/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/pl.gmo $(DESTDIR)$(NLS_DIR)/pl/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/pt.gmo $(DESTDIR)$(NLS_DIR)/pt/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/ro.gmo $(DESTDIR)$(NLS_DIR)/ro/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/ru.gmo $(DESTDIR)$(NLS_DIR)/ru/LC_MESSAGES/$(PACKAGE).mo
	install -m 644 nls/sk.gmo $(DESTDIR)$(NLS_DIR)/sk/LC_MESSAGES/$(PACKAGE).mo
endif

install_all: install_base
	$(CHOWN) $(CRON_OWNER) $(DESTDIR)$(SA_DIR)
	if [ -d /etc/cron.d ]; then \
	   install -m 644 sysstat.crond $(DESTDIR)/etc/cron.d/sysstat; \
	else \
	   su $(CRON_OWNER) -c "crontab -l > /tmp/crontab-$(CRON_OWNER).save"; \
	   $(CP) -a /tmp/crontab-$(CRON_OWNER).save ./crontab-$(CRON_OWNER).`date '+%Y%m%d.%H%M%S'`.save; \
	   echo "USER PREVIOUS CRONTAB SAVED IN CURRENT DIRECTORY (USING .save SUFFIX)."; \
	   su $(CRON_OWNER) -c "crontab crontab"; \
	fi
	if [ -d $(DESTDIR)$(INIT_DIR) ]; then \
	   install -m 755 sysstat $(DESTDIR)$(INIT_DIR)/sysstat; \
	fi
	if [ -d /etc/sysconfig ]; then \
	   install -m 644 sysstat.sysconfig $(DESTDIR)/etc/sysconfig/sysstat; \
	fi
	cd $(DESTDIR)$(RC2_DIR) && ln -sf ../$(INITD_DIR)/sysstat S03sysstat
	cd $(DESTDIR)$(RC3_DIR) && ln -sf ../$(INITD_DIR)/sysstat S03sysstat
	cd $(DESTDIR)$(RC5_DIR) && ln -sf ../$(INITD_DIR)/sysstat S03sysstat


ifeq ($(INSTALL_CRON),y)
uninstall: uninstall_all
else
uninstall: uninstall_base
endif

ifeq ($(INSTALL_CRON),y)
install: install_all
else
install: install_base
endif

clean:
	rm -f sadc sa1 sa2 sysstat sar sadf iostat mpstat *.o *.a core TAGS crontab
	rm -f sapath.h version.h sysstat.sysconfig sysstat.crond
	find nls -name "*.gmo" -exec rm -f {} \;

distclean: clean
	$(CP) build/CONFIG.def build/CONFIG
	rm -f *.save *.old .*.swp data

dist: distclean
	cd .. && (tar -cvf - sysstat-$(VERSION) | gzip -v9 > sysstat-$(VERSION).tar.gz)

bdist: distclean
	cd .. && (tar -cvf - sysstat-$(VERSION) | bzip2 > sysstat-$(VERSION).tar.bz2)

config: clean
	@sh build/Configure.sh

squeeze:
	sed 's/ *$$//g' sar.c > squeeze-file
	mv squeeze-file sar.c
	sed 's/ *$$//g' sadf.c > squeeze-file
	mv squeeze-file sadf.c
	sed 's/ *$$//g' sadc.c > squeeze-file
	mv squeeze-file sadc.c
	sed 's/ *$$//g' iostat.c > squeeze-file
	mv squeeze-file iostat.c
	sed 's/ *$$//g' mpstat.c > squeeze-file
	mv squeeze-file mpstat.c
	sed 's/ *$$//g' common.c > squeeze-file
	mv squeeze-file common.c
	sed 's/ *$$//g' sa_common.c > squeeze-file
	mv squeeze-file sa_common.c
	sed 's/ *$$//g' common.h > squeeze-file
	mv squeeze-file common.h
	sed 's/ *$$//g' iostat.h > squeeze-file
	mv squeeze-file iostat.h
	sed 's/ *$$//g' mpstat.h > squeeze-file
	mv squeeze-file mpstat.h
	sed 's/ *$$//g' sa.h > squeeze-file
	mv squeeze-file sa.h
	sed 's/ *$$//g' version.in > squeeze-file
	mv squeeze-file version.in
	sed 's/ *$$//g' sapath.in > squeeze-file
	mv squeeze-file sapath.in

tags:
	etags ./*.[hc]

