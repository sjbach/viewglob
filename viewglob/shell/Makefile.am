# Only the viewglob/gviewglob script should be in the path.
# It calls the other programs in the package.
bin_SCRIPTS = viewglob
pkglib_SCRIPTS = getopt.sh
pkglib_DATA = init-viewglob.bashrc .zshrc

viewglob:
	rm -f viewglob.tmp
	sed -e 's,@datadir\@,$(pkglibdir),g' \
		-e 's,@BASH_FULL_PATH\@,$(BASH_FULL_PATH),g' \
		-e 's,@PACKAGE_NAME\@,$(PACKAGE_NAME),g' \
		-e 's,@PACKAGE_VERSION\@,$(PACKAGE_VERSION),g' \
		-e 's/@RELEASE_DATE\@/$(RELEASE_DATE)/g' \
		viewglob.in > viewglob.tmp
	chmod a+x viewglob.tmp
	mv viewglob.tmp viewglob

getopt.sh:
	rm -f getopt.sh.tmp
	sed -e 's,@BASH_FULL_PATH\@,$(BASH_FULL_PATH),g' \
		getopt.sh.in > getopt.sh.tmp
	chmod a+x getopt.sh.tmp
	mv getopt.sh.tmp getopt.sh

.zshrc:
	cp -f init-viewglob.zshrc .zshrc

CLEANFILES = viewglob .zshrc getopt.sh
EXTRA_DIST = init-viewglob.bashrc init-viewglob.zshrc \
             viewglob.in getopt.sh.in

# Have the program also callable under the name gviewglob.
#install-exec-hook:
#	cd $(DESTDIR)$(bindir) && \
#	rm -f gviewglob && \
#	$(LN_S) viewglob gviewglob
#
#uninstall-local:
#	rm -f $(DESTDIR)$(bindir)/gviewglob

