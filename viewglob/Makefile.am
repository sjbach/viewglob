AUTOMAKE_OPTIONS = gnu

#SUBDIRS = shell vgd vgdisplay vgping vgseer vgexpand
SUBDIRS = vgseer vgdisplay vgexpand shell 
man_MANS = viewglob.1
EXTRA_DIST = COPYING2 HACKING $(man_MANS)

# Also put the manpage under the name gviewglob.
#install-data-hook:
#	cd $(DESTDIR)$(mandir)/man1 && \
#	rm -f gviewglob.1 && \
#	$(LN_S) viewglob.1 gviewglob.1
#
#uninstall-local:
#	rm -f $(DESTDIR)$(mandir)/man1/gviewglob.1

