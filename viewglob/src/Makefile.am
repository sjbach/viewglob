# Only the viewglob/gviewglob script should be in the path.
# It calls the other programs.
bin_SCRIPTS = viewglob

viewglob: Makefile
	rm -f viewglob.tmp
	sed -e 's,@datadir\@,$(pkgdatadir),g' \
		-e 's,@BASH_FULL_PATH\@,$(BASH_FULL_PATH),g' \
		-e 's,@PACKAGE_NAME\@,$(PACKAGE_NAME),g' \
		-e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' \
		-e 's/@RELEASE_DATE\@/$(RELEASE_DATE)/g' \
		viewglob.in > viewglob.tmp
	chmod a+x viewglob.tmp
	mv viewglob.tmp viewglob

getopt.sh: Makefile
	rm -f getopt.sh.tmp
	sed -e 's,@BASH_FULL_PATH\@,$(BASH_FULL_PATH),g' \
		getopt.sh.in > getopt.sh.tmp
	chmod a+x getopt.sh.tmp
	mv getopt.sh.tmp getopt.sh

.zshrc: Makefile
	cp -f init-viewglob.zshrc .zshrc

# Very slight modifications to these files from Marc J. Rochkind's
# book Advanced Unix Programming 2nd ed.  You can get them on the
# web at: http://www.basepath.com/aup/
BUILT_SOURCES = ptutil.viewglob.c ptutil.viewglob.h \
                tc_setraw.viewglob.c tc_setraw.viewglob.h
ptutil.viewglob.c: Makefile
	rm -f ptutil.viewglob.c
	patch -i ptutil.c.diff -o ptutil.viewglob.c ptutil.c
ptutil.viewglob.h: Makefile
	rm -f ptutil.viewglob.h
	patch -i ptutil.h.diff -o ptutil.viewglob.h ptutil.h
tc_setraw.viewglob.c: Makefile
	rm -f tc_setraw.viewglob.c
	patch -i tc_setraw.c.diff -o tc_setraw.viewglob.c tc_setraw.c
tc_setraw.viewglob.h: Makefile
	rm -f tc_setraw.viewglob.h
	patch -i tc_setraw.h.diff -o tc_setraw.viewglob.h tc_setraw.h

CLEANFILES = viewglob .zshrc getopt.sh ptutil.viewglob.c       \
             ptutil.viewglob.h tc_setraw.viewglob.c tc_setraw.viewglob.h

pkgdata_DATA = init-viewglob.bashrc .zshrc
pkgdata_SCRIPTS = getopt.sh
pkgdata_PROGRAMS = seer glob-expand

seer_SOURCES = seer.c children.c cmdline.c sequences.c sanitize.c  \
               tc_setraw.viewglob.c ptutil.viewglob.c xmalloc.c    \
               viewglob-error.c hardened_io.c misc.c actions.c     \
               buffer.c

glob_expand_SOURCES = glob-expand.c xmalloc.c viewglob-error.c

noinst_HEADERS = seer.h children.h cmdline.h sequences.h sanitize.h   \
                 tc_setraw.h ptutil.h viewglob-error.h hardened_io.h  \
                 common.h glob-expand.h actions.h buffer.h circular.h

EXTRA_DIST = ptutil.h ptutil.h.diff ptutil.c ptutil.c.diff \
             tc_setraw.h tc_setraw.h.diff tc_setraw.c tc_setraw.c.diff \
             viewglob.in getopt.sh.in init-viewglob.bashrc init-viewglob.zshrc

# Have the program also callable under the name gviewglob.
install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	rm -f gviewglob && \
	$(LN_S) viewglob gviewglob

uninstall-local:
	rm -f $(DESTDIR)$(bindir)/gviewglob

