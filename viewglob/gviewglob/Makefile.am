LDADD = @GTK_LIBS@
CFLAGS = @GTK_CFLAGS@

pkgdata_PROGRAMS = gviewglob
gviewglob_SOURCES = gviewglob.c dlisting.c wrap_box.c file_box.c

BUILT_SOURCES = icons.h
icons.h: Makefile
	rm -f icons.h
	gdk-pixbuf-csource --raw --name=icon_16x16_inline icon_16x16.png > icons.h
	gdk-pixbuf-csource --raw --name=icon_24x24_inline icon_24x24.png >> icons.h
	gdk-pixbuf-csource --raw --name=icon_32x32_inline icon_32x32.png >> icons.h
	gdk-pixbuf-csource --raw --name=icon_36x36_inline icon_36x36.png >> icons.h
	gdk-pixbuf-csource --raw --name=context_hidden_inline hidden.png >> icons.h
	gdk-pixbuf-csource --raw --name=context_all_inline all.png >> icons.h
	gdk-pixbuf-csource --raw --name=regular_inline gnome-fs-regular.png >> icons.h
	gdk-pixbuf-csource --raw --name=executable_inline gnome-fs-executable.png >> icons.h
	gdk-pixbuf-csource --raw --name=directory_inline gnome-fs-directory.png >> icons.h
	gdk-pixbuf-csource --raw --name=blockdev_inline gnome-fs-blockdev.png >> icons.h
	gdk-pixbuf-csource --raw --name=chardev_inline gnome-fs-chardev.png >> icons.h
	gdk-pixbuf-csource --raw --name=fifo_inline gnome-fs-fifo.png >> icons.h
	gdk-pixbuf-csource --raw --name=symlink_inline gnome-dev-symlink.png >> icons.h
	gdk-pixbuf-csource --raw --name=socket_inline gnome-fs-socket.png >> icons.h

CLEANFILES = icons.h
noinst_HEADERS = gviewglob.h dlisting.h wrap_box.h file_box.h common.h
EXTRA_DIST = icon_16x16.png icon_24x24.png icon_32x32.png icon_36x36.png \
			 gnome-fs-regular.png gnome-fs-directory.png gnome-fs-fifo.png \
			 gnome-dev-symlink.png gnome-fs-blockdev.png gnome-fs-chardev.png \
			 gnome-fs-socket.png gnome-fs-executable.png \
			 all.png hidden.png

