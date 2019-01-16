#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([virtualScreenGtk], [1.0], [abbati.simone@gmail.com])
AC_CONFIG_SRCDIR([src/virtualScreenGtk.c])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# pkg-config
PKG_CHECK_MODULES(GTK3, gtk+-3.0)

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([gtk/gtk.h glib.h gdk-pixbuf/gdk-pixbuf.h stdio.h fcntl.h stdlib.h unistd.h sys/time.h getopt.h errno.h sys/stat.h sys/types.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
#AC_HEADER_STDBOOL

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday memset])

AC_OUTPUT(Makefile src/Makefile)

