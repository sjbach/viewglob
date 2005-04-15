AUTOMAKE_OPTIONS = gnu

if FULL_BUILD
  SUBDIRS_FULL = vgd vgdisplay vgping
  MANS_FULL = vgd.1
endif

SUBDIRS = common vgseer vgexpand shell $(SUBDIRS_FULL)
man_MANS = vgseer.1 viewglob.1 $(MANS_FULL)

viewglob.1:
	rm -f viewglob.1.tmp
	sed -e 's/@RELEASE_DATE\@/$(RELEASE_DATE)/g' \
		viewglob.1.in > viewglob.1.tmp
	mv viewglob.1.tmp viewglob.1

vgseer.1:
	rm -f vgseer.1.tmp
	sed -e 's/@RELEASE_DATE\@/$(RELEASE_DATE)/g' \
		vgseer.1.in > vgseer.1.tmp
	mv vgseer.1.tmp vgseer.1

vgd.1:
	rm -f vgd.1.tmp
	sed -e 's/@RELEASE_DATE\@/$(RELEASE_DATE)/g' \
		vgd.1.in > vgd.1.tmp
	mv vgd.1.tmp vgd.1

EXTRA_DIST = COPYING2 HACKING $(man_MANS)
CLEAN_FILES = viewglob.1.tmp vgseer.1.tmp vgd.1.tmp

