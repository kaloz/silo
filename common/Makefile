##
## Common files between tilo and silo
##

include ../Rules.make

.c.o:
	$(CC) $(CFLAGS) -c $*.c

.S.o:
	$(CC) $(CFLAGS) -c $*.S

OBJS = udiv.o urem.o jmp.o printf.o console.o prom.o tree.o stringops2.o stringops1.o
PROGRAMS = bin2h

all: $(OBJS) $(PROGRAMS)

prom.o: prom.c
	$(CC) $(CFLAGS) -c -Wa,-Av9 -o prom.o prom.c

clean:
	$(RM) $(PROGRAMS) *.o
