MAINTAINERCLEANFILES = Makefile.in

bin_PROGRAMS = phonefsod

phonefsod_SOURCES = \
	phonefsod.c \
	phonefsod-fso.c \
	phonefsod-fso.h \
	phonefsod-dbus.c \
	phonefsod-dbus.h \
	phonefsod-dbus-common.h


phonefsod_CFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DPKGDATADIR=\"$(pkgdatadir)\" \
	-DPHONEFSOD_CONFIG=\"$(sysconfdir)/phonefsod.conf\" \
	-DG_LOG_DOMAIN=\"phonefsod\" \
	@GLIB_CFLAGS@ \
	-ggdb

phonefsod_LDFLAGS = @GLIB_LIBS@

