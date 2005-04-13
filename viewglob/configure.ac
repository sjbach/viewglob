dnl Process this file with autoconf to produce a configure script.
AC_INIT([viewglob], [1.1.1])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([config.h])
RELEASE_DATE="March 17, 2005"

AC_DEFINE_UNQUOTED([VG_RELEASE_DATE], ["$RELEASE_DATE"] , [Date of release])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_HEADER_TIOCGWINSZ
AC_CHECK_HEADERS([fcntl.h errno.h stdlib.h string.h sys/ioctl.h termios.h unistd.h stdlib.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_MODE_T
AC_TYPE_PID_T

dnl Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_PROG_GCC_TRADITIONAL
AC_CHECK_FUNCS([dup2 getcwd memchr memmove memset mkfifo pathconf select strchr grantpt unlockpt setsid setpgid setpgrp isastream])

dnl Seer now uses some Xlib for focusing the display.
AC_PATH_XTRA

dnl The display requires GTK+ 2.4.0.
AM_PATH_GTK_2_0(2.4.0)
AM_PATH_GLIB_2_0(2.2.0)

dnl Test for bash.
AC_PATH_PROG(BASH_FULL_PATH, bash, nope)
if test "x$BASH_FULL_PATH" = xnope; then
	AC_MSG_ERROR([viewglob requires bash])
fi
AC_SUBST(BASH_FULL_PATH)

dnl Test for sed.
AC_PATH_PROG(SED_FULL_PATH, sed, nope)
if test "x$SED_FULL_PATH" = xnope; then
	AC_MSG_ERROR([viewglob requires sed])
fi

dnl Output release info so the shell scripts can get 'em.
AC_SUBST(PACKAGE_NAME)
AC_SUBST(PACKAGE_VERSION)
AC_SUBST(RELEASE_DATE)


dnl =====================================================================
dnl The following is for pty stuff, and is taken from rxvt 2.7.10.
dnl =====================================================================

AC_CACHE_CHECK(for getpt, rxvt_cv_func_getpt,
[AC_TRY_LINK([#define _GNU_SOURCE
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif],
  [(void)getpt();],
  rxvt_cv_func_getpt=yes, rxvt_cv_func_getpt=no)])
if test x$rxvt_cv_func_getpt = xyes; then
  AC_DEFINE(HAVE_GETPT, 1, Define if you have _GNU_SOURCE getpt() )
fi

dnl# if we don't guess right then it's up to the user
AC_CACHE_CHECK(for pty/tty type, rxvt_cv_ptys,
[if test x$ac_cv_func_openpty = xyes; then
    rxvt_cv_ptys=OPENPTY
 else if test x$ac_cv_func__getpty = xyes; then
    rxvt_cv_ptys=SGI4
 else if test -c /dev/ttyp20; then
    rxvt_cv_ptys=SCO
 else if test -c /dev/ptym/clone; then
    rxvt_cv_ptys=HPUX
 else if test x$rxvt_cv_func_getpt = xyes; then
    rxvt_cv_ptys=GLIBC
 else if test -c /dev/ptc -a -c /dev/pts; then
    rxvt_cv_ptys=PTC
 else if test -c /dev/ptc -a -d /dev/pts; then
    rxvt_cv_ptys=PTC
 else if test -c /dev/ptmx -a -c /dev/pts/0; then
    rxvt_cv_ptys=STREAMS
 else if test x$ac_cv_func_grantpt = xyes && test x$ac_cv_func_unlockpt = xyes; then
dnl# catch CYGWIN
    rxvt_cv_ptys=STREAMS
 else
    rxvt_cv_ptys=BSD
fi
fi
fi
fi
fi
fi
fi
fi
fi
])

if test x$rxvt_cv_ptys = xOPENPTY; then
  AC_DEFINE(PTYS_ARE_OPENPTY, 1, Define for this pty type)
fi
if test x$rxvt_cv_ptys = xSCO; then
  AC_DEFINE(PTYS_ARE_NUMERIC, 1, Define for this pty type)
fi
if test x$rxvt_cv_ptys = xSTREAMS; then
  AC_DEFINE(PTYS_ARE_PTMX, 1, Define for this pty type)
fi
if test x$rxvt_cv_ptys = xPTC; then
  AC_DEFINE(PTYS_ARE_PTC, 1, Define for this pty type)
fi
if test x$rxvt_cv_ptys = xSGI4; then
  AC_DEFINE(PTYS_ARE__GETPTY, 1, Define for this pty type)
fi
if test x$rxvt_cv_ptys = xCONVEX; then
  AC_DEFINE(PTYS_ARE_GETPTY, 1, Define for this pty type)
fi
if test x$rxvt_cv_ptys = xGLIBC; then
  AC_DEFINE(PTYS_ARE_GETPT, 1, Define for this pty type)
fi
if test x$rxvt_cv_ptys = xHPUX; then
  AC_DEFINE(PTYS_ARE_CLONE, 1, Define for this pty type)
fi
if test x$rxvt_cv_ptys = xBSD -o x$rxvt_cv_ptys = xHPUX -o x$rxvt_cv_ptys = xGLIBC; then
  AC_DEFINE(PTYS_ARE_SEARCHED, 1, Define for this pty type)
fi

AC_CHECKING(for pty ranges)
ptys=`echo /dev/pty??`
pch1=`echo $ptys | tr ' ' '\012' | sed -e 's/^.*\(.\).$/\1/g' | sort -u | tr -d '\012'`
pch2=`echo $ptys | tr ' ' '\012' | sed -e 's/^.*\(.\)$/\1/g' | sort -u | tr -d '\012'`
if test x$pch1 != x; then
  AC_DEFINE_UNQUOTED(PTYCHAR1, "$pch1", Define for first char in devptyXX)
fi
if test x$pch2 != x; then
  AC_DEFINE_UNQUOTED(PTYCHAR2, "$pch2", Define for second char in devptyXX)
fi

dnl =====================================================================
dnl End of pty stuff.
dnl =====================================================================


AC_CONFIG_FILES([Makefile vgseer/Makefile vgd/Makefile vgdisplay/Makefile vgexpand/Makefile vgping/Makefile shell/Makefile common/Makefile])
AC_OUTPUT

