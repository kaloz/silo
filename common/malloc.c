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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#include <stringops.h>

#ifndef MALLOC_BASE
extern unsigned long _start;
static char *malloc_ptr = ((char *)&_start) + 0x30000;
#else
static char *malloc_ptr = (char *) MALLOC_BASE;
#endif

static char *last_alloc = 0;

static char *align_ptr_to(char *ptr, unsigned long align)
{
    return (char *) ((((unsigned long) ptr) + (align - 1UL)) &
		     ~(align - 1UL));
}

void *malloc (int size)
{
    char *caddr;

    caddr = malloc_ptr;
    malloc_ptr += size;
    last_alloc = caddr;
    malloc_ptr = align_ptr_to(malloc_ptr, 8UL);
    return caddr;
}

void *calloc (int nmemb, int memb_size)
{
    char *ret;
    int size;

    if (!nmemb || !memb_size)
        return (void *) 0;

    size = nmemb * memb_size;
    ret = malloc(size);

    if (ret)
        memset(ret, 0, size);

    return ret;
}

int posix_memalign(void **memptr, unsigned long alignment, unsigned long size)
{
    char *caddr;

    if (alignment & (alignment - 1UL))
        return -1;
    if (alignment & (sizeof(void *) - 1UL))
        return -1;

    if (size == 0) {
      *memptr = (void *) 0;
      return 0;
    }

    caddr = align_ptr_to(malloc_ptr, alignment);
    malloc_ptr = (caddr + size);
    last_alloc = caddr;
    malloc_ptr = align_ptr_to(malloc_ptr, 8UL);

    *memptr = caddr;

    return 0;
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
