VERSION=1.2.6
IMGVERSION=0.99
RM=rm -f
CC=gcc
LD=ld
ELFTOAOUT=elftoaout
BIN2H=../common/bin2h
CFLAGS=-O2 -Wall -I. -I../include -fomit-frame-pointer

OPSYS=$(shell uname)
OSREV=$(shell uname -r)
ifeq ($(OPSYS),SunOS)
  ifeq (5.,$(findstring 5.,$(OSREV)))
    OPSYS=Solaris
  endif
endif
MACHINE=$(subst sparc64,sparc,$(shell uname -m))

../common/%:
	$(MAKE) -C ../common $*
