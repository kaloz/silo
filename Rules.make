VERSION=1.2.0
IMGVERSION=0.99
RM=rm -f
CC=gcc
LD=ld
ELFTOAOUT=elftoaout
BIN2H=../common/bin2h
CFLAGS=-O2 -Wall -I. -I../include -fomit-frame-pointer

../common/%:
	$(MAKE) -C ../common $*
