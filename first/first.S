/* First stage loader bootstrap
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define BLOCK_START	0x1dc
#define promvec		%l5
#define promvers	%l4
#define buffer		%l0
#define fd		%l6
#define dest		%l7

	.text
	.align 4
	.global _start, digit_here, letter_here, number_here

_start:
	mov	%o0, promvec
	ld	[promvec+4], promvers
	sethi	%hi(0x4000), buffer
	sethi	%hi(0x10000), dest
	call	putchar
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
letter_here:
	mov	0xDD, %o4
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

	call	putchar
	 mov	'I', %o0
	b	5b
	 mov	dest, %l1
7:
	ldub	[dest + 0x08], %o0
	cmp	%o0, 'L'
	bne	9f
number_here:
	 mov	0xFF, %l3

	call	putchar
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
putchar:
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
