# Makefile to build sysstat commands
# (C) 1999-2000 Sebastien GODARD <sebastien.godard@wanadoo.fr>

# Version
VERSION = 3.3.3

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
DESTDIR = $(RPM_BUILD_ROOT)
BIN_DIR = $(PREFIX)/bin
LIB_DIR = $(PREFIX)/lib
MAN_DIR = $(PREFIX)/share/man
MAN1_DIR = $(MAN_DIR)/man1
MAN8_DIR = $(MAN_DIR)/man8
SA_DIR = /var/log/sysstat
DOC_DIR = $(PREFIX)/doc/sysstat-$(VERSION)
NLS_DIR = $(PREFIX)/share/locale

# Compiler flags
CFLAGS = -Wall -Wstrict-prototypes -pipe -O2 -fno-strength-reduce
LFLAGS = -L. -lsysstat 
SAS_DFLAGS += -DSA_DIR=\"$(SA_DIR)\"

ifneq (,$(findstring debug,$(DEB_BUILD_OPTIONS)))
  CFLAGS += -g
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

all: sadc sa1 sa2 crontab sysstat sar iostat mpstat locales

common.o: common.c common.h
	$(CC) -c -o $@ $(CFLAGS) $(DFLAGS) $<

libsysstat.a: common.o
	$(AR) r $@ $<
	$(AR) s $@

sadc: sadc.c sa.h common.h version.h libsysstat.a
	$(CC) -o $@ $(CFLAGS) $(DFLAGS) $(SAS_DFLAGS) $< $(LFLAGS)

sapath.h: sapath.in
	$(SED) s+ALTLOC+$(PREFIX)+g sapath.in > sapath.h

sa1: sa1.sh
	$(SED) -e s+PREFIX+$(PREFIX)+g -e s+SA_DIR+$(SA_DIR)+g sa1.sh > sa1
	$(CHMOD) 755 sa1

sa2: sa2.sh
	$(SED) -e s+BIN_DIR+$(BIN_DIR)+g -e s+SA_DIR+$(SA_DIR)+g \
		-e s+PREFIX+$(PREFIX)+g sa2.sh > sa2
	$(CHMOD) 755 sa2

sysstat_base: sysstat.sh
	$(SED) s+PREFIX/+$(PREFIX)/+g sysstat.sh > sysstat
	$(CHMOD) 755 sysstat

sysstat_all: sysstat.sh
ifeq ($(CRON_OWNER),root)
	$(SED) s+PREFIX/+$(PREFIX)/+g sysstat.sh > sysstat
else
	$(SED) 's+PREFIX/+su $(CRON_OWNER) -c $(PREFIX)/+g' sysstat.sh > sysstat
endif
	$(CHMOD) 755 sysstat

crontab: crontab.sample
	$(SED) s+PREFIX/+$(PREFIX)/+g crontab.sample > crontab

sar: sar.c sa.h common.h version.h sapath.h libsysstat.a
	$(CC) -o $@ $(CFLAGS) $(DFLAGS) $(SAS_DFLAGS) $< $(LFLAGS)

iostat: iostat.c iostat.h common.h version.h libsysstat.a
	$(CC) -o $@ $(CFLAGS) $(DFLAGS) $(IOS_DFLAGS) $< $(LFLAGS)

mpstat: mpstat.c mpstat.h common.h version.h libsysstat.a
	$(CC) -o $@ $(CFLAGS) $(DFLAGS) $< $(LFLAGS)

locales: nls/fr/$(PACKAGE).po nls/de/$(PACKAGE).po nls/es/$(PACKAGE).po nls/pt/$(PACKAGE).po
ifdef REQUIRE_NLS
	$(MSGFMT) -o nls/fr/$(PACKAGE).mo nls/fr/$(PACKAGE).po
	$(MSGFMT) -o nls/de/$(PACKAGE).mo nls/de/$(PACKAGE).po
	$(MSGFMT) -o nls/es/$(PACKAGE).mo nls/es/$(PACKAGE).po
	$(MSGFMT) -o nls/pt/$(PACKAGE).mo nls/pt/$(PACKAGE).po
endif

# Phony targets
.PHONY: clean distclean config install install_base install_all uninstall uninstall_base uninstall_all dist squeeze sysstat sysstat_base sysstat_all

uninstall_base:
	rm -f $(DESTDIR)$(LIB_DIR)/sa/sadc
	rm -f $(DESTDIR)$(MAN8_DIR)/sadc.8
	rm -f $(DESTDIR)$(LIB_DIR)/sa/sa1
	rm -f $(DESTDIR)$(MAN8_DIR)/sa1.8
	rm -f $(DESTDIR)$(LIB_DIR)/sa/sa2
	rm -f $(DESTDIR)$(MAN8_DIR)/sa2.8
	rm -f $(DESTDIR)$(BIN_DIR)/sar
	rm -f $(DESTDIR)$(MAN1_DIR)/sar.1
	rm -f $(DESTDIR)$(BIN_DIR)/iostat
	rm -f $(DESTDIR)$(MAN1_DIR)/iostat.1
	rm -f $(DESTDIR)$(BIN_DIR)/mpstat
	rm -f $(DESTDIR)$(MAN1_DIR)/mpstat.1
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(LIB_DIR)/sa
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(SA_DIR)
	rm -f $(DESTDIR)$(PREFIX)/share/locale/fr/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/de/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/es/LC_MESSAGES/$(PACKAGE).mo
	rm -f $(DESTDIR)$(PREFIX)/share/locale/pt/LC_MESSAGES/$(PACKAGE).mo
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/fr/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/de/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/es/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/pt/LC_MESSAGES
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/fr
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/de
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/es
	-rmdir --ignore-fail-on-non-empty $(DESTDIR)$(PREFIX)/share/locale/pt
	rm -f $(DESTDIR)$(PREFIX)/doc/sysstat-$(VERSION)/*
	-rmdir $(DESTDIR)$(PREFIX)/doc/sysstat-$(VERSION)
	@echo "Please ignore the errors above, if any."

uninstall_all: uninstall_base
	-su $(CRON_OWNER) -c "crontab -l > /tmp/crontab-$(CRON_OWNER).old"
	-$(CP) -a /tmp/crontab-$(CRON_OWNER).old ./crontab-$(CRON_OWNER).`date '+%Y%m%d.%H%M%S'`.old
	@echo "USER CRONTAB SAVED IN CURRENT DIRECTORY (WITH .old SUFFIX)."
	-su $(CRON_OWNER) -c "crontab -r"
	rm -f $(DESTDIR)$(INIT_DIR)/sysstat
	rm -f $(DESTDIR)$(RC2_DIR)/S03sysstat
	rm -f $(DESTDIR)$(RC3_DIR)/S03sysstat
	rm -f $(DESTDIR)$(RC5_DIR)/S03sysstat

install_base: all man/sadc.8 man/sar.1 man/sa1.8 man/sa2.8 man/iostat.1
	mkdir -p $(DESTDIR)$(LIB_DIR)/sa
	mkdir -p $(DESTDIR)$(MAN1_DIR)
	mkdir -p $(DESTDIR)$(MAN8_DIR)
	mkdir -p $(DESTDIR)$(SA_DIR)
	mkdir -p $(DESTDIR)$(BIN_DIR)
	mkdir -p $(DESTDIR)$(DOC_DIR)
	install -m 755 sadc $(DESTDIR)$(LIB_DIR)/sa
	install -m 644 $(MANGRPARG) man/sadc.8 $(DESTDIR)$(MAN8_DIR)
	install -m 755 sa1 $(DESTDIR)$(LIB_DIR)/sa
	install -m 644 $(MANGRPARG) man/sa1.8 $(DESTDIR)$(MAN8_DIR)
	install -m 755 sa2 $(DESTDIR)$(LIB_DIR)/sa
	install -m 644 $(MANGRPARG) man/sa2.8 $(DESTDIR)$(MAN8_DIR)
	install -m 755 sar $(DESTDIR)$(BIN_DIR)
	install -m 644 $(MANGRPARG) man/sar.1 $(DESTDIR)$(MAN1_DIR)
	install -m 755 iostat $(DESTDIR)$(BIN_DIR)
	install -m 644 $(MANGRPARG) man/iostat.1 $(DESTDIR)$(MAN1_DIR)
	install -m 755 mpstat $(DESTDIR)$(BIN_DIR)
	install -m 644 $(MANGRPARG) man/mpstat.1 $(DESTDIR)$(MAN1_DIR)
#	install -m 644 CHANGES $(DESTDIR)$(DOC_DIR)
#	install -m 644 COPYING $(DESTDIR)$(DOC_DIR)
#	install -m 644 CREDITS $(DESTDIR)$(DOC_DIR)
#	install -m 644 README  $(DESTDIR)$(DOC_DIR)
#	install -m 644 *.lsm   $(DESTDIR)$(DOC_DIR)
ifdef REQUIRE_NLS
	mkdir -p $(DESTDIR)$(NLS_DIR)/fr/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/de/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/es/LC_MESSAGES
	mkdir -p $(DESTDIR)$(NLS_DIR)/pt/LC_MESSAGES
	install -m 644 nls/fr/$(PACKAGE).mo $(DESTDIR)$(NLS_DIR)/fr/LC_MESSAGES
	install -m 644 nls/de/$(PACKAGE).mo $(DESTDIR)$(NLS_DIR)/de/LC_MESSAGES
	install -m 644 nls/es/$(PACKAGE).mo $(DESTDIR)$(NLS_DIR)/es/LC_MESSAGES
	install -m 644 nls/pt/$(PACKAGE).mo $(DESTDIR)$(NLS_DIR)/pt/LC_MESSAGES
endif

install_all: install_base
	$(CHOWN) $(CRON_OWNER) $(DESTDIR)$(SA_DIR)
	-su $(CRON_OWNER) -c "crontab -l > /tmp/crontab-$(CRON_OWNER).save"
	-$(CP) -a /tmp/crontab-$(CRON_OWNER).save ./crontab-$(CRON_OWNER).`date '+%Y%m%d.%H%M%S'`.save
	@echo "USER PREVIOUS CRONTAB SAVED IN CURRENT DIRECTORY (WITH .save SUFFIX)."
	-su $(CRON_OWNER) -c "crontab crontab"
	if [ -d $(DESTDIR)$(INIT_DIR) ]; then \
	   install -m 755 sysstat $(DESTDIR)$(INIT_DIR)/sysstat; \
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

ifeq ($(INSTALL_CRON),y)
sysstat: sysstat_all
else
sysstat: sysstat_base
endif

clean:
	rm -f sadc sa1 sa2 sysstat sar iostat mpstat *.o *.a core TAGS data crontab
	rm -f sapath.h
	find nls -name "*.mo" -exec rm -f {} \;

distclean: clean
	$(CP) build/CONFIG.def build/CONFIG
	rm -f *.save *.old .*.swp

dist: distclean
	cd .. && (tar -cvf - sysstat-$(VERSION) | gzip -v9 > sysstat-$(VERSION).tar.gz)

config:
	@sh build/Configure.sh

squeeze:
	sed 's/ *$$//g' sar.c > squeeze-file
	mv squeeze-file sar.c
	sed 's/ *$$//g' sadc.c > squeeze-file
	mv squeeze-file sadc.c
	sed 's/ *$$//g' iostat.c > squeeze-file
	mv squeeze-file iostat.c
	sed 's/ *$$//g' mpstat.c > squeeze-file
	mv squeeze-file mpstat.c
	sed 's/ *$$//g' common.c > squeeze-file
	mv squeeze-file common.c
	sed 's/ *$$//g' common.h > squeeze-file
	mv squeeze-file common.h
	sed 's/ *$$//g' iostat.h > squeeze-file
	mv squeeze-file iostat.h
	sed 's/ *$$//g' mpstat.h > squeeze-file
	mv squeeze-file mpstat.h
	sed 's/ *$$//g' sa.h > squeeze-file
	mv squeeze-file sa.h
	sed 's/ *$$//g' version.h > squeeze-file
	mv squeeze-file version.h
	sed 's/ *$$//g' sapath.in > squeeze-file
	mv squeeze-file sapath.in

tags:
	etags ./*.[hc]

