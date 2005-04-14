AM_CPPFLAGS = -DVG_LIB_DIR="\"$(pkglibdir)\"" @GLIB_CFLAGS@
LDADD = @GLIB_LIBS@ @ETR_SOCKET_LIBS@
COMMON = ../common
INCLUDES = -I$(COMMON)

pkglib_PROGRAMS = vgping

vgping_SOURCES = vgping.c $(COMMON)/hardened-io.c $(COMMON)/param-io.c \
	       $(COMMON)/tcp-connect.c

