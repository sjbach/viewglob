dnl Process this file with autoconf to produce a configure script.
AC_INIT([viewglob], [0.8.3])
AM_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_HEADER_TIOCGWINSZ
AC_CHECK_HEADERS([fcntl.h errno.h stdlib.h string.h sys/ioctl.h termios.h unistd.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_MODE_T
AC_TYPE_PID_T

dnl Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_PROG_GCC_TRADITIONAL
AC_CHECK_FUNCS([dup2 getcwd memchr memmove memset mkfifo pathconf select strchr])

dnl The display requires GTK+ 2.4.0.
AM_PATH_GTK_2_0(2.4.0)

dnl Test for bash.
AC_PATH_PROG(BASH_FULL_PATH, bash, nope)
if test "x$BASH_FULL_PATH" = xnope; then
	AC_MSG_ERROR([viewglob requires bash])
fi
AC_SUBST(BASH_FULL_PATH)

AC_CONFIG_FILES([Makefile src/Makefile gviewglob/Makefile])
AC_OUTPUT

