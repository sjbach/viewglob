AM_CPPFLAGS = @GLIB_CFLAGS@ @X_CFLAGS@
LDADD = @X_PRE_LIBS@ @X_LIBS@ -lX11 @X_EXTRA_LIBS@ @GLIB_LIBS@
INCLUDES = -I../common

pkglib_PROGRAMS = seer

seer_SOURCES = seer.c children.c cmdline.c sequences.c sanitize.c  \
               tc_setraw.c xmalloc.c buffer.c   \
               viewglob-error.c misc.c actions.c     \
               x11-stuff.c ../common/ptytty.c ../common/hardened_io.c

noinst_HEADERS = seer.h children.h cmdline.h sequences.h sanitize.h   \
                 tc_setraw.h vgseer-common.h   \
                 actions.h buffer.h circular.h x11-stuff.h

