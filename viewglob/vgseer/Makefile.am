AM_CPPFLAGS = -DVG_LIB_DIR="\"$(pkglibdir)\"" @GLIB_CFLAGS@
LDADD = @GLIB_LIBS@
COMMON = ../common
INCLUDES = -I$(COMMON)

bin_PROGRAMS = vgseer

vgseer_SOURCES = vgseer.c cmdline.c sequences.c tc_setraw.c \
                 connection.c actions.c sanitize.c \
               $(COMMON)/ptytty.c $(COMMON)/hardened-io.c \
               $(COMMON)/child.c pty-child.c $(COMMON)/param-io.c \
	       $(COMMON)/shell.c $(COMMON)/tcp-connect.c \
	       $(COMMON)/logging.c $(COMMON)/conf-to-args.c

noinst_HEADERS = cmdline.h sequences.h sanitize.h \
                 tc_setraw.h actions.h connection.h pty-child.h

