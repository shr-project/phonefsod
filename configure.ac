# get rid of that stupid cache mechanism
rm -f config.cache

AC_INIT(phonefsod, 0.1, http://shr-project.org/)
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(configure.ac)
AC_ISC_POSIX

AM_INIT_AUTOMAKE(1.6 dist-bzip2)
AM_CONFIG_HEADER(config.h)
AC_PROG_CC
AC_HEADER_STDC

PKG_CHECK_MODULES(GLIB,
	glib-2.0 >= 2.29.3
	shr-glib-1.0
	fso-glib-1.0
)

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

AC_OUTPUT([
Makefile
src/Makefile
data/Makefile
])

