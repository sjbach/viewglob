LDADD = @GTK_LIBS@
AM_CFLAGS = @GTK_CFLAGS@

pkglib_PROGRAMS = gviewglob
gviewglob_SOURCES = gviewglob.c exhibit.c dlisting.c wrap_box.c file_box.c \
                    feedback.c

# These are mostly standard Gnome icons.
BUILT_SOURCES = icons.h
icons.h: Makefile
	rm -f icons.tmp icons.h
	gdk-pixbuf-csource --raw --name=icon_16x16_inline \
		icon_16x16.png > icons.tmp
	gdk-pixbuf-csource --raw --name=icon_24x24_inline \
		icon_24x24.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=icon_32x32_inline \
		icon_32x32.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=icon_36x36_inline \
		icon_36x36.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=context_hidden_inline \
		hidden.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=context_all_inline \
		all.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=regular_inline \
		gnome-fs-regular.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=executable_inline \
		gnome-fs-executable.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=directory_inline \
		gnome-fs-directory.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=blockdev_inline \
		gnome-fs-blockdev.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=chardev_inline \
		gnome-fs-chardev.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=fifo_inline \
		gnome-fs-fifo.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=symlink_inline \
		gnome-dev-symlink.png >> icons.tmp
	gdk-pixbuf-csource --raw --name=socket_inline \
		gnome-fs-socket.png >> icons.tmp
	mv icons.tmp icons.h

CLEANFILES = icons.h icons.tmp
noinst_HEADERS = gviewglob.h exhibit.h dlisting.h wrap_box.h file_box.h \
                 feedback.h common.h
EXTRA_DIST = icon_16x16.png icon_24x24.png icon_32x32.png icon_36x36.png \
             gnome-fs-regular.png gnome-fs-directory.png gnome-fs-fifo.png \
             gnome-dev-symlink.png gnome-fs-blockdev.png gnome-fs-chardev.png \
             gnome-fs-socket.png gnome-fs-executable.png \
             all.png hidden.png

