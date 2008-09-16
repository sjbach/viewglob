COMMON_DIR = $(top_srcdir)/common
AM_CPPFLAGS = @GLIB_CFLAGS@ -DVG_LIB_DIR="\"$(pkglibdir)\"" -I$(COMMON_DIR)
LDADD = @GLIB_LIBS@

pkglib_PROGRAMS = vgexpand

vgexpand_SOURCES = vgexpand.c

noinst_HEADERS = vgexpand.h

