SUBDIRS=common first second silo tilo

all dep depend clean:
	@for I in $(SUBDIRS); do cd $$I; $(MAKE) $@ || exit 1; cd ..; done

ifeq (Linux,$(shell uname))
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
else
ifeq (SunOS,$(shell uname -s))
ifeq (5.,$(findstring 5.,$(shell uname -r)))
install:
	install -d -m755 $(DESTDIR)/boot
	install -m644 first/*.b second/*.b $(DESTDIR)/boot/
	[ -f $(DESTDIR)/etc/silo.conf ] || install -m644 etc/silo.conf $(DESTDIR)/etc/
else
install:
	@echo SunOS SILO not yet supported
endif
else
install:
	@echo SILO for other platforms not supported
endif
endif
