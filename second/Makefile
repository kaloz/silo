##
## Linux Loader for SPARC
##
##

CC=gcc 
LD=ld
AS=as
RM=/bin/rm -f
NM=nm
DD=dd
AWK=awk
LS=ls
ELFTOAOUT=elftoaout
STRIP=strip
GZIP=gzip
GDB=gdb
ECHO=echo
GREP=grep
SED=sed
CP=cp

CFLAGS = -I../include -O2 -I. -Wall -fomit-frame-pointer

# Relocate to 2.5MB. The Hitchhiker's Guide to Open Boot Rev 3 claims
# that 3MB are guaranteed to be mapped.
LDFLAGS=-N -Ttext 0x280000
LDFLAGS2=-N -Ttext 0x380000

.c.o:
	$(CC) $(CFLAGS) -c $*.c

.S.o:
	$(CC) $(CFLAGS) -c $*.S

# The ordering here is very significant. Please add new object files to OBJS5 only.
OBJS1 = crt0.o
OBJS2 = decomp.o
OBJS2N = decompnet.o
OBJS3 = console.o printf.o malloc.o jmp.o prom.o tree.o urem.o udiv.o stringops1.o
OBJS4 = main.o
OBJS4N = mainnet.o
OBJS5 = cmdline.o disk.o file.o misc.o cfg.o strtol.o ranges.o timer.o memory.o ufs.o romfs.o 9660.o divdi3.o \
	mul.o rem.o sdiv.o umul.o stringops2.o ls.o muldi3.o
OBJS = $(OBJS1) $(OBJS2) $(OBJS3) bmark.o $(OBJS4) $(OBJS5)
OBJSNET = $(OBJS1) $(OBJS2N) $(OBJS3) bmark.o $(OBJS4N) $(OBJS5)

ifeq (Linux,$(shell uname))
ifeq (sparc,$(subst sparc64,sparc,$(shell uname -m)))
# Should really switch to autoconf...
LIBINC=$(shell if [ ! -f /lib/ld-linux.so.2 -o ! -f /usr/lib/libext2fs.so ]; then echo -L../include; fi)
all: second.b silotftp.b
else
all:
	@$(ECHO) Second stage bootloader should be build under Linux/Sparc only
endif
else
all:
	@$(ECHO) Second stage bootloader should be build under Linux/Sparc only
endif

second: $(OBJS) mark.o
	$(LD) $(LDFLAGS) -Bstatic -o second $(OBJS) $(LIBINC) -lext2fs mark.o
	$(LD) $(LDFLAGS2) -Bstatic -o second2 $(OBJS) $(LIBINC) -lext2fs mark.o
	$(NM) second | grep -v '*ABS*' | sort > second.map

silotftp: $(OBJSNET) mark.o
	$(LD) $(LDFLAGS) -Bstatic -o silotftp $(OBJSNET) $(LIBINC) -lext2fs mark.o
	$(LD) $(LDFLAGS2) -Bstatic -o silotftp2 $(OBJSNET) $(LIBINC) -lext2fs mark.o
	$(NM) silotftp | grep -v '*ABS*' | sort > silotftp.map

second.l: second
	( $(ECHO) 'disassemble 0x280000 '`$(GREP) '_etext' second.map | $(SED) 's/ .*$$//' | $(SED) 's/^00/0x/'`; $(ECHO) quit ) | $(GDB) second | $(SED) '1,/^(gdb)/d;/^End /,$$d' > second.l

main.o:        main.c silo.h
ls.o:          ls.c silo.h
malloc.o:      malloc.c silo.h
printf.o:      printf.c silo.h
cmdline.o:     cmdline.c silo.h
console.o:     console.c silo.h
misc.o:	       misc.c silo.h
disk.o:	       disk.c silo.h
file.o:	       file.c silo.h ufs.h
cfg.o:	       cfg.c silo.h
ranges.o:      ranges.c silo.h
tree.o:	       tree.c silo.h
timer.o:       timer.c silo.h
decomp.o:      decomp.c inflate.c silo.h
mark.o:	       mark.S
bmark.o:       bmark.S
mul.o:         mul.S
umul.o:        umul.S
sdiv.o:        sdiv.S
udiv.o:        udiv.S
rem.o:         rem.S
urem.o:        urem.S
ufs.o:	       ufs.c silo.h ufs.h
romfs.o:       romfs.c silo.h
memory.o:      memory.c silo.h
jmp.o:	       jmp.S
divdi3.o:      divdi3.S
prom.o:	       prom.c promlib.h
decompnet.o:   decomp.c inflate.c silo.h
	$(CC) $(CFLAGS) -DTFTP -c -o decompnet.o decomp.c
mainnet.o:   main.c silo.h
	$(CC) $(CFLAGS) -DTFTP -c -o mainnet.o main.c

util:	util.c
	$(CC) -o util util.c

clean:
	$(RM) $(OBJS) *.o second second2 second.b silotftp silotftp2 silotftp.b second.l *.map core *~ util second.b second.image.tmp *.s

crt0.o:	crt0.S
	$(CC) $(CFLAGS) -c -Wa,-Av9 -DIMGVERSION='"SILO$(IMGVERSION)"' crt0.S

prom.o: prom.c
	$(CC) $(CFLAGS) -c -Wa,-Av9 prom.c

memory.o: memory.c
	$(CC) $(CFLAGS) -c -Wa,-Av9 memory.c

timer.o: timer.c
	$(CC) $(CFLAGS) -c -Wa,-Av9a timer.c

second.b: second util
	$(CP) second second.tmp
	$(CP) second2 second2.tmp
	$(STRIP) second.tmp
	$(STRIP) second2.tmp
	$(ELFTOAOUT) -o second.image.tmp second.tmp
	$(ELFTOAOUT) -o second2.image.tmp second2.tmp
	./util -z second.map second.image.tmp second2.image.tmp second.b second.b2
	$(GZIP) -9cfn second.b2 >> second.b
	$(DD) if=/dev/zero bs=1 count=`$(LS) -l second.b | $(AWK) '{len=$$5%512;if(len!=0)len=512-len;print len}'` >> second.b
	$(LS) -l second.b | $(AWK) '{printf "%c%c%c%c",0,$$5/65536,($$5%65536)/256,0}' | $(DD) of=second.b bs=4 count=1 seek=581 conv=notrunc
	$(RM) second.tmp second.image.tmp second2.tmp second2.image.tmp second.b2

silotftp.b: silotftp util
	$(CP) silotftp silotftp.tmp
	$(CP) silotftp2 silotftp2.tmp
	$(STRIP) silotftp.tmp
	$(STRIP) silotftp2.tmp
	$(ELFTOAOUT) -o silotftp.image.tmp silotftp.tmp
	$(ELFTOAOUT) -o silotftp2.image.tmp silotftp2.tmp
	./util -a silotftp.map silotftp.image.tmp silotftp2.image.tmp silotftp.b silotftp.b2
	$(GZIP) -9cfn silotftp.b2 >> silotftp.b
	$(RM) silotftp.tmp silotftp.image.tmp silotftp2.tmp silotftp2.image.tmp silotftp.b2

