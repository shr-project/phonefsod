MAINTAINERCLEANFILES = Makefile.in

phonefsod-usage-service-glue.h: ${top_builddir}/src/dbus/phonefsod-usage.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=phonefsod_usage_service $^

EXTRA_DIST = dbus/phonefsod-usage.xml

bin_PROGRAMS = phonefsod
dbus_sources = phonefsod-usage-service-glue.h


BUILT_SOURCES = $(dbus_sources)

phonefsod_SOURCES = $(dbus_sources) \
	phonefsod.c \
	phonefsod-fso.c \
	phonefsod-fso.h \
	phonefsod-dbus.c \
	phonefsod-dbus.h \
	phonefsod-dbus-common.h \
	phonefsod-dbus-usage.c \
	phonefsod-dbus-usage.h \
	phonefsod-dbus-phoneuid.c \
	phonefsod-dbus-phoneuid.h

phonefsod_CFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DPKGDATADIR=\"$(pkgdatadir)\" \
	-DPHONEFSOD_CONFIG=\"$(sysconfdir)/phonefsod.conf\" \
	@DBUS_GLIB_CFLAGS@ \
	@DBUS_CFLAGS@ \
	@FRAMEWORKD_GLIB_CFLAGS@ \
	-ggdb

phonefsod_LDFLAGS = \
	-ldl \
	@DBUS_GLIB_LIBS@ \
	@DBUS_LIBS@ \
	@FRAMEWORKD_GLIB_LIBS@

