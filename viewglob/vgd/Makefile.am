AM_CPPFLAGS = @GLIB_CFLAGS@ @X_CFLAGS@
LDADD = @GLIB_LIBS@ @X_PRE_LIBS@ @X_LIBS@ -lX11 @X_EXTRA_LIBS@
COMMON = ../common
INCLUDES = -I$(COMMON)

bin_PROGRAMS = vgd

vgd_SOURCES = vgd.c $(COMMON)/param-io.c \
              $(COMMON)/ptytty.c $(COMMON)/hardened-io.c $(COMMON)/shell.c \
	      $(COMMON)/children.c $(COMMON)/x11-stuff.c

#noinst_HEADERS = display.h

