/* setjmp/longjmp code
   
   Copyright (C) 1996 Miguel de Icaza
		 1996 Jakub Jelinek
   
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

#define _SV save %sp, -0x40, %sp
#define _RV restore
#define FLUSH_ALL_WINDOWS \
	_SV; _SV; _SV; _SV; _SV; _SV; _SV; \
	_RV; _RV; _RV; _RV; _RV; _RV; _RV;

	/* Save our return PC and SP.  */
	.global __setjmp, setjmp
setjmp:
__setjmp:
	st %o7, [%o0]
	st %sp, [%o0 + 4]
	retl
	clr %o0

	.global __longjmp, longjmp
longjmp:
__longjmp:
	/* Flush register windows.
	   Write all the register windows to their stack slots, and
	   mark them all as invalid (needing to be sucked up from the
	   stack when used).  This ensures that all information needed to
	   unwind to these callers is in memory, not in the register
	   windows.  */
	FLUSH_ALL_WINDOWS
	ld [%o0], %o7		/* Return PC.  */
	ld [%o0 + 4], %fp	/* Saved SP.  */
	sub %fp, 64, %sp	/* Allocate a register save area.  */
	tst %o1
	be,a 1f
	mov 1, %o1
1:	retl
	/* On the way out, put the return value in %o0.  */
	restore %o1, 0, %o0
