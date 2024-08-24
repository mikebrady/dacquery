#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.50])
AC_INIT([dacaroni], [1.0], [4265913+mikebrady@users.noreply.github.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([dacaroni.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_CHECK_PROGS([PKGCONFIG], [pkg-config])
if test -z "$PKGCONFIG" ; then
  AC_MSG_ERROR(pkg-config is not installed.)
fi

PKG_PROG_PKG_CONFIG([0.9.0])

# Needed for git versioning
AC_CHECK_PROGS([GIT], [git])
if test -n "$GIT" && test -e "$srcdir/.git/index" ; then
  AC_DEFINE([CONFIG_USE_GIT_VERSION_STRING], 1, [Use the version string produced by running 'git describe --dirty'.])
fi
AM_CONDITIONAL([USE_GIT_VERSION], [ test -n "$GIT" && test -e "$srcdir/.git/index" ])

# Checks for libraries.
AC_CHECK_LIB([m],[exp], , AC_MSG_ERROR(maths library needed))
PKG_CHECK_MODULES([alsa], [alsa], [CFLAGS="${alsa_CFLAGS} ${CFLAGS}" LIBS="${alsa_LIBS} ${LIBS}"], AC_MSG_ERROR(alsa library needed -- libasound2-dev suggested.))

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h math.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([memset rint strchr strncasecmp strtol])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
