# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# $Id: configure.ac 110 2006-05-15 00:08:01Z lennart $

# This file is part of syrep.
#
# syrep is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# syrep is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with syrep; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.

AC_PREREQ(2.59)
AC_INIT([syrep],[0.9],[mzflerc (at) 0pointer (dot) de])
AC_CONFIG_SRCDIR([src/syrep.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign 1.9 -Wall])

# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_GNU_SOURCE

AC_SUBST(PACKAGE_URL, [http://0pointer.de/lennart/projects/syrep/])

# GCC flags

test_gcc_flag() {
    AC_LANG_CONFTEST([int main() {}])
    $CC -c conftest.c $CFLAGS $@ > /dev/null 2> /dev/null
    ret=$?
    rm -f conftest.o
    return $ret
}

# If using GCC specify some additional parameters
if test "x$GCC" = "xyes" ; then

    # We use gnu99 instead of c99 because many have interpreted the standard
    # in a way that int64_t isn't defined on non-64 bit platforms.
    DESIRED_FLAGS="-std=gnu99 -Wall -W -Wextra -pedantic -pipe -Wformat -Wold-style-definition -Wdeclaration-after-statement -Wfloat-equal -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wmissing-noreturn -Wshadow -Wendif-labels -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Winline -Wno-unused-parameter"

    for flag in $DESIRED_FLAGS ; do
        AC_MSG_CHECKING([whether $CC accepts $flag])
        if test_gcc_flag $flag ; then 
           CFLAGS="$CFLAGS $flag"
           AC_MSG_RESULT([yes])
        else
           AC_MSG_RESULT([no])
        fi
    done 
fi

if type -p stow > /dev/null && test -d /usr/local/stow ; then
   AC_MSG_NOTICE([*** Found /usr/local/stow: default install prefix set to /usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION} ***])
   ac_default_prefix="/usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION}"
fi

# Check for zlib
AC_CHECK_LIB([z], [inflate],, [AC_MSG_ERROR([*** Sorry, you have to install zlib ***])])
AC_CHECK_HEADER([zlib.h],, [AC_MSG_ERROR([*** Sorry, you have to install the zlib headers ***])])

# Check for Berkeley DB (needs to be improved)
AC_CHECK_HEADER([db.h],, [AC_MSG_ERROR([*** Sorry, you have to install the Berkeley Database Library (libdb) 4.3 or newer ***])])
LIBS="$LIBS -ldb"

# Checking libdb version number
AC_MSG_CHECKING([for Berkeley libdb 4.3])
AC_LANG_PUSH(C)
AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
#include <db.h>
#if (DB_VERSION_MAJOR != 4) || (DB_VERSION_MINOR != 3 && DB_VERSION_MINOR != 4)
#error "foo"
#endif
]])],
[AC_MSG_RESULT([yes])],
[AC_MSG_FAILURE([Berkeley libdb 4.3 required])])
AC_LANG_POP(C)

# Check for Linux sendfile()
AC_CHECK_HEADER([sys/sendfile.h], sendfile=yes, sendfile=no)

if test "x$sendfile" = xyes ; then
    AC_CHECK_LIB([c], [sendfile], sendfile=yes, sendfile=no)
fi

if test "x$sendfile" = xyes ; then
    AC_DEFINE([USE_SENDFILE], [1], [Use sendfile()])
fi

AM_CONDITIONAL([USE_SENDFILE], [test "x$sendfile" = xyes])

AC_C_BIGENDIAN
if test "x$ac_cv_c_bigendian" = "xyes"; then
   AC_DEFINE([ARCH_IS_BIG_ENDIAN], [1], [Big Endian machine])
else
   AC_DEFINE([ARCH_IS_BIG_ENDIAN], [0], [Big Endian machine])
fi

# Check for XATTR
AC_CHECK_HEADER([attr/xattr.h], xattr=yes, xattr=no)

if test "x$xattr" = xyes ; then
   AC_CHECK_LIB([attr], [getxattr], xattr=yes, xattr=no)
fi

if test "x$xattr" = xyes ; then
   AC_DEFINE([USE_XATTR], [1], [Use extended attributes])
   LIBS="-lattr $LIBS"
fi

# 64 Bit LFS support
AC_SYS_LARGEFILE

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h inttypes.h limits.h malloc.h stdlib.h string.h unistd.h libintl.h sys/time.h])

# Checks for header files.
AC_C_CONST
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_MMAP
AC_CHECK_FUNCS([atexit ftruncate getcwd gethostname memset mkdir munmap rmdir setenv strchr strdup strerror strrchr strtol putenv madvise])
#AC_FUNC_LSTAT
#AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
#AC_FUNC_MALLOC
#AC_FUNC_MEMCMP
#AC_FUNC_REALLOC
#AC_FUNC_STAT

adl_FUNC_GETOPT_LONG
lp_FUNC_MKDTEMP

# LYNX documentation generation
AC_ARG_ENABLE(lynx,
        AS_HELP_STRING(--disable-lynx,Turn off lynx usage for documentation generation),
[case "${enableval}" in
  yes) lynx=yes ;;
  no)  lynx=no ;;
  *) AC_MSG_ERROR(bad value ${enableval} for --disable-lynx) ;;
esac],[lynx=yes])

if test x$lynx = xyes ; then
   AC_CHECK_PROG(have_lynx, lynx, yes, no)

   if test x$have_lynx = xno ; then
     AC_MSG_ERROR([*** Sorry, you have to install lynx or use --disable-lynx ***])
   fi
fi

AM_CONDITIONAL([USE_LYNX], [test "x$lynx" = xyes])

# XMLTOMAN manpage generation
AC_ARG_ENABLE(xmltoman,
        AS_HELP_STRING(--disable-xmltoman,Disable rebuilding of man pages with xmltoman),
[case "${enableval}" in
  yes) xmltoman=yes ;;
  no)  xmltoman=no ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --disable-xmltoman]) ;;
esac],[xmltoman=yes])

if test x$xmltoman = xyes ; then
   AC_CHECK_PROG(have_xmltoman, xmltoman, yes, no)

   if test x$have_xmltoman = xno ; then
     AC_MSG_WARN([*** Not rebuilding man pages as xmltoman is not found ***])
     xmltoman=no
   fi
fi

AM_CONDITIONAL([USE_XMLTOMAN], [test "x$xmltoman" = xyes])

# gengetopt command line parser generation
AC_ARG_ENABLE(gengetopt,
        AS_HELP_STRING(--disable-gengetopt,Disable rebuilding of command line parser with gengetopt),
[case "${enableval}" in
  yes) gengetopt=yes ;;
  no)  gengetopt=no ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --disable-gengetopt]) ;;
esac],[gengetopt=yes])

if test x$gengetopt = xyes ; then
   AC_CHECK_PROG(have_gengetopt, gengetopt, yes, no)

   if test x$have_gengetopt = xno ; then
     AC_MSG_WARN([*** Not rebuilding command line parser as gengetopt is not found ***])
     gengetopt=no
   fi
fi

AM_CONDITIONAL([USE_GENGETOPT], [test "x$gengetopt" = xyes])

# subversion release number generation
AC_ARG_ENABLE(subversion,
        AS_HELP_STRING(--disable-subversion,Disable rebuilding of svn-release.h),
[case "${enableval}" in
  yes) subversion=yes ;;
  no)  subversion=no ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --disable-subversion]) ;;
esac],[subversion=yes])

if test x$subversion = xyes ; then
   AC_CHECK_PROG(have_subversion, svn, yes, no)

   if test x$have_subversion = xno ; then
     AC_MSG_WARN([*** Not rebuilding svn-release.h as subversion is not found ***])
     subversion=no
   fi
fi

AM_CONDITIONAL([USE_SUBVERSION], [test "x$subversion" = xyes])

AC_CONFIG_FILES([src/Makefile Makefile doc/Makefile man/Makefile doc/README.html])
AC_OUTPUT
