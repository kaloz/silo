SUBDIRS=first second silo

VERSION=0.9.9
IMGVERSION=0.99
export VERSION IMGVERSION

all dep depend clean:
	@for I in $(SUBDIRS); do cd $$I; $(MAKE) $@ || exit 1; cd ..; done

ifeq (Linux,$(shell uname))
install:
	if [ \! -d $(DESTDIR)/boot ]; then mkdir $(DESTDIR)/boot; fi
	cp -f -b first/*.b second/*.b $(DESTDIR)/boot
	if [ \! -f $(DESTDIR)/etc/silo.conf ]; then cp etc/silo.conf $(DESTDIR)/etc; fi
	mkdir -p $(DESTDIR)/sbin $(DESTDIR)/usr/bin
	cp -f -b silo/silo $(DESTDIR)/sbin
	cp -f -b silo/silocheck $(DESTDIR)/usr/bin
else
ifeq (SunOS,$(shell uname -s))
ifeq (5.,$(findstring 5.,$(shell uname -r)))
install:
	if [ \! -d /boot ]; then mkdir /boot; fi
	cp -f -b first/*.b second/*.b /boot
	if [ \! -f /etc/silo.conf ]; then cp etc/silo.conf /etc; fi
else
install:
	@echo SunOS SILO not yet supported
endif
else
install:
	@echo SILO for other platforms not supported
endif
endif
