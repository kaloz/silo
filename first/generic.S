/* First stage loader bootstrap for both v0, v2 and SUNW,Ultra's IEEE 1275 PROM
   
   Copyright (C) 1996,1997,1998,2000 Jakub Jelinek
   
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

#define BLOCK_START	0x3dc
#define promvec		%l5
#define buffer		%l0
#define fd		%l6
#define stdouth		%l7
#define promvers	%l7
#define dest		%l4

	.text
	.align 4
	.global _start, letter_here, digit_here, number_here

_start:
bootmain:
	tst	%o4
	be	sparc_v8
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

	.align	512
sparc_v8:
	mov	%o0, promvec
	ld	[promvec+4], promvers
	sethi	%hi(0x4000), buffer
	sethi	%hi(0x10000), dest
	call	putchar_v8
	 mov 	'S', %o0

	tst	promvers
	bne,a	1f
	 ld	[promvec+136], %o0		! pv_v2bootargs.bootpath

	ld	[promvec+128], %o0
	ld	[%o0], %o3			! o3 = *pv_v0bootargs
	lduh	[%o3+132], %o1			! 'sd', 'fd' or whatever
	sll	%o1, 8, %o1
	or	%o1, '(', %o1
	sll	%o1, 8, %o1
	ld	[%o3+136], %o0			! controler id (hope it is less than 10)
	add	%o0, '0', %o0
	or	%o1, %o0, %o2
	mov	',', %o1
	sll	%o1, 8, %o1
	ld	[%o3+140], %o4			! unit id (again, hope nobody uses wide on v0 prom)
	add	%o4, '0', %o4
	or	%o4, %o1, %o4
	sll	%o4, 16, %o4
	or	%o4, %o1, %o4
digit_here:
	or	%o4, 0xDD, %o3
	std	%o2, [buffer + 0x400]		! put the 'sd(i,j,k' somewhere...
	mov	')', %o2
	sll	%o2, 8, %o2
	sth	%o2, [buffer + 0x408]		! put there the rest ')\0' as well...
	b	4f
	ld	[promvec+36], %o1		! pv_v0devops.v0_devopen
1:
	ld	[promvec+172], %o1
	ld	[%o0], %o0
	add	buffer, 0x400, %o2
	mov	':', %o4
2:
	ldub	[%o0], %o3
	tst	%o3
	be	3f
	 cmp	%o3, %o4

	be	3f
	 stb	%o3, [%o2]
	add	%o2, 1, %o2
	b	2b
	 add	%o0, 1, %o0
3:
	stb	%o4, [%o2]
	ldub	[buffer + (letter_here - _start + 3)], %o4
	stb	%o4, [%o2 + 1]
	stb	%g0, [%o2 + 2]
4:
	call	%o1
	 add	buffer, 0x400, %o0

	mov	%o0, fd
	or	buffer, BLOCK_START, %l1
	mov	%l1, %l2
5:
	ld	[%l2], %l3
	add	%l2, 4, %l2
	tst	%l3
	be	7f
	 tst	promvers

	bne	6f
	 ld	[promvec+44], %o4		! pv_v0devops.v0_rdblkdev

	mov	1, %o1
	mov	%l3, %o2
	mov	%l1, %o3
	call	%o4
	 mov	fd, %o0
	cmp	%o0, 1
	b,a	0f
6:
	ld	[promvec+188], %o4		! pv_v2devops.v2_dev_seek
	mov	%g0, %o1
	sll	%l3, 9, %o2
	call	%o4
	 mov	fd, %o0
	cmp	%o0, -1
	be	9f
	 ld	[promvec+180], %o4		! pv_v2devops.v2_dev_read

	mov	512, %o2
	mov	%l1, %o1
	call	%o4
	 mov	fd, %o0
	cmp	%o0, 512
0:
	bne	9f
	 cmp	%l1, %i5

	bne	5b
	 add	%l1, 512, %l1

	call	putchar_v8
	 mov	'I', %o0
	b	5b
	 mov	dest, %l1
7:
	ldub	[dest + 0x08], %o0
	cmp	%o0, 'L'
	bne	9f
	 ldub	[buffer + (number_here - _start + 3)], %l3

	call	putchar_v8
	 ld	[promvec+176], %l1		! pv_v2devops.v2_dev_close

	tst	promvers
	be,a	8f
	 ld	[promvec+40], %l1		! pv_v0devops.v0_dev_close
8:
	call	%l1
	 mov	fd, %o0
	clr	%o4				! we are not an Ultra
	jmpl	dest + %g0, %g0
	 mov	promvec, %o0
9:
	ld	[promvec+116], %g1
	jmpl	%g1 + %g0, %g0
putchar_v8:
	tst	promvers
	bne	1f
	 ld	[promvec+92], %o1		! pv_nbputchar

	jmpl 	%o1 + %g0, %g0
	 or	buffer, (BLOCK_START + 3 * 512), %i5
1:
	stb 	%o0, [buffer]
	ld 	[promvec+148], %o0		! pv_v2bootargs.fd_stdout
	ld 	[promvec+184], %o3		! pv_v2devops.v2_dev_write
	mov	buffer, %o1
	ld	[%o0], %o0
	mov	1, %o2
	jmpl 	%o3 + %g0, %g0
	 or	buffer, (BLOCK_START + 3 * 512), %i5
