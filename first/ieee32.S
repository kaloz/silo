/* First stage loader bootstrap for flash of a JavaStation.

   Copyright (C) 1996,1997,2000 Jakub Jelinek
   Copyright (C) 1999,2000 Pete Zaitcev

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define BLOCK_START	0x300	/* We are not a.out, so can use anything */
#include "ieee32.h"		/* IEEE32_OFFSET */
#define LOAD_OFF	1024	/* Skip PROM interface buffer */

#define buffer		%l3
#define dest		%l4
#define promvec		%l5
#define fd		%l6
#define stdouth		%l7

	.text
	.align 4
	.global _start

_start:
	ba	1f
	 sethi	%hi(0x4000), buffer
	.ascii	VERSION
1:	sethi	%hi(0x10000), dest
	mov	%o3, promvec

	add	buffer, %lo(chosen), %o1
	call	prom11
	 add	buffer, %lo(finddevice), %o0
	ld	[dest + 16], fd

	call	getprop
	 add	buffer, %lo(stdout), %o0
	ld	[dest + 256], stdouth

	call	putchar
	 mov 	'S', %o0

	/*
	 * Get phandle of flash memory.
	 * Unlike other first stage loaders
	 * we do not use "/chosen"."bootpath" value.
	 * It's a feature, not a bug.
	 */
	add	buffer, %lo(flashs), %o1
	call	prom11
	 add	buffer, %lo(finddevice), %o0
	ld	[dest + 16], fd		/* phandle of /flash-memory */

	/*
	 * Get "reg". We use it to find the base address,
	 * which is different for 4MB, 8MB and 16MB SIMMs.
	 */
	call	getprop
	 add	buffer, %lo(regs), %o0
	ld	[dest + 260], %i2	/* 20400000 for 8MB SIMM */

	/*
	 * Load 4 blocks of second stage loader with the map,
	 * then all other blocks at 0x10000 (dest).
	 * First of them will have an 'L' header embedded.
	 */
	or	buffer, BLOCK_START, %l1
	add	%l1, (3 * 512), %i5	/* Will read 4 blocks of block map */
	mov	%l1, %l2
	/*
	 * But before we start the loop, get the block 0 number.
	 * In other loaders we get it for free when 16 blocks of
	 * bootstrap are loaded. In flash it is outside of ELF.
	 */
	add	%i2, IEEE32_OFFSET, %i4
	lda	[%i4] 0x20, %i3
	st	%i3, [%l2]

5:
	ld	[%l2], %i3
	subcc	%i3, 0, %g0
	add	%l2, 4, %l2
	bz	7f
	 sll	%i3, 9, %o3		/* Convert blocks into bytes */
	/* srl	%i3, 23, %o2 */		/* %o2 and %o3 make an 41 bits offset */

	/*
	 * blkblt
	 * We use ASI=20 (BYPASS) to spare remapping work. Lazy, but works.
	 * We do not unroll the loop because flash is slow and RAM is tight.
	 */
	mov	512, %i0		/* running cnt */
	mov	%l1, %i1		/* running dest */
	add	%i2, %o3, %i4		/* running source (physical addr) */
	/* We conviniently ignore upper bits of offset for flash is small. */
1:
	lda	[%i4] 0x20, %i3
	st	%i3, [%i1]
	add	%i4, 4, %i4
	add	%i1, 4, %i1
	subcc	%i0, 4, %i0
	bne	1b
	 nop

	cmp	%l1, %i5
	bne	5b
	 add	%l1, 512, %l1

	call	putchar
	 mov	'I', %o0
	b	5b
	 add	dest, LOAD_OFF, %l1

7:
	ldub	[dest + LOAD_OFF + 0x08], %o0
	cmp	%o0, 'L'
	bne	exit
	 nop
	call	putchar
	 nop
	/* flush dest + LOAD_OFF */  /* XXX Write-through cache on IIep */

	clr	%o4			/* Important - not Ultra */
	or	promvec, 1, %o0		/* XXX Gross... */
	clr	%o3			/* Hmm... */
	jmpl	dest + LOAD_OFF, %g0
	 clr	%l3

exit:
	add	buffer, %lo(exits), %o0
	call	prom11
	 st	%o0, [dest + 16]

/*
 * getprop(node:fd, name:%o0) {
 *   cif_handler:promvec("getprops", node, name, dest+256, 1024);
 * }
 */
getprop:
	add	buffer, %lo(getprops), %o2
	st	%o2, [dest]
	mov	4, %o2
	st	%o2, [dest + 4]
	mov	1, %o2
	st	%o2, [dest + 8]
	st	fd, [dest + 12]
	st	%o0, [dest + 16]
	add	dest, 256, %o0
	st	%o0, [dest + 20]
	mov	1024, %o0
	st	%o0, [dest + 24]
	jmpl	promvec + %g0, %g0
	 mov	dest, %o0

/*
 */
putchar:
	stb 	%o0, [buffer]
	mov	stdouth, %o1
	mov	buffer, %o2
	mov	1, %o3
	ba	prom31			/* one of many tail recustion -O's */
	 add	buffer, %lo(write), %o0

/*
 * One in, one out.
 */
prom11:
	st	%o0, [dest]
	mov	1, %o2
	st	%o2, [dest + 4]
	st	%o2, [dest + 8]
	st	%o1, [dest + 12]
	jmpl	promvec + %g0, %g0
	 mov	dest, %o0

/*
 * Three in, one out.
 */
prom31:
	st	%o0, [dest]
	mov	3, %o4
	st	%o4, [dest + 4]
	mov	1, %o4
	st	%o4, [dest + 8]
	st	%o1, [dest + 12]
	st	%o2, [dest + 16]
	st	%o3, [dest + 20]
	jmpl	promvec + %g0, %g0
	 mov	dest, %o0

finddevice:
	.asciz	"finddevice"
getprops:
	.asciz	"getprop"
stdout:
	.asciz	"stdout"
write:
	.asciz	"write"
chosen:
	.asciz	"/chosen"
exits:
	.asciz	"exit"
flashs:
	.asciz	"/flash-memory"
regs:
	.asciz	"reg"
