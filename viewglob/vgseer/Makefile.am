AM_CPPFLAGS = @GLIB_CFLAGS@
LDADD = @GLIB_LIBS@
COMMON = ../common
INCLUDES = -I$(COMMON)

pkglib_PROGRAMS = vgseer

vgseer_SOURCES = vgseer.c children.c cmdline.c sequences.c \
               tc_setraw.c connection.c actions.c     \
               $(COMMON)/ptytty.c $(COMMON)/hardened-io.c \
               $(COMMON)/param-io.c $(COMMON)/shell.c

noinst_HEADERS = children.h cmdline.h sequences.h \
                 tc_setraw.h actions.h connection.h

