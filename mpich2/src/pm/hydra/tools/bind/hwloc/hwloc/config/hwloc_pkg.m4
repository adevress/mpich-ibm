# Copyright (c) 2010 Cisco Systems, Inc.  All rights reserved.
#
# hwloc modification to the following PKG_* macros -- add HWLOC_
# prefix to make it "safe" to embed these macros in other packages.
# Originally copied from the pkg-config package; see copyright and
# license below.

# pkg.m4 - Macros to locate and utilise pkg-config.            -*- Autoconf -*-
# 
# Copyright © 2004 Scott James Remnant <scott@netsplit.com>.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# As a special exception to the GNU General Public License, if you
# distribute this file as part of a program that contains a
# configuration script generated by Autoconf, you may include it under
# the same distribution terms that you use for the rest of that program.

# HWLOC_PKG_PROG_PKG_CONFIG([MIN-VERSION])
# ----------------------------------
AC_DEFUN([HWLOC_PKG_PROG_PKG_CONFIG],
[m4_pattern_forbid([^_?PKG_[A-Z_]+$])
m4_pattern_allow([^HWLOC_PKG_CONFIG(_PATH)?$])
AC_ARG_VAR([HWLOC_PKG_CONFIG], [path to pkg-config utility])dnl
if test "x$ac_cv_env_HWLOC_PKG_CONFIG_set" != "xset"; then
	AC_PATH_TOOL([HWLOC_PKG_CONFIG], [pkg-config])
fi
if test -n "$HWLOC_PKG_CONFIG"; then
	HWLOC_pkg_min_version=m4_default([$1], [0.9.0])
	AC_MSG_CHECKING([pkg-config is at least version $HWLOC_pkg_min_version])
	if $HWLOC_PKG_CONFIG --atleast-pkgconfig-version $HWLOC_pkg_min_version; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		HWLOC_PKG_CONFIG=""
	fi
		
fi[]dnl
])# HWLOC_PKG_PROG_PKG_CONFIG

# HWLOC_PKG_CHECK_EXISTS(MODULES, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
#
# Check to see whether a particular set of modules exists.  Similar
# to HWLOC_PKG_CHECK_MODULES(), but does not set variables or print errors.
#
#
# Similar to HWLOC_PKG_CHECK_MODULES, make sure that the first instance of
# this or HWLOC_PKG_CHECK_MODULES is called, or make sure to call
# HWLOC_PKG_CHECK_EXISTS manually
# --------------------------------------------------------------
AC_DEFUN([HWLOC_PKG_CHECK_EXISTS],
[AC_REQUIRE([HWLOC_PKG_PROG_PKG_CONFIG])dnl
if test -n "$HWLOC_PKG_CONFIG" && \
    AC_RUN_LOG([$HWLOC_PKG_CONFIG --exists --silence-errors "$1"]); then
    m4_ifval([$2], [$2], [:])
    m4_ifvaln([$3], [else
                     $3])dnl
fi])


# _HWLOC_PKG_CONFIG([VARIABLE], [COMMAND], [MODULES])
# ---------------------------------------------
m4_define([_HWLOC_PKG_CONFIG],
[if test -n "$HWLOC_PKG_CONFIG"; then
    if test -n "$$1"; then
        HWLOC_pkg_cv_[]$1="$$1"
    else
        HWLOC_PKG_CHECK_EXISTS([$3],
                         [HWLOC_pkg_cv_[]$1=`$HWLOC_PKG_CONFIG --[]$2 "$3" 2>/dev/null`],
			 [HWLOC_pkg_failed=yes])
    fi
else
	HWLOC_pkg_failed=untried
fi[]
])# _HWLOC_PKG_CONFIG

# _HWLOC_PKG_SHORT_ERRORS_SUPPORTED
# -----------------------------
AC_DEFUN([_HWLOC_PKG_SHORT_ERRORS_SUPPORTED],
[AC_REQUIRE([HWLOC_PKG_PROG_PKG_CONFIG])
if $HWLOC_PKG_CONFIG --atleast-pkgconfig-version 0.20; then
        HWLOC_pkg_short_errors_supported=yes
else
        HWLOC_pkg_short_errors_supported=no
fi[]dnl
])# _HWLOC_PKG_SHORT_ERRORS_SUPPORTED


# HWLOC_PKG_CHECK_MODULES(VARIABLE-PREFIX, MODULES, [ACTION-IF-FOUND],
# [ACTION-IF-NOT-FOUND])
#
#
# Note that if there is a possibility the first call to
# HWLOC_PKG_CHECK_MODULES might not happen, you should be sure to include an
# explicit call to HWLOC_PKG_PROG_PKG_CONFIG in your configure.ac
#
#
# --------------------------------------------------------------
AC_DEFUN([HWLOC_PKG_CHECK_MODULES],[
    AC_REQUIRE([HWLOC_PKG_PROG_PKG_CONFIG])dnl
    AC_ARG_VAR([HWLOC_]$1[_CFLAGS], [C compiler flags for $1, overriding pkg-config])dnl
    AC_ARG_VAR([HWLOC_]$1[_LIBS], [linker flags for $1, overriding pkg-config])dnl

    HWLOC_pkg_failed=no
    AC_MSG_CHECKING([for $1])

    _HWLOC_PKG_CONFIG([HWLOC_][$1][_CFLAGS], [cflags], [$2])
    _HWLOC_PKG_CONFIG([HWLOC_][$1][_LIBS], [libs], [$2])

    m4_define([_HWLOC_PKG_TEXT], [Alternatively, you may set the environment variables HWLOC_[]$1[]_CFLAGS
and HWLOC_[]$1[]_LIBS to avoid the need to call pkg-config.
See the pkg-config man page for more details.])

    # Check for failure of pkg-config
    if test $HWLOC_pkg_failed = yes; then
        _HWLOC_PKG_SHORT_ERRORS_SUPPORTED
        if test $HWLOC_pkg_short_errors_supported = yes; then
            HWLOC_[]$1[]_PKG_ERRORS=`$HWLOC_PKG_CONFIG --short-errors --errors-to-stdout --print-errors "$2" 2>&1`
        else 
            HWLOC_[]$1[]_PKG_ERRORS=`$HWLOC_PKG_CONFIG --errors-to-stdout --print-errors "$2" 2>&1`
        fi
        # Put the nasty error message in config.log where it belongs
	echo "$HWLOC_[]$1[]_PKG_ERRORS" >&AS_MESSAGE_LOG_FD

	ifelse([$5], , [AC_MSG_ERROR(dnl
[Package requirements ($2) were not met:

$HWLOCC_$1_PKG_ERRORS

Consider adjusting the HWLOC_PKG_CONFIG_PATH environment variable if you
installed software in a non-standard prefix.

_HWLOC_PKG_TEXT
])],
		[AC_MSG_RESULT([no])
                $5])
    elif test $HWLOC_pkg_failed = untried; then
        ifelse([$5], , [AC_MSG_FAILURE(dnl
[The pkg-config script could not be found or is too old.  Make sure it
is in your PATH or set the HWLOC_PKG_CONFIG environment variable to the full
path to pkg-config.

_HWLOC_PKG_TEXT

To get pkg-config, see <http://pkg-config.freedesktop.org/>.])],
		[$5])
    else
        AC_MSG_RESULT([yes])

        # If we got good results from pkg-config, check that they
        # actually work (i.e., that we can link against the resulting
        # $LIBS).  The canonical example why we do this is if
        # pkg-config returns 64 bit libraries but ./configure was run
        # with CFLAGS=-m32 LDFLAGS=-m32.  pkg-config gave us valid
        # results, but we'll fail if we try to link.  So detect that
        # failure now.
        hwloc_cflags_save=$CFLAGS
        hwloc_libs_save=$LIBS
        CFLAGS="$CFLAGS $HWLOC_pkg_cv_HWLOC_[]$1[]_CFLAGS"
        LIBS="$LIBS $HWLOC_pkg_cv_HWLOC_[]$1[]_LIBS"
        AC_CHECK_FUNC([$3], [hwloc_result=yes], [hwloc_result=no])
        CFLAGS=$hwloc_cflags_save
        LIBS=$hwloc_libs_save

        AC_MSG_CHECKING([for final $1 support])
        AS_IF([test "$hwloc_result" = "yes"],
              [HWLOC_[]$1[]_CFLAGS=$HWLOC_pkg_cv_HWLOC_[]$1[]_CFLAGS
               HWLOC_[]$1[]_LIBS=$HWLOC_pkg_cv_HWLOC_[]$1[]_LIBS
               AC_MSG_RESULT([yes])            
               ifelse([$4], , :, [$4])],
              [AC_MSG_RESULT([no])
               ifelse([$5], , :, [$5])])
    fi[]dnl
])# HWLOC_PKG_CHECK_MODULES
