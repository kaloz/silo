/* Dump memory allocation routines
   
   Copyright (C) 1996 Maurizio Plaza
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

#ifndef MALLOC_BASE
extern unsigned long _start;
static char *malloc_ptr = ((char *)&_start) + 0x30000;
#else
static char *malloc_ptr = (char *) MALLOC_BASE;
#endif

static char *last_alloc = 0;

void *malloc (int size)
{
    char *caddr;

    caddr = malloc_ptr;
    malloc_ptr += size;
    last_alloc = caddr;
    malloc_ptr = (char *) ((((unsigned long) malloc_ptr) + 7) & (~7));
    return caddr;
}

void free (void *m)
{
    if (m == last_alloc)
	malloc_ptr = last_alloc;
}

void mark (void **ptr)
{
    *ptr = (void *) malloc_ptr;
}

void release (void *ptr)
{
    malloc_ptr = (char *) ptr;
}
