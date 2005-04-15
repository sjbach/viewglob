COMMON_DIR = $(top_srcdir)/common

pkglib_PROGRAMS = vgping

vgping_CPPFLAGS = @GLIB_CFLAGS@ -DVG_LIB_DIR="\"$(pkglibdir)\"" -I$(COMMON_DIR)
vgping_LDADD = @GLIB_LIBS@ @ETR_SOCKET_LIBS@
vgping_SOURCES = \
	vgping.c \
	$(COMMON_DIR)/hardened-io.c \
	$(COMMON_DIR)/param-io.c \
	$(COMMON_DIR)/tcp-connect.c

