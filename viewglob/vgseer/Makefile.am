AM_CPPFLAGS = @GLIB_CFLAGS@
LDADD = @GLIB_LIBS@
INCLUDES = -I../common

pkglib_PROGRAMS = vgseer

vgseer_SOURCES = vgseer.c children.c cmdline.c sequences.c sanitize.c  \
               tc_setraw.c buffer.c   \
               viewglob-error.c actions.c     \
               ../common/ptytty.c ../common/hardened_io.c

noinst_HEADERS = vgseer.h children.h cmdline.h sequences.h sanitize.h   \
                 tc_setraw.h actions.h buffer.h circular.h

