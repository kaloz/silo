VERSION=1.3.2
IMGVERSION=0.99
RM=rm -f
# We want to force 32-bit builds
CC=gcc -m32
LD=ld
AS=as
STRIP=strip
NM=nm
ELFTOAOUT=elftoaout
BIN2H=../common/bin2h
CFLAGS=-Os -Wall -I. -I../include -fomit-frame-pointer -fno-strict-aliasing

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
