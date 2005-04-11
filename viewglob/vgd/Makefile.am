AM_CPPFLAGS = @GLIB_CFLAGS@ @X_CFLAGS@
LDADD = @GLIB_LIBS@ @X_PRE_LIBS@ @X_LIBS@ -lX11 @X_EXTRA_LIBS@
COMMON = ../common
INCLUDES = -I$(COMMON)

bin_PROGRAMS = vgd

vgd_SOURCES = vgd.c tcp-listen.c $(COMMON)/param-io.c \
              $(COMMON)/hardened-io.c $(COMMON)/shell.c \
	      $(COMMON)/child.c $(COMMON)/x11-stuff.c $(COMMON)/syslogging.c

noinst_HEADERS = tcp-listen.h

