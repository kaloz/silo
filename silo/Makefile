include ../Rules.make

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

CFLAGS=-Wall -g -O2 -I../include
CC=gcc

silo: silo.c floppy.h headers confcheck.o prom.o
	$(CC) $(CFLAGS) -I/usr/src/linux/include -DVERSION='"$(VERSION)"' \
		-DIMGVERSION='"$(IMGVERSION)"' -o silo silo.c prom.o confcheck.o

silocheck: silocheck.c
	$(CC) $(CFLAGS) -I/usr/src/linux/include -o silocheck silocheck.c

solarissilo: silo.c floppy.h ufs.o prom.c confcheck.c headers
	$(CC) $(CFLAGS) -D__solaris__ -o solarissilo silo.c ufs.o prom.c confcheck.c

solarissilocheck: silocheck.c ufs.c
	$(CC) $(CFLAGS) -D__solaris__ -o solarissilocheck silocheck.c ufs.c

floppy.h: floppy.label util
	./util floppy.label | sed 's/cdrom/floppy/' > floppy.h

util: util.c
	$(CC) $(CFLAGS) -o util util.c

ufs.o: ../second/ufs.c ../second/ufs.h
	$(CC) $(CFLAGS) -D__solaris__ -c ../second/ufs.c

headers:
	$(MAKE) -C ../first first.h ultra.h fd.h

clean:
	rm -f $(INSTBOOT) $(TDEV) *.o *~ silo solarissilo sunossilo floppy.h util silocheck solarissilocheck ufs.[ch]
