include ../Rules.make

PROGRAMS=silo silocheck
UFSDEPS=

CFLAGS=-O2 -Wall -I. -I../include

ifeq ($(OPSYS),Solaris)
  UFSDEPS=../second/ufs.c ufs.h
endif

all: $(PROGRAMS)

HEADERS=../first/first.h ../first/ultra.h ../first/fd.h
SILO_OBJS=confcheck.o prom.o silo.o
SILOCHK_OBJS=silocheck.o

silo: $(HEADERS) $(SILO_OBJS)
	$(CC) -o $@ $(SILO_OBJS)

silocheck: $(SILOCHK_OBJS) $(UFSDEPS)
	$(CC) -o $@ $(SILOCHK_OBJS)

floppy.h: floppy.label $(BIN2H)
	$(BIN2H) floppy_label floppy.label > floppy.h

ufs.h: ../second/ufs.h
	ln -f ../second/ufs.h ufs.h

$(HEADERS):
	$(MAKE) -C ../first first.h ultra.h fd.h

silo.o: silo.c floppy.h $(UFSDEPS)
	$(CC) $(CFLAGS) -DVERSION='"$(VERSION)"' -DIMGVERSION='"$(IMGVERSION)"' -c silo.c

clean:
	$(RM) *.o silo floppy.h ufs.h silocheck
