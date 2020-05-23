VERSION=1.4.14
IMGVERSION=0.99
SHELL=/bin/bash
RM=rm -f
# We want to force 32-bit builds
CC=gcc -m32
HOSTCC=gcc
LD=ld -m elf32_sparc
AS=as
STRIP=strip
NM=nm
ELFTOAOUT=elftoaout
BIN2H=../common/bin2h

SMALL_RELOC=0x280000
LARGE_RELOC=0x380000

cc-option-yn = $(shell if $(CC) $(CFLAGS) $(1) -S -o /dev/null -xc /dev/null \
		> /dev/null 2>&1; then echo "y"; else echo "n"; fi;)

CFLAGS = -Os -Wall -I. -I../include -fomit-frame-pointer \
	-fno-strict-aliasing -fno-PIC -U_FORTIFY_SOURCE \
	-DSMALL_RELOC=$(SMALL_RELOC) \
	-DLARGE_RELOC=$(LARGE_RELOC)
HOSTCFLAGS = -O2 -Wall

ifeq ($(call cc-option-yn, -fno-stack-protector),y)
CFLAGS += -fno-stack-protector
endif

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
