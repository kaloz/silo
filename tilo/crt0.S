/* Trivial lInux LOader bootstrap
   
   Copyright (C) 1996,1997 Jakub Jelinek
		 1998 Jan Vondrak
   
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

#define COPY	jmpl	%o7 + (copy - _start), %l7

#ifndef LARGETILO
#define STACK_BASE 0x400000
#else
#define STACK_BASE 0x500000
#endif

	.text
	.global _start, image_table
	.align	16
_start:
	call	1f
	 mov	%o7, %l0

image_table:
	.skip   64
		
		/*
		struct	{
			int packed_start;
			int packed_len;
			int unpacked_len;
			int root_start;
			}
			image_table[4];
		*/

1:
	/* Move ourselves up */			! %l0 = original start
	set	__bss_start + 16, %i1		! %i1 = original end
	set	_start, %l1			! %i0 = %l1 = relocated start
	mov	%l1, %i0			! %l2 = code length
	COPY
	 sub	%i1, %l1, %l2

	/* Jump to relocated code */
	sethi	%hi(_end), %l0
	jmpl	%i0 + (jumphere - _start), %g0
	 or	%l0, %lo(_end), %l0		! %l0 = _end
jumphere:
	/* Clear BSS */
5:	andcc	%i1, 4, %g0
	sub	%l0, %i1, %i2			! %i2 = BSS length
	be	1f
	 mov	%g0, %g1

	st	%g0, [%i1]
	add	%i1, 4, %i1
1:
	subcc	%i2, 32, %i2
	std	%g0, [%i1]
	std	%g0, [%i1 + 8]
	std	%g0, [%i1 + 16]
	std	%g0, [%i1 + 24]
	bgu	1b
	 add	%i1, 32, %i1

	tst	%o4				! if not Ultra,
	be	1f				! skip %pstate modification
	 nop

	rdpr	%pstate, %l1
	or	%l1, 8, %l1
	wrpr	%l1, 0, %pstate
1:

	! Set up a stack
setup_stack:
	set	STACK_BASE, %l1
	save	%l1, -64, %sp

	! Call tilo_main() to start the whole thingie up

	call	flush_icache
	 mov	%i6, %o2			! Cif sp on sun4u
	mov	%i4, %o1			! Cif handler on sun4u
	call	tilo_main
	 mov	%i0, %o0			! Prom vector or cifh on sun4m
	call	flush_icache
	 nop
	jmpl	%o0 + %g0, %g0			! Jump to return address from tilo_main()
	 restore

/* l0 from, l1 to, l2 len, l7 ret. Returns from + len in l0 */
copy:
	ld	[%l0], %l3
	ld	[%l0 + 4], %l4
	ld	[%l0 + 8], %l5
	ld	[%l0 + 12], %l6
	subcc	%l2, 16, %l2
	add	%l0, 16, %l0
	st	%l3, [%l1]
	st	%l4, [%l1 + 4]
	st	%l5, [%l1 + 8]
	st	%l6, [%l1 + 12]
	bgu	copy
	 add	%l1, 16, %l1

	jmpl	%l7 + 8, %g0
	 nop

flush_icache:
	tst	%i4			/* quit unless it's Sun4u */
	be	0f
	 nop
	rdpr	%ver, %l0
	sethi	%hi(0x003e0014), %l1
	srlx	%l0, 32, %l0
	or	%l1, %lo(0x003e0014), %l1
	cmp	%l0, %l1
	be	0f			/* quit if it is Ultra-III */
	 nop
	clr	%l0
	sethi	%hi(16384), %l1
	stxa	%g0, [%l0] 0x67		/* ASI_IC_TAG */
1:
	add	%l0, 32, %l0
	cmp	%l0, %l1
	blu,a,pt %xcc, 1b
	 stxa	%g0, [%l0] 0x67		/* ASI_IC_TAG */
0:
	retl
	 nop
