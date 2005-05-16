COMMON_DIR = $(top_srcdir)/common

bin_PROGRAMS = vgseer

vgseer_CPPFLAGS = \
	-DVG_LIB_DIR="\"$(pkglibdir)\"" -I$(COMMON_DIR) \
	@GLIB_CFLAGS@
vgseer_LDADD = @GLIB_LIBS@ @ETR_SOCKET_LIBS@
vgseer_SOURCES = \
	vgseer.c \
	cmdline.c \
	sequences.c \
	tc_setraw.c \
	connection.c \
	actions.c \
	sanitize.c \
	ptytty.c \
	pty-child.c \
	$(COMMON_DIR)/hardened-io.c \
	$(COMMON_DIR)/child.c \
	$(COMMON_DIR)/param-io.c \
	$(COMMON_DIR)/shell.c \
	$(COMMON_DIR)/socket-connect.c \
	$(COMMON_DIR)/logging.c \
	$(COMMON_DIR)/conf-to-args.c \
	$(COMMON_DIR)/fgetopt.c

BUILT_SOURCES = vgseer-usage.h

vgseer-usage.h: vgseer-usage.txt
	sed <vgseer-usage.txt >vgseer-usage.tmp \
		-e 's/\\/\\\\/g' \
		-e 's/"/\\"/g' \
		-e 's/.*/\"&\\n\"/'
	mv vgseer-usage.tmp vgseer-usage.h

CLEANFILES = vgseer-usage.tmp vgseer-usage.h
noinst_HEADERS = \
	cmdline.h \
	sequences.h \
	sanitize.h \
	tc_setraw.h \
	actions.h \
	connection.h \
	ptytty.h \
	pty-child.h

