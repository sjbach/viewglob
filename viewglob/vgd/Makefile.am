AM_CPPFLAGS = -DVG_LIB_DIR="\"$(pkglibdir)\""  @GLIB_CFLAGS@ @X_CFLAGS@
LDADD = @GLIB_LIBS@ @X_PRE_LIBS@ @X_LIBS@ -lX11 @X_EXTRA_LIBS@ @ETR_SOCKET_LIBS@
COMMON = ../common
INCLUDES = -I$(COMMON)

bin_PROGRAMS = vgd

vgd_SOURCES = vgd.c tcp-listen.c $(COMMON)/param-io.c \
              $(COMMON)/hardened-io.c $(COMMON)/shell.c \
	      $(COMMON)/child.c $(COMMON)/x11-stuff.c \
	      $(COMMON)/logging.c $(COMMON)/syslogging.c \
	      $(COMMON)/conf-to-args.c

noinst_HEADERS = tcp-listen.h

