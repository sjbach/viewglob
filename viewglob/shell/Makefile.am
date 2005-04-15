
bin_SCRIPTS = viewglob
pkglib_SCRIPTS = getopt.sh conf-to-args.sh
pkglib_DATA = init-viewglob.bashrc .zshrc

viewglob:
	rm -f viewglob.tmp
	sed -e 's,@pkglibdir\@,$(pkglibdir),g' \
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
	rm -f init-viewglob.zshrc.tmp
	sed -e 's,@pkglibdir\@,$(pkglibdir),g' \
		init-viewglob.zshrc.in > init-viewglob.zshrc.tmp
	mv init-viewglob.zshrc.tmp .zshrc

init-viewglob.bashrc:
	rm -f init-viewglob.bashrc.tmp
	sed -e 's,@pkglibdir\@,$(pkglibdir),g' \
		init-viewglob.bashrc.in > init-viewglob.bashrc.tmp
	mv init-viewglob.bashrc.tmp init-viewglob.bashrc


CLEANFILES = \
	viewglob \
	init-viewglob.bashrc \
	init-viewglob.bashrc.tmp \
	.zshrc \
	init-viewglob.zshrc.tmp \
	getopt.sh

EXTRA_DIST = \
	init-viewglob.bashrc.in \
	init-viewglob.bashrc \
	init-viewglob.zshrc.in \
	.zshrc \
	viewglob.in \
	getopt.sh.in \
	conf-to-args.sh

