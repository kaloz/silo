##
## Trivial Linux Loader for SPARC
##
##

include ../Rules.make

# Relocate to 3.75MB
# Hopefully, 4MB are guaranteed to be mapped.
LDFLAGS_SMALL=-N -Ttext 0x3c0000
# This is even more, but will be used only for extra large images.
LDFLAGS_LARGE=-N -Ttext 0x4c0000

all: maketilo

.c.o:
	$(CC) $(CFLAGS) -c $*.c

.S.o:
	$(CC) $(CFLAGS) -c $*.S

OBJS_COMMON = ../common/printf.o ../common/jmp.o ../common/prom.o \
	../common/tree.o ../common/console.o ../common/stringops1.o \
	../common/urem.o ../common/stringops2.o ../common/udiv.o \
	../common/divdi3.o ../common/udivdi3.o
OBJS = crt0.o tilo.o $(OBJS_COMMON) malloc.o
OBJS2 = crt0l.o tilol.o $(OBJS_COMMON) mallocl.o

maketilo: maketilo.c b.h b2.h
	$(HOSTCC) $(HOSTCFLAGS) -g maketilo.c -o maketilo

b.h : b.out $(BIN2H)
	$(BIN2H) -l BOOT_LEN boot_loader $< > $@ || ($(RM) $@; exit 1)

b2.h : b2.out $(BIN2H)
	$(BIN2H) -l LARGE_BOOT_LEN large_boot_loader $< > $@ || ($(RM) $@; exit 1)

b.out: $(OBJS)
	$(LD) $(LDFLAGS_SMALL) -Bstatic -o boot $(OBJS)
	$(ELFTOAOUT) boot -o b.out

b2.out: $(OBJS2)
	$(LD) $(LDFLAGS_LARGE) -Bstatic -o boot2 $(OBJS2)
	$(ELFTOAOUT) boot2 -o b2.out


mallocl.o: ../common/malloc.c
	$(CC) $(CFLAGS) -DMALLOC_BASE=0x4D0000 -c -o $@ $<

malloc.o: ../common/malloc.c
	$(CC) $(CFLAGS) -DMALLOC_BASE=0x3D0000 -c -o $@ $<

tilol.o: tilo.c
	$(CC) $(CFLAGS) -DLARGETILO -c -o $@ $<

crt0.o:	crt0.S
	$(CC) $(CFLAGS) -c -Wa,-Av9 -o $@ $<

crt0l.o: crt0.S
	$(CC) $(CFLAGS) -c -Wa,-Av9 -DLARGETILO -o $@ $<

clean:
	$(RM) *.o boot boot2 *.out b.h b2.h maketilo
