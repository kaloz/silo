DIRS=first second silo

all clean dep depend:
	@for I in $(DIRS); do cd $$I; make $@ || exit 1; cd ..; done

ifeq (Linux,$(shell uname))
install:
	if [ \! -d /boot ]; then mkdir /boot; fi
	cp -f -b boot/*.b /boot
	if [ \! -f /etc/silo.conf ]; then cp etc/silo.conf /etc; fi
	cp -f -b sbin/silo /sbin
	/sbin/silo -f
else
ifeq (SunOS,$(shell uname -s))
ifeq (5.,$(findstring 5.,$(shell uname -r)))
install:
	if [ \! -d /boot ]; then mkdir /boot; fi
	cp -f -b boot/*.b /boot
	if [ \! -f /etc/silo.conf ]; then cp etc/silo.conf /etc; fi
	misc/solarissilo -f
else
install:
	@echo SunOS SILO not yet supported
endif
else
install:
	@echo SILO for other platforms not supported
endif
endif
