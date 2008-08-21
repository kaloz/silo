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
#
# XXX: We seem to be going 4Megs here for LARGE? -- BenC
LDFLAGS_SMALL=-N -Ttext $(SMALL_RELOC)
LDFLAGS_LARGE=-N -Ttext $(LARGE_RELOC)

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
	../common/stringops1.o ../common/ffs.o ../common/divdi3.o \
	../common/udivdi3.o
OBJS4 = main.o
OBJS4N = mainnet.o
OBJS5 = cmdline.o disk.o file.o misc.o cfg.o strtol.o ranges.o timer.o \
	memory.o fs/libfs.a mul.o ../common/rem.o ../common/sdiv.o umul.o \
	../common/stringops2.o ls.o muldi3.o
OBJS = $(OBJS1) $(OBJS2) $(OBJS3) bmark.o $(OBJS4) $(OBJS5)
OBJSNET = $(OBJS1) $(OBJS2N) $(OBJS3) bmark.o $(OBJS4N) $(OBJS5)

FS_OBJS = fs/iom.o fs/ext2.o fs/isofs.o fs/romfs.o fs/ufs.o

# Should really switch to autoconf...
all: second.b silotftp.b

fs/libfs.a: $(FS_OBJS)
	$(RM) $@
	$(AR) rc $@ $(FS_OBJS)

second: $(OBJS) mark.o
	$(LD) $(LDFLAGS_SMALL) -Bstatic -o second $(OBJS) -lext2fs mark.o `$(CC) -print-libgcc-file-name`
	$(LD) $(LDFLAGS_LARGE) -Bstatic -o second2 $(OBJS) -lext2fs mark.o `$(CC) -print-libgcc-file-name`
	$(NM) second | grep -v '*ABS*' | sort > second.map

silotftp: $(OBJSNET) mark.o
	$(LD) $(LDFLAGS_SMALL) -Bstatic -o silotftp $(OBJSNET) -lext2fs mark.o `$(CC) -print-libgcc-file-name`
	$(LD) $(LDFLAGS_LARGE) -Bstatic -o silotftp2 $(OBJSNET) -lext2fs mark.o `$(CC) -print-libgcc-file-name`
	$(NM) silotftp | grep -v '*ABS*' | sort > silotftp.map

second.l: second
	( $(ECHO) 'disassemble $(SMALL_RELOC) '`$(GREP) '_etext' second.map | $(SED) 's/ .*$$//' | $(SED) 's/^00/0x/'`; $(ECHO) quit ) | $(GDB) second | $(SED) '1,/^(gdb)/d;/^End /,$$d' > second.l

file.o:	       file.c

decompnet.o:   decomp.c
	$(CC) $(CFLAGS) -DTFTP -DVERSION='"$(VERSION)"' -c -o $@ $<

decomp.o: decomp.c
	$(CC) $(CFLAGS) -DVERSION='"$(VERSION)"' -c $<

mainnet.o:   main.c
	$(CC) $(CFLAGS) -DTFTP -c -o $@ $<

malloc.o: ../common/malloc.c
	$(CC) $(CFLAGS) -c -o $@ $<

util:	util.c
	$(CC) -DSMALL_RELOC=$(SMALL_RELOC) -DLARGE_RELOC=$(LARGE_RELOC) -o $@ $<

clean:
	$(RM) *.o fs/*.o second* silotftp* util fs/libfs.a

crt0.o:	crt0.S
	$(CC) $(CFLAGS) -c -Wa,-Av9 -DIMGVERSION='"SILO$(IMGVERSION)"' crt0.S

memory.o: memory.c
	$(CC) $(CFLAGS) -c -Wa,-Av9 memory.c

timer.o: timer.c
	$(CC) $(CFLAGS) -c -Wa,-Av9a timer.c

second.b: second util
	$(ELFTOAOUT) -o second.aout second
	$(ELFTOAOUT) -o second2.aout second2
	./util second.map second.aout second2.aout second.b second.b2
	$(GZIP) -9cfn second.b2 >> second.b
	$(DD) if=/dev/zero bs=1 count=`$(LS) -l second.b | $(AWK) '{len=$$5%512;if(len!=0)len=512-len;print len}'` >> second.b
	$(LS) -l second.b | $(AWK) '{printf "%c%c%c%c",0,$$5/65536,($$5%65536)/256,0}' | $(DD) of=second.b bs=4 count=1 seek=581 conv=notrunc

silotftp.b: silotftp util
	$(ELFTOAOUT) -o silotftp.aout silotftp
	$(ELFTOAOUT) -o silotftp2.aout silotftp2
	./util -a silotftp.map silotftp.aout silotftp2.aout silotftp.b silotftp.b2
	$(GZIP) -9cfn silotftp.b2 >> silotftp.b
