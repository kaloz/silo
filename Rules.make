VERSION=1.0
IMGVERSION=0.99
RM=/bin/rm -f
CC=gcc
LD=ld
ELFTOAOUT=elftoaout

../common/%:
	$(MAKE) -C ../common $*
