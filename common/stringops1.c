/* Slooow, but small string operations, so that we don't have
   to link libc5/glibc in.
   These are needed before the second part is uncompressed.
   Originally from linux/lib/string.c, which is 
   	Copyright (C) 1991, 1992  Linus Torvalds
 */
 
#include <stringops.h>

char *strcpy(char *dest, const char *src)
{
	char *tmp = dest;
	while ((*dest++ = *src++) != '\0');
	return tmp;
}

int strcmp(const char *cs,const char *ct)
{
	register signed char __res;
	while (1)
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
	return __res;
}

void *memset(void *s,int c,size_t count)
{
	char *xs = (char *) s;
	while (count--)
		*xs++ = c;
	return s;
}

void __bzero(void *s,size_t count)
{
	memset(s,0,count);
}

void *memcpy(void *dest,const void *src,size_t count)
{
	char *tmp = (char *) dest, *s = (char *) src;
	while (count--)
		*tmp++ = *s++;
	return dest;
}

void *memmove(void *dest,const void *src,size_t count)
{
	char *d, *s;

	if (dest <= src) {
		d = (char *) dest;
		s = (char *) src;
		while (count--)
			*d++ = *s++;
	} else {
		d = (char *) dest + count;
		s = (char *) src + count;
		while (count--)
			*--d = *--s;
	}
	return dest;
}
