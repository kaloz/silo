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

char *strchr(const char *s, int c)
{
	for(; *s != (char) c; ++s)
		if (*s == '\0')
			return 0;
	return (char *) s;
}

int strlen(const char *s)
{
	const char *sc;
	for (sc = s; *sc != '\0'; ++sc);
	return sc - s;
}

int strncmp(const char *cs, const char *ct, size_t count)
{
	register signed char __res = 0;
	while (count) {
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
		count--;
	}
	return __res;
}

int memcmp(const void *cs, const void *ct, size_t count)
{
	const unsigned char *su1, *su2;
	signed char res = 0;

	for( su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
		if ((res = *su1 - *su2) != 0)
			break;
	return res;
}

