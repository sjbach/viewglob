AM_CPPFLAGS = @GLIB_CFLAGS@
LDADD = @GLIB_LIBS@
INCLUDES = -I../common

pkglib_PROGRAMS = seer

seer_SOURCES = seer.c children.c cmdline.c sequences.c sanitize.c  \
               tc_setraw.c buffer.c   \
               viewglob-error.c actions.c     \
               ../common/ptytty.c ../common/hardened_io.c

noinst_HEADERS = seer.h children.h cmdline.h sequences.h sanitize.h   \
                 tc_setraw.h vgseer-common.h   \
                 actions.h buffer.h circular.h

