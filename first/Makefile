##
## Linux Loader for SPARC
##
#

CC=gcc
LD=ld
AS=as
STRIP=strip
NM=nm

RM=/bin/rm -f
DD=dd
ELFTOAOUT=/usr/bin/elftoaout

include ../Rules.make

# Relocate to 2.5MB. The Hitchhiker's Guide to Open Boot Rev 3 claims
# that 3MB are guaranteed to be mapped.
# Changing this value will require changing a bunch of other constants throughout the code
LDFLAGS=-N -Ttext 0x4000

.S.o:
	$(CC) $(CFLAGS) -c $*.S

ifeq ($(OPSYS),Linux)
 ifeq (sparc,$(MACHINE))
all: first.b first.h ultra.b ultra.h cd.b generic.b generic.h fd.b ieee32.b

first.h: first
	sed -n 's/^\(000000000000\|0000\)4\([0-9a-f][0-9a-f][0-9a-f]\) .*digit_here.*$$/#define DIGIT_OFFSET_TMP 0x\2/p' < first.map > first.h
	sed -n 's/^\(000000000000\|0000\)4\([0-9a-f][0-9a-f][0-9a-f]\) .*letter_here.*$$/#define LETTER_OFFSET_TMP 0x\2/p' < first.map >> first.h
	sed -n 's/^\(000000000000\|0000\)4\([0-9a-f][0-9a-f][0-9a-f]\) .*number_here.*$$/#define NUMBER_OFFSET_TMP 0x\2/p' < first.map >> first.h
ultra.h: ultra
	sed -n 's/^\(000000000000\|0000\)4\([0-9a-f][0-9a-f][0-9a-f]\) .*letter_here.*$$/#define ULTRA_LETTER_OFFSET_TMP 0x\2/p' < ultra.map > ultra.h
	sed -n 's/^\(000000000000\|0000\)4\([0-9a-f][0-9a-f][0-9a-f]\) .*number_here.*$$/#define ULTRA_NUMBER_OFFSET_TMP 0x\2/p' < ultra.map >> ultra.h
generic.h: generic
	sed -n 's/^\(000000000000\|0000\)4\([0-9a-f][0-9a-f][0-9a-f]\) .*digit_here.*$$/#define GENERIC_DIGIT_OFFSET_TMP 0x\2/p' < generic.map > generic.h
	sed -n 's/^\(000000000000\|0000\)4\([0-9a-f][0-9a-f][0-9a-f]\) .*letter_here.*$$/#define GENERIC_LETTER_OFFSET_TMP 0x\2/p' < generic.map >> generic.h
	sed -n 's/^\(000000000000\|0000\)4\([0-9a-f][0-9a-f][0-9a-f]\) .*number_here.*$$/#define GENERIC_NUMBER_OFFSET_TMP 0x\2/p' < generic.map >> generic.h
fd.h: fd
	sed -n 's/^\(000000000000\|0000\)4\([0-9a-f][0-9a-f][0-9a-f]\) .*digit_here.*$$/#define FD_DIGIT_OFFSET_TMP 0x\2/p' < fd.map > fd.h
	sed -n 's/^\(000000000000\|0000\)4\([0-9a-f][0-9a-f][0-9a-f]\) .*letter_here.*$$/#define FD_LETTER_OFFSET_TMP 0x\2/p' < fd.map >> fd.h
 else
all:
	@echo First stage bootblock should be build under Linux/Sparc only

first.h ultra.h generic.h fd.h:
	@if [ ! -f first.h ]; then echo 'First do make under Linux/Sparc (and do not make clean until compiled on this platform)'; exit 1; fi
 endif
else
all:
	@echo First stage bootblock should be build under Linux/Sparc only

first.h ultra.h generic.h fd.h:
	@if [ ! -f first.h ]; then echo 'First do make under Linux/Sparc (and do not make clean until compiled on this platform)'; exit 1; fi
endif

first.o: first.S
	$(CC) $(CFLAGS) -c first.S -o first.o

first: first.o
	$(LD) $(LDFLAGS) -o first first.o
	$(NM) first | grep -v '*ABS*' | sort > first.map
	$(STRIP) first

first.b: first
	$(ELFTOAOUT) -o first.b first
	echo -n 'SILO'$(IMGVERSION) | $(DD) of=first.b bs=8 conv=notrunc seek=3 count=1
	$(DD) if=/dev/zero of=first.b bs=4 count=1 seek=127

ultra.o: ultra.S
	$(CC) $(CFLAGS) -c -Wa,-Av9a ultra.S

ultra: ultra.o
	$(LD) $(LDFLAGS) -o ultra ultra.o
	$(NM) ultra | grep -v '*ABS*' | sort > ultra.map
	$(STRIP) ultra

ultra.b: ultra
	$(ELFTOAOUT) -o ultra.b ultra
	echo -n 'SILO'$(IMGVERSION) | $(DD) of=ultra.b bs=8 conv=notrunc seek=3 count=1
	echo -n -e '\340' | $(DD) of=ultra.b bs=1 count=1 seek=7 conv=notrunc
	$(DD) if=/dev/zero of=ultra.b bs=4 count=1 seek=127

cd.o: cd.S
	$(CC) $(CFLAGS) -c -Wa,-Av9a cd.S

cd: cd.o
	$(LD) $(LDFLAGS) -o cd cd.o
	$(NM) cd | grep -v '*ABS*' | sort > cd.map
	$(STRIP) cd

cd.b: cd
	$(ELFTOAOUT) -o cd.b cd
	echo -n 'SILO'$(IMGVERSION) | $(DD) of=cd.b bs=8 conv=notrunc seek=3 count=1
	echo -n -e '\340' | $(DD) of=cd.b bs=1 count=1 seek=7 conv=notrunc
	$(DD) if=/dev/zero of=cd.b bs=4 count=1 seek=255

generic.o: generic.S
	$(CC) $(CFLAGS) -c -Wa,-Av9a generic.S

generic: generic.o
	$(LD) $(LDFLAGS) -o generic generic.o
	$(NM) generic | grep -v '*ABS*' | sort > generic.map
	$(STRIP) generic

generic.b: generic
	$(ELFTOAOUT) -o generic.b generic
	echo -n 'SILO'$(IMGVERSION) | $(DD) of=generic.b bs=8 conv=notrunc seek=3 count=1
	echo -n -e '\340' | $(DD) of=generic.b bs=1 count=1 seek=7 conv=notrunc
	$(DD) if=/dev/zero of=generic.b bs=4 count=1 seek=255

fd.o: fd.S
	$(CC) $(CFLAGS) -c -Wa,-Av9a fd.S

fd: fd.o
	$(LD) $(LDFLAGS) -o fd fd.o
	$(NM) fd | grep -v '*ABS*' | sort > fd.map
	$(STRIP) fd

fd.b: fd
	$(ELFTOAOUT) -o fd.b fd
	echo -n 'SILO'$(IMGVERSION) | $(DD) of=fd.b bs=8 conv=notrunc seek=3 count=1
	echo -n -e '\340' | $(DD) of=fd.b bs=1 count=1 seek=7 conv=notrunc
	$(DD) if=/dev/zero of=fd.b bs=4 count=1 seek=255

ieee32.o: ieee32.S ieee32.h
	$(CC) $(CFLAGS) -DIMGVERSION='"SILO$(IMGVERSION)"' -c ieee32.S

ieee32.b: ieee32.o
	$(LD) $(LDFLAGS) -o ieee32.b ieee32.o
	$(NM) ieee32.b | grep -v '*ABS*' | sort > ieee32.map
	$(STRIP) ieee32.b
	chmod 644 ieee32.b

clean:
	$(RM) *.o *.map *.b first first.h ultra ultra.h \
	generic generic.h fd fd.h cd
