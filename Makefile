include Rules.make


# These only get built on Linux
ifeq ($(OPSYS),Linux)
  SUBDIRS  = common first second first-isofs tilo
  MANPAGES = maketilo.1 tilo.1
endif

# These get built on Linux and Solaris
MANPAGES += silo.8 silo.conf.5
SUBDIRS  += silo

all dep depend clean:
	@for I in $(SUBDIRS); do $(MAKE) -C $$I $@ || exit 1; done

ifeq ($(OPSYS),$(findstring $(OPSYS),Linux Solaris))
install:
	install -d -m755 $(DESTDIR)/etc $(DESTDIR)/sbin $(DESTDIR)/usr/sbin
	install -m755 silo/silo $(DESTDIR)/sbin
	install -m755 silo/silocheck $(DESTDIR)/usr/sbin
	[ -f $(DESTDIR)/etc/silo.conf ] || \
		install -m644 etc/silo.conf $(DESTDIR)/etc/
ifeq ($(OPSYS),Linux)
	install -d -m755 $(DESTDIR)/boot $(DESTDIR)/usr/bin
	install -m644 first/*.b second/*.b first-isofs/*.b $(DESTDIR)/boot/
	install -m755 tilo/maketilo $(DESTDIR)/usr/bin/
	install -m755 tilo/tilo.sh $(DESTDIR)/usr/bin/tilo
endif
	for manpage in $(MANPAGES); do \
		sect=`echo $$manpage | sed 's/.*\([1-8]\)$$/\1/'`; \
		install -d -m755 $(DESTDIR)/usr/share/man/man$$sect; \
		install -m644 man/$$manpage $(DESTDIR)/usr/share/man/man$$sect/; \
	done
	if test x"$(DESTDIR)" = x; then \
		echo "You need to run 'silo -f' now, to update the boot block" 1>&2; \
	fi
else
install:
	@echo SILO is only supported on SPARC Linux and Solaris.
endif

tag:
	svn copy -m "Tag $(VERSION)" svn://svn.phunnypharm.org/silo/trunk \
		svn://svn.phunnypharm.org/silo/tags/v$(VERSION)

# This is just for me to make release tarballs
release: ../silo-loaders-$(VERSION).tar.gz ../silo-$(VERSION).tar.gz ../silo-$(VERSION).tar.bz2
	rm -rf ../silo-$(VERSION) boot

../silo-loaders-$(VERSION).tar%:
	rm -rf ../silo-$(VERSION)
	svn export svn://svn.sparc-boot.org/silo/tags/v$(VERSION) \
		../silo-$(VERSION)
	install -d ../silo-$(VERSION)/boot
	for I in first first-isofs second; do $(MAKE) -C ../silo-$(VERSION)/$$I all || exit 1; done;
	install -d ../silo-$(VERSION)/boot
	(cd ../silo-$(VERSION); install -m644 first/*.b first-isofs/*.b second/*.b boot/)
	(cd ../silo-$(VERSION); \
		case "$*" in .gz) foo="gzip -c9";; .bz2) foo="bzip2 -c9";; *) foo=cat;; esac; \
		tar cf - boot | $$foo > $@; \
	)
	rm -rf ../silo-$(VERSION)

../silo-$(VERSION).tar%: clean
	rm -rf ../silo-$(VERSION)
	svn export svn://svn.sparc-boot.org/silo/tags/v$(VERSION) \
		../silo-$(VERSION)
	case "$*" in .gz) foo="gzip -c9";; .bz2) foo="bzip2 -c9";; *) foo=cat;; esac; \
		(cd ../ && tar cf - silo-$(VERSION)) | $$foo > $@
	rm -rf ../silo-$(VERSION)
