include Rules.make

ifeq ($(OPSYS),Linux)
  SUBDIRS=common first second tilo silo
else
  SUBDIRS=silo
endif

all dep depend:
	@for I in $(SUBDIRS); do cd $$I; $(MAKE) $@ || exit 1; cd ..; done

clean:
	@for I in $(SUBDIRS); do cd $$I; $(MAKE) $@ || exit 1; cd ..; done
	rm -rf boot-loaders* boot

ifeq ($(OPSYS),Linux)
install:
	install -d -m755 $(DESTDIR)/boot $(DESTDIR)/etc $(DESTDIR)/sbin \
		$(DESTDIR)/usr/sbin $(DESTDIR)/usr/bin
	install -m644 first/*.b second/*.b $(DESTDIR)/boot/
	install -m755 silo/silo $(DESTDIR)/sbin
	install -m755 silo/silocheck $(DESTDIR)/usr/sbin
	[ -f $(DESTDIR)/etc/silo.conf ] || \
		install -m644 etc/silo.conf $(DESTDIR)/etc/
	install -m755 tilo/maketilo $(DESTDIR)/usr/bin
	install -m755 tilo/tilo.sh $(DESTDIR)/usr/bin/tilo
	for sect in 1 5 8; do \
		install -d -m755 $(DESTDIR)/usr/share/man/man$$sect; \
		install -m644 man/*.$$sect $(DESTDIR)/usr/share/man/man$$sect; \
	done
	if test x"$(DESTDIR)" = x; then \
		echo "You need to run 'silo -f' now, to update the boot block" 1>&2; \
	fi
else
 ifeq ($(OPSYS),SunOS)
  ifeq (5.,$(findstring 5.,$(OSREV)))
install:
	install -d -m755 $(DESTDIR)/sbin $(DESTDIR)/usr/sbin
	install -m755 silo/silo $(DESTDIR)/sbin
	install -m755 silo/silocheck $(DESTDIR)/usr/sbin
	[ -f $(DESTDIR)/etc/silo.conf ] || \
		install -m644 etc/silo.conf $(DESTDIR)/etc/
	if test x"$(DESTDIR)" = x; then \
		echo "You need to run 'silo -f' now, to update the boot block" 1>&2; \
	fi
  else
install:
	@echo SunOS SILO not yet supported
  endif
 else
install:
	@echo SILO for other platforms not supported
 endif
endif


# This is just for me to make release tarballs
release: ../silo-loaders-$(VERSION).tar.gz ../silo-$(VERSION).tar.gz ../silo-$(VERSION).tar.bz2
	rm -rf ../silo-$(VERSION) boot

../silo-loaders-$(VERSION).tar%:
	if ! test -d boot; then \
		for I in first second; do $(MAKE) -C $$I all || exit 1; done; \
		install -d -m755  boot; \
		install -m644 first/*.b second/*.b boot/; \
	fi
	case "$*" in .gz) foo=z;; .bz2) foo=j;; *) foo="";; esac; \
		tar $${foo}cf $@ boot

../silo-$(VERSION).tar%: clean
	if ! test -d ../silo-$(VERSION); then \
		install -d -m755 ../silo-$(VERSION); \
		cp -a ./ ../silo-$(VERSION); \
	fi
	cd ../silo-$(VERSION) && find -name .\#\* -o -name CVS -o -name .cvsignore | \
		xargs -r rm -rf
	case "$*" in .gz) foo="gzip -c";; .bz2) foo="bzip2 -c";; *) foo=cat;; esac; \
		(cd ../ && tar cf - silo-$(VERSION)) | $$foo > $@
