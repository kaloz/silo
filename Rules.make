VERSION=1.1.0
IMGVERSION=0.99
RM=rm -f
CC=gcc
LD=ld
ELFTOAOUT=elftoaout
BIN2H=../common/bin2h
CFLAGS=-O2 -Wall -I../include -fomit-frame-pointer

../common/%:
	$(MAKE) -C ../common $*
