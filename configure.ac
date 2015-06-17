#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([geofence-server], [0.1])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_SRCDIR([geofence-server/geofence_server.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

###### Checks for programs.
m4_pattern_allow([AM_PROG_AR])
AM_PROG_AR
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_LN_S
AC_PROG_RANLIB

# PLATFORM configuration
PLATFORM_INIT

# Add default build options to CFLAGS, LDFLAGS
if test "x$GCC" = "xyes"; then
	CFLAGS="$CFLAGS -Wall -Werror"
	LDFLAGS="$LDFLAGS -Wl,-z,defs -Wl,--as-needed -Wl,--hash-style=both"
fi

# Add -g option to CFLAGS
AC_ARG_ENABLE([debug],
	[AC_HELP_STRING([--enable-debug],[turn on debugging [default=no]])],
	[case "${enableval}" in
	 yes) enable_dbg=yes ;;
	 no) enable_dbg=no ;;
	 *) AC_MSG_ERROR([Bad value ${enableval} for --enable-debug]) ;;
	 esac],[enable_dbg=no])
if ([test "x$enable_dbg" = xyes]); then
	CFLAGS="$CFLAGS -g"
fi

# Check GCC ELF visibility
AC_MSG_CHECKING(for ELF visibility)
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM([[
		__attribute__((visibility("default")))
		int var=10;
	]])],
	[has_visibility=yes
	AC_DEFINE([EXPORT_API], [__attribute__((visibility("default")))], [Symbol visibility prefix])
	CFLAGS="$CFLAGS -fvisibility=hidden"],
	[has_visibility=no
	AC_DEFINE([EXPORT_API], [], [Symbol visibility prefix])	]
)
AC_MSG_RESULT($has_visibility)

# Check required libraries
PKG_CHECK_MODULES(PROVIDERS, [glib-2.0 network tapi vconf vconf-internal-keys gthread-2.0 dlog gio-2.0 gio-unix-2.0 geofence-dbus sqlite3 db-util alarm-service deviced capi-appfw-app-manager capi-location-manager capi-network-bluetooth capi-network-wifi vconf vconf-internal-keys libcore-context-manager])
AC_SUBST(PROVIDERS_CFLAGS)
AC_SUBST(PROVIDERS_LIBS)

PKG_CHECK_MODULES(MODULE, [glib-2.0 gmodule-2.0 dlog gio-2.0 geofence-dbus capi-appfw-app-manager sqlite3 db-util])
AC_SUBST(MODULE_CFLAGS)
AC_SUBST(MODULE_LIBS)

AC_HAVE_LIBRARY(pthread, [PTHREAD_LIBS=-lpthread])

# Check DBus configuration path
DBUS_CONF_DIR="${sysconfdir}/dbus-1/system.d"
AC_SUBST(DBUS_CONF_DIR)

# Check DBus service path
DBUS_SERVICES_DIR="/usr/share/dbus-1/system-services"
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [Where services dir for DBus is])

# Config files
AC_CONFIG_FILES([
Makefile
geofence-server/Makefile
module/Makefile
])

AC_OUTPUT
