AM_CPPFLAGS = @GLIB_CFLAGS@ @X_CFLAGS@
LDADD = @GLIB_LIBS@ @X_PRE_LIBS@ @X_LIBS@ -lX11 @X_EXTRA_LIBS@
COMMON = ../common
INCLUDES = -I$(COMMON)

bin_PROGRAMS = vgd

vgd_SOURCES = vgd.c x11-stuff.c $(COMMON)/param-io.c $(COMMON)/io-all.c \
              $(COMMON)/ptytty.c

#noinst_HEADERS = 

