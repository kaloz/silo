/* First stage loader bootstrap for SUNW,Ultra's IEEE 1275 PROM
   
   Copyright (C) 1996,1997,2000 Jakub Jelinek
   
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#define BLOCK_START	0x1dc
#define promvec		%l5
#define buffer		%l0
#define fd		%l6
#define stdouth		%l7
#define dest		%l4

	.text
	.align 4
	.global _start, letter_here, number_here

_start:
	sethi	%hi(0x4000), buffer
	or	buffer, (BLOCK_START + 3 * 512), %i5
	sethi	%hi(0x10000), dest
	mov	%o4, promvec

	add	buffer, %lo(chosen), %o1
	call	prom11
	 add	buffer, %lo(finddevice), %o0
	ldx	[dest + 32], fd
	call	getprop
	 add	buffer, %lo(stdout), %o0
	lduw	[dest + 256], stdouth
	call	putchar
	 mov 	'S', %o0
	call	getprop
	 add	buffer, %lo(bootpath), %o0
	add	dest, 256, %o0
	mov	':', %o4
2:
	ldub	[%o0], %o3
	brz,pn	%o3, 3f
	 cmp	%o3, %o4
	bne,a,pt %icc, 2b
	 inc	%o0
3:
	stb	%o4, [%o0]
letter_here:
	mov	0xDD, %o4
	stb	%o4, [%o0 + 1]
	stb	%g0, [%o0 + 2]
	add	dest, 256, %o1
	call	prom11
	 add	buffer, %lo(open), %o0

	ldx	[dest + 32], fd
	or	buffer, BLOCK_START, %l1
	mov	%l1, %l2
5:
	lduw	[%l2], %l3
	add	%l2, 4, %l2
	brz,pn	%l3, 7f
	 sll	%l3, 9, %o3
	srl	%l3, 23, %o2
	call	rwprom31
	 add	buffer, %lo(seek), %o0
	ldx	[dest + 48], %o0
	brlz,pn	%o0, 9f
	 mov	512, %o3
	mov	%l1, %o2
	call	rwprom31
	 add	buffer, %lo(read), %o0
	ldx	[dest + 48], %o0
	cmp	%o0, 512
	bne,pn	%icc, 9f
	 cmp	%l1, %i5

	bne,pt	%xcc, 5b
	 add	%l1, 512, %l1

	call	putchar
	 mov	'I', %o0
	ba,pt	%xcc, 5b
	 add	dest, 1024, %l1
7:
	mov	fd, %o1
	call	prom11
	 add	buffer, %lo(close), %o0
	ldub	[dest + 1024 + 0x08], %o0
	cmp	%o0, 'L'
	bne,a,pn %icc, 10f
	 add	buffer, %lo(exit), %o0

	call	putchar
	 flush	dest + 1024

	mov	promvec, %o4
	jmpl	dest + 1024, %g0
number_here:
	 mov	0xFF, %l3
putchar:
	stb 	%o0, [buffer]
	mov	stdouth, %o1
	mov	buffer, %o2
	mov	1, %o3
	ba,pt	%xcc, prom31
	 add	buffer, %lo(write), %o0
9:
	add	buffer, %lo(exit), %o0
10:
	call	prom11
getprop:
	 stx	%o0, [dest + 32]
	add	dest, 256, %o0
	stx	%o0, [dest + 40]
	mov	1024, %o0
	stx	%o0, [dest + 48]
	mov	fd, %o1
	add	buffer, %lo(getprops), %o0
	mov	4, %o2
	stx	%o2, [dest + 8]
2:
	ba,pt	%xcc, 1f
prom11:
	 mov	1, %o2
	stx	%o2, [dest + 8]
1:
	stx	%o0, [dest]
	stx	%o2, [dest + 16]
	stx	%o1, [dest + 24]
	jmpl	promvec + %g0, %g0
	 mov	dest, %o0
rwprom31:
	mov	fd, %o1
prom31:
	mov	3, %o4
	stx	%o4, [dest + 8]
	stx	%o2, [dest + 32]
	ba,pt	%xcc, 2b
	 stx	%o3, [dest + 40]

finddevice:
	.asciz	"finddevice"
getprops:
	.asciz	"getprop"
bootpath:
	.asciz	"bootpath"
stdout:
	.asciz	"stdout"
open:
	.asciz	"open"
close:
	.asciz	"close"
seek:
	.asciz	"seek"
read:
	.asciz	"read"
write:
	.asciz	"write"
chosen:
	.asciz	"/chosen"
exit:
	.asciz	"exit"
