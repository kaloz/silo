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
