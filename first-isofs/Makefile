##
## CD Linux Loader for SPARC
##

include ../Rules.make

LDFLAGS = -N -Ttext 0x4000

NAME = isofs

all: $(NAME).b

.c.o:
	$(CC) $(CFLAGS) -c $*.c

.S.o:
	$(CC) $(CFLAGS) -c $*.S

OBJS_COMMON = ../common/prom.o ../common/console.o ../common/tree.o
OBJS = crt0.o $(NAME).o $(OBJS_COMMON)

$(NAME): $(OBJS)
	$(LD) $(LDFLAGS) -Bstatic -o $@ $(OBJS)

$(NAME).b: $(NAME)
	dd if=/dev/zero bs=512 count=1 of=$@
	$(ELFTOAOUT) $< -o $<.aout || ($(RM) $@ && exit 1)
	cat $<.aout >> $@

crt0.o:	crt0.S
	$(CC) $(CFLAGS) -c -Wa,-Av9 -o $@ $<

clean:
	$(RM) *.o $(NAME) $(NAME).b $(NAME).aout
