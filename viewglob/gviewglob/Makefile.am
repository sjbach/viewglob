LDADD = @GTK_LIBS@
CFLAGS = @GTK_CFLAGS@

pkgdata_PROGRAMS = gviewglob
gviewglob_SOURCES = gviewglob.c fitem.c dlisting.c wrap_box.c

BUILT_SOURCES = icons.h
icons.h: Makefile
	rm -f icons.h
	gdk-pixbuf-csource --raw --name=icon_16x16_inline icon_16x16.png > icons.h
	gdk-pixbuf-csource --raw --name=icon_24x24_inline icon_24x24.png >> icons.h
	gdk-pixbuf-csource --raw --name=icon_32x32_inline icon_32x32.png >> icons.h
	gdk-pixbuf-csource --raw --name=icon_36x36_inline icon_36x36.png >> icons.h
	gdk-pixbuf-csource --raw --name=regular_inline gnome-fs-regular.png >> icons.h
	gdk-pixbuf-csource --raw --name=directory_inline gnome-fs-directory.png >> icons.h
	gdk-pixbuf-csource --raw --name=hidden_inline hidden.png >> icons.h
	gdk-pixbuf-csource --raw --name=all_inline all.png >> icons.h

CLEANFILES = icons.h
noinst_HEADERS = gviewglob.h fitem.h dlisting.h wrap_box.h common.h
EXTRA_DIST = icon_16x16.png icon_24x24.png icon_32x32.png icon_36x36.png \
			 gnome-fs-regular.png gnome-fs-directory.png \
			 all.png hidden.png

