/* Second stage boot loader bootstrap
   
   Copyright (C) 1996 Pete A. Zaitcev
		 1996 Miguel de Icaza
		 1996,1997,1998,2000 Jakub Jelinek
   
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

#define COPY	jmpl	%o7 + (copy - start), %l7

	.text
	.global _start, start, silo_conf, silo_conf_part, boot_part
	.global raid_dsk_number, silo_conf_parts, boot_parts
_start:
start:
	call	1f
	 mov	%o7, %l0

	.byte	'L'	! This is for the primary loader to check if we are ok... :)
silo_conf_part:
	.byte 	0
boot_part:
	.byte 	0
raid_dsk_number:
	.byte	0
	.align 4
silo_conf:
	.skip  256
	.ascii	VERSION
imglen:	.word	0
silo_conf_parts:
	.skip	32
boot_parts:
	.skip	32
1:	/* First store %l3 to raid_dsk_number */
	stb	%l3, [%l0 + (raid_dsk_number - _start)]
	/* Move ourselves up - there are three chunks to move. */
	clr	%i3
	tst	%o4
	bne,a	1f
	 sethi	%hi(0x100000), %i3
1:	set	main_text_start, %l3
	set	start, %l4
	add	%l4, %i3, %i0
	mov	%i0, %l1
	COPY
	 sub	%l3, %l4, %l2

	set	main_rodata_start, %l3
	set	main_text_end, %l4
	add	%l4, %i3, %l1
	COPY
	 sub	%l3, %l4, %l2

	set	main_data_start, %l3
	set	main_rodata_end, %l4
	add	%l4, %i3, %l1
	COPY
	 sub	%l3, %l4, %l2

	set	__bss_start + 16, %i1
	set	main_data_end, %l4
	add	%l4, %i3, %l1
	COPY
	 sub	%i1, %l4, %l2

	sub	%l0, 16, %l0
	sethi	%hi(gzminpi), %l1
	add	%l1, %i3, %l1
	st	%l0, [%l1 + %lo(gzminpi)]
#if 0
	add	%i0, (jumphere - start), %l0
	andn	%l0, 0xfff, %l0
	sethi	%hi(_etext), %l1
	or	%l1, %lo(_etext), %l1
	add	%l1, %i3, %l1
	sethi	%hi(0x1000), %l2
1:
	flush	%l0
	cmp	%l0, %l1
	blu	1b
	 add	%l0, %l2, %l0
#endif

	/* Jump to relocated code */
	sethi	%hi(_end), %l0
	jmpl	%i0 + (jumphere - start), %g0
	 or	%l0, %lo(_end), %l0

	.align	32
jumphere:
	/* Clear BSS */
	add	%l0, %i3, %l0
	add	%i1, %i3, %i1
	andcc	%i1, 4, %g0
	sub	%l0, %i1, %i2			! length
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

	tst	%o4
	be	1f
	 nop
#if 1
	rdpr	%pstate, %l1
	or	%l1, 8, %l1
	wrpr	%l1, 0, %pstate
#else
	nop; nop; nop
#endif
1:

	! Set up a stack
setup_stack:
	save	%i0,-120,%sp

	tst	%i4
	be	0f
	 sethi	%hi(gzminpi+0x100000), %l0
	ld	[%l0 + %lo(gzminpi)], %l1
	sethi	%hi(0x380000), %l2
1:	lduh	[%l1], %l3
	add	%l1, 2, %l1
	tst	%l3
	be	2f
	 add	%l2, %l3, %l2
	ldub	[%l2], %l3
	add	%l3, 4, %l3
	ba	1b
	 stb	%l3, [%l2]
2:	sethi	%hi(0x380000), %l2
3:	lduh	[%l1], %l3
	add	%l1, 2, %l1
	tst	%l3
	be	4f
	 add	%l2, %l3, %l2
	ldub	[%l2], %l3
	add	%l3, 16, %l3
	ba	3b
	 stb	%l3, [%l2]
4:	st	%l1, [%l0 + %lo(gzminpi)]

	! Call gunzipmain() to start the whole thingie up

0:	call	flush_icache
	 mov	%i6, %o2			! Cif sp on sun4u
	mov	%i4, %o1			! Cif handler on sun4u
	call	gunzipmain
	 mov	%i0, %o0			! Prom vector or cifh on sun4m
	call	flush_icache
	 nop
	jmpl	%o0 + %g0, %g0			! Jump to return address from gunzipmain
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
	tst	%i4
	be	0f
	 clr	%l0
	sethi	%hi(16384), %l1
	stxa	%g0, [%l0] 0x67
1:
	add	%l0, 32, %l0
	cmp	%l0, %l1
	blu,a,pt %xcc, 1b
	 stxa	%g0, [%l0] 0x67
0:
	retl
	 nop

	.section	".rodata"
	.word	_start
