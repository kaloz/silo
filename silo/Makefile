ifeq (Linux,$(shell uname))
ifeq (sparc,$(subst sparc64,sparc,$(shell uname -m)))
all: silo silocheck
else
all:
	@echo SILO install program now supported on Linux/SPARC and Solaris only
endif
else
ifeq (SunOS,$(shell uname -s))
ifeq (5.,$(findstring 5.,$(shell uname -r)))
all: solarissilo solarissilocheck
else
all:
	@echo SunOS SILO not yet supported
endif
else
all:
	@echo SILO install program now supported on Linux/SPARC and Solaris only
endif
endif

silo: silo.c floppy.h prom.c prom.h headers
	gcc -Wall -I../include -I/usr/include -I/usr/src/linux/include -O2 -o silo silo.c prom.c
	strip silo
	ln -f silo ../sbin

silocheck: silocheck.c
	gcc -I../include -I/usr/include -I/usr/src/linux/include -O2 -o silocheck silocheck.c
	strip silocheck
	ln -f silocheck ../misc

solarissilo: silo.c ufs.c ufs.h floppy.h prom.c prom.h headers
	gcc -D__solaris__ -I../include -o solarissilo silo.c ufs.c prom.c
	strip solarissilo
	ln -f solarissilo ../misc

solarissilocheck: silocheck.c ufs.c ufs.h
	gcc -D__solaris__ -I../include -o solarissilocheck silocheck.c ufs.c
	strip solarissilocheck
	ln -f solarissilocheck ../misc

floppy.h: floppy.label util
	./util floppy.label | sed 's/cdrom/floppy/' > floppy.h

util: util.c
	gcc -o util util.c

ufs.h: ../second/ufs.h
	ln -sf ../second/ufs.h ufs.h

ufs.c: ../second/ufs.c
	ln -sf ../second/ufs.c ufs.c

headers:
	cd ../first; make first.h ultra.h fd.h

clean:
	rm -f $(INSTBOOT) $(TDEV) *.o *~ silo solarissilo sunossilo floppy.h util silocheck solarissilocheck ufs.[ch]
