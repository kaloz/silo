##
## Linux Loader for SPARC
##
##

AS=as
AR=ar
NM=nm
DD=dd
AWK=awk
LS=ls
STRIP=strip
GZIP=gzip
GDB=gdb
ECHO=echo
GREP=grep
SED=sed
CP=cp

include ../Rules.make

# Relocate to 2.5MB. The Hitchhiker's Guide to Open Boot Rev 3 claims
# that 3MB are guaranteed to be mapped.
LDFLAGS_SMALL=-N -Ttext 0x280000
LDFLAGS_LARGE=-N -Ttext 0x380000

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $@

.S.o:
	$(CC) $(CFLAGS) -c $*.S -o $@

# The ordering here is very significant. Please add new object files to OBJS5 only.
OBJS1 = crt0.o
OBJS2 = decomp.o
OBJS2N = decompnet.o
OBJS3 = ../common/console.o ../common/printf.o malloc.o ../common/jmp.o \
	../common/prom.o ../common/tree.o ../common/urem.o ../common/udiv.o \
	../common/stringops1.o
OBJS4 = main.o
OBJS4N = mainnet.o
OBJS5 = cmdline.o disk.o file.o misc.o cfg.o strtol.o ranges.o timer.o \
	memory.o fs/libfs.a divdi3.o mul.o rem.o sdiv.o umul.o \
	../common/stringops2.o ls.o muldi3.o
OBJS = $(OBJS1) $(OBJS2) $(OBJS3) bmark.o $(OBJS4) $(OBJS5)
OBJSNET = $(OBJS1) $(OBJS2N) $(OBJS3) bmark.o $(OBJS4N) $(OBJS5)

FS_OBJS = fs/iom.o fs/ext2.o fs/isofs.o fs/romfs.o fs/ufs.o

ifeq (Linux,$(shell uname))
ifeq (sparc,$(subst sparc64,sparc,$(shell uname -m)))
# Should really switch to autoconf...
LIBINC=$(shell if [ ! -f /lib/ld-linux.so.2 -o ! -f /usr/lib/libext2fs.so ]; then echo -L../include; fi)
all: second.b silotftp.b
else
all:
	@$(ECHO) Second stage bootloader should be built under Linux/Sparc only
endif
else
all:
	@$(ECHO) Second stage bootloader should be build under Linux/Sparc only
endif

fs/libfs.a: $(FS_OBJS)
	$(RM) $@
	$(AR) rc $@ $(FS_OBJS)

second: $(OBJS) mark.o
	$(LD) $(LDFLAGS_SMALL) -Bstatic -o second $(OBJS) $(LIBINC) -lext2fs mark.o
	$(LD) $(LDFLAGS_LARGE) -Bstatic -o second2 $(OBJS) $(LIBINC) -lext2fs mark.o
	$(NM) second | grep -v '*ABS*' | sort > second.map

silotftp: $(OBJSNET) mark.o
	$(LD) $(LDFLAGS_SMALL) -Bstatic -o silotftp $(OBJSNET) $(LIBINC) -lext2fs mark.o
	$(LD) $(LDFLAGS_LARGE) -Bstatic -o silotftp2 $(OBJSNET) $(LIBINC) -lext2fs mark.o
	$(NM) silotftp | grep -v '*ABS*' | sort > silotftp.map

second.l: second
	( $(ECHO) 'disassemble 0x280000 '`$(GREP) '_etext' second.map | $(SED) 's/ .*$$//' | $(SED) 's/^00/0x/'`; $(ECHO) quit ) | $(GDB) second | $(SED) '1,/^(gdb)/d;/^End /,$$d' > second.l

file.o:	       file.c

decompnet.o:   decomp.c
	$(CC) $(CFLAGS) -DTFTP -c -o decompnet.o decomp.c
mainnet.o:   main.c
	$(CC) $(CFLAGS) -DTFTP -c -o mainnet.o main.c

malloc.o: ../common/malloc.c
	$(CC) $(CFLAGS) -c -o malloc.o ../common/malloc.c

util:	util.c
	$(CC) -o util util.c

clean:
	$(RM) *.o fs/*.o second* silotftp* *.map util fs/libfs.a

crt0.o:	crt0.S
	$(CC) $(CFLAGS) -c -Wa,-Av9 -DIMGVERSION='"SILO$(IMGVERSION)"' crt0.S

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

