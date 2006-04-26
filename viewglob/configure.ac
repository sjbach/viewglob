dnl Process this file with autoconf to produce a configure script.
AC_INIT([viewglob], [2.0.3])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([config.h])
RELEASE_DATE="July 18, 2005"

AC_DEFINE_UNQUOTED([VG_RELEASE_DATE], ["$RELEASE_DATE"] , [Date of release])

CFLAGS="$CFLAGS -Wall"
if test x"$USE_MAINTAINER_MODE" = xyes; then
	CFLAGS="$CFLAGS -g3"
fi

dnl Optionally build only vgseer (and the other programs it needs).
VGSEER_ONLY=
AC_ARG_ENABLE(vgseer-only,
	[AC_HELP_STRING(
		[--enable-vgseer-only],
		[Only build and install the client side of Viewglob])],
	[VGSEER_ONLY=yep], [VGSEER_ONLY=nope])
AM_CONDITIONAL([FULL_BUILD], [test "$VGSEER_ONLY" = nope])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_HEADER_TIOCGWINSZ
AC_HEADER_TIME
AC_CHECK_HEADERS([sys/time.h time.h sys/select.h])
AC_CHECK_HEADERS([ \
	fnmatch.h  sys/un.h \
	fcntl.h    errno.h       stdlib.h      \
	string.h   sys/ioctl.h   termios.h     \
	unistd.h   stdlib.h      arpa/inet.h   \
	netdb.h    netinet/in.h  sys/socket.h],,
	AC_MSG_ERROR([Required header missing!]))

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_CHECK_TYPES([ptrdiff_t],,,)

dnl Checks for functions.
AC_FUNC_CLOSEDIR_VOID
AC_PROG_GCC_TRADITIONAL
AC_CHECK_FUNCS([ \
 	stat     lstat \
	dup2     getcwd    memmove    \
	memset   pathconf  select     \
	strchr   setsid    socket     \
	bind     listen    accept     \
	connect  setsockopt],,
	AC_MSG_ERROR([Required function missing!]))

dnl Determine whether to use getaddrinfo or gethostbyname and inet_aton.
AC_CHECK_FUNCS([getaddrinfo],,
	[AC_CHECK_FUNCS([gethostbyname inet_aton],,
		[AC_MSG_ERROR([Socket interface is incomplete!])])])

dnl Determine necessary libraries for sockets.
LIB_SOCKET_NSL
TYPE_SOCKLEN_T

AM_PATH_GLIB_2_0(2.2.0,,
	AC_MSG_ERROR(GLib 2.2.0+ is required to build Viewglob))

dnl Test for bash.
AC_PATH_PROG(BASH_FULL_PATH, bash, nope)
if test "x$BASH_FULL_PATH" = xnope; then
	AC_MSG_ERROR([Viewglob requires bash])
fi
AC_SUBST(BASH_FULL_PATH)

dnl Test for sed.
AC_CHECK_PROGS(SED,[sed],no)
if test  "$SED" = "no"; then
	AC_MSG_ERROR([Viewglob installation requires sed])
fi

dnl =====================================================================
dnl The following is for pty stuff and is taken from rxvt 2.7.10.
dnl =====================================================================

AC_CHECK_FUNCS([setpgid setpgrp isastream getpt grantpt unlockpt])
AC_FUNC_GETPGRP
AC_FUNC_SETPGRP

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

dnl Output release stuff so the shell scripts can get to it.
AC_SUBST(PACKAGE_NAME)
AC_SUBST(PACKAGE_VERSION)
AC_SUBST(RELEASE_DATE)

if test "$VGSEER_ONLY" = nope; then

	dnl This stuff is specific to vgd and the displays.

	AC_CHECK_HEADERS([syslog.h],,
		AC_MSG_ERROR([Required header missing!]))
	AC_CHECK_FUNCS([strtoul],,
		AC_MSG_ERROR([Required function missing!]))

	dnl For advanced Xlib stuff
	AC_PATH_XTRA

	dnl The displays require GTK+ 2.4.0.
	AM_PATH_GTK_2_0(2.4.0,,
		AC_MSG_ERROR(GTK+ 2.4.0+ is required to build Viewglob))
fi

AC_CONFIG_FILES([ \
	Makefile vgseer/Makefile vgd/Makefile \
	vgdisplay/Makefile vgexpand/Makefile vgping/Makefile \
	shell/Makefile common/Makefile])

AC_OUTPUT

