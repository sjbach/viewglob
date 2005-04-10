LDADD = @GTK_LIBS@ @GLIB_LIBS@
AM_CPPFLAGS = @GTK_CFLAGS@ @GLIB_CFLAGS@
COMMON = ../common
INCLUDES = -I$(COMMON)

pkglib_PROGRAMS = vgclassic vgmini
vgclassic_SOURCES = vgclassic.c exhibit.c dlisting.c wrap_box.c file_box.c \
                    lscolors.c $(COMMON)/param-io.c $(COMMON)/hardened-io.c \
		    display-common.c $(COMMON)/x11-stuff.c  \
		    $(COMMON)/syslogging.c
vgmini_SOURCES = vgmini.c wrap_box.c file_box.c lscolors.c dircont.c \
                 $(COMMON)/param-io.c $(COMMON)/hardened-io.c  \
		 display-common.c $(COMMON)/x11-stuff.c $(COMMON)/syslogging.c

BUILT_SOURCES = app_icons.h file_icons.h
#app_icons.h: Makefile
app_icons.h:
	rm -f app_icons.tmp app_icons.h
	gdk-pixbuf-csource --raw --name=icon_16x16_inline \
		icon_16x16.png > app_icons.tmp
	gdk-pixbuf-csource --raw --name=icon_24x24_inline \
		icon_24x24.png >> app_icons.tmp
	gdk-pixbuf-csource --raw --name=icon_32x32_inline \
		icon_32x32.png >> app_icons.tmp
	gdk-pixbuf-csource --raw --name=icon_36x36_inline \
		icon_36x36.png >> app_icons.tmp
	mv app_icons.tmp app_icons.h

# These are mostly standard Gnome icons.
#file_icons.h: Makefile
file_icons.h:
	rm -f file_icons.tmp file_icons.h
	gdk-pixbuf-csource --raw --name=regular_inline \
		gnome-fs-regular.png >> file_icons.tmp
	gdk-pixbuf-csource --raw --name=executable_inline \
		gnome-fs-executable.png >> file_icons.tmp
	gdk-pixbuf-csource --raw --name=directory_inline \
		gnome-fs-directory.png >> file_icons.tmp
	gdk-pixbuf-csource --raw --name=blockdev_inline \
		gnome-fs-blockdev.png >> file_icons.tmp
	gdk-pixbuf-csource --raw --name=chardev_inline \
		gnome-fs-chardev.png >> file_icons.tmp
	gdk-pixbuf-csource --raw --name=fifo_inline \
		gnome-fs-fifo.png >> file_icons.tmp
	gdk-pixbuf-csource --raw --name=symlink_inline \
		gnome-dev-symlink.png >> file_icons.tmp
	gdk-pixbuf-csource --raw --name=socket_inline \
		gnome-fs-socket.png >> file_icons.tmp
	mv file_icons.tmp file_icons.h


CLEANFILES = app_icons.h app_icons.tmp file_icons.h file_icons.tmp
noinst_HEADERS = exhibit.h dlisting.h wrap_box.h file_box.h \
                 lscolors.h dircont.h display-common.h
EXTRA_DIST = icon_16x16.png icon_24x24.png icon_32x32.png icon_36x36.png \
             gnome-fs-regular.png gnome-fs-directory.png gnome-fs-fifo.png \
             gnome-dev-symlink.png gnome-fs-blockdev.png gnome-fs-chardev.png \
             gnome-fs-socket.png gnome-fs-executable.png

