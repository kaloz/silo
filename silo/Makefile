include ../Rules.make

MESSAGE="SILO is now only supported on SPARC Linux and Solaris."
PROGRAMS=
UFSDEPS=

ifeq (Linux,$(shell uname))
  ifeq (sparc,$(subst sparc64,sparc,$(shell uname -m)))
    PROGRAMS=silo silocheck
    MESSAGE="SILO build for SPARC/Linux completed."
  endif
else
  ifeq (SunOS,$(shell uname -s))
    ifeq (5.,$(findstring 5.,$(shell uname -r)))
      PROGRAMS=silo silocheck
      MESSAGE="SILO build for SPARC/Solaris completed."
      UFSDEPS=../second/ufs.c ufs.h
    endif
  endif
endif

all: $(PROGRAMS)
	@echo $(MESSAGE)

HEADERS=../first/first.h ../first/ultra.h ../first/fd.h
SILO_OBJS=confcheck.o prom.o silo.o
SILOCHK_OBJS=silocheck.o

silo: $(HEADERS) $(SILO_OBJS)
	$(CC) -O2 -o $@ $(SILO_OBJS)

silocheck: $(SILOCHK_OBJS) $(UFSDEPS)
	$(CC) -O2 -o $@ $(SILOCHK_OBJS)

floppy.h: floppy.label $(BIN2H)
	$(BIN2H) floppy_label floppy.label > floppy.h

ufs.h: ../second/ufs.h
	ln -f ../second/ufs.h ufs.h

$(HEADERS):
	$(MAKE) -C ../first first.h ultra.h fd.h

silo.o: silo.c floppy.h $(UFSDEPS)
	$(CC) $(CFLAGS) -DVERSION='"$(VERSION)"' -DIMGVERSION='"$(IMGVERSION)"' -c silo.c

clean:
	$(RM) *.o *~ silo floppy.h ufs.h silocheck
