VERSION=1.4.3
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

SMALL_RELOC=0x280000
LARGE_RELOC=0x380000

CFLAGS=-Os -Wall -I. -I../include -fomit-frame-pointer \
	-fno-strict-aliasing -DSMALL_RELOC=$(SMALL_RELOC) \
	-DLARGE_RELOC=$(LARGE_RELOC)

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
