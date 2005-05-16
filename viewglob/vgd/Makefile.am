COMMON_DIR = $(top_srcdir)/common

bin_PROGRAMS = vgd

vgd_CPPFLAGS = \
	-DVG_LIB_DIR="\"$(pkglibdir)\"" -I$(COMMON_DIR) \
	@GLIB_CFLAGS@ @X_CFLAGS@
vgd_LDADD = \
	@GLIB_LIBS@ @ETR_SOCKET_LIBS@ \
	@X_PRE_LIBS@ @X_LIBS@ -lX11 @X_EXTRA_LIBS@
vgd_SOURCES = \
	vgd.c \
	tcp-listen.c \
	$(COMMON_DIR)/param-io.c \
	$(COMMON_DIR)/hardened-io.c \
	$(COMMON_DIR)/shell.c \
	$(COMMON_DIR)/child.c \
	$(COMMON_DIR)/x11-stuff.c \
	$(COMMON_DIR)/logging.c \
	$(COMMON_DIR)/syslogging.c \
	$(COMMON_DIR)/conf-to-args.c \
	$(COMMON_DIR)/fgetopt.c

BUILT_SOURCES = vgd-usage.h

vgd-usage.h: vgd-usage.txt
	sed <vgd-usage.txt >vgd-usage.tmp \
		-e 's/\\/\\\\/g' \
		-e 's/"/\\"/g' \
		-e 's/.*/\"&\\n\"/'
	mv vgd-usage.tmp vgd-usage.h

CLEANFILES = vgd-usage.tmp vgd-usage.h
noinst_HEADERS = tcp-listen.h

