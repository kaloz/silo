/* Slooow, but small string operations, so that we don't have
   to link libc5/glibc in.
   Originally from linux/lib/string.c, which is 
   	Copyright (C) 1991, 1992  Linus Torvalds
 */
 
#include <stringops.h>

char * strncpy(char *dest, const char *src, size_t count)
{
	char *tmp = dest;

	while (count-- && (*dest++ = *src++) != '\0')
		/* nothing */;

	return tmp;
}

char *strcat(char *dest, const char *src)
{
	char *tmp = dest;
	while (*dest) dest++;
	while ((*dest++ = *src++) != '\0');
	return tmp;
}

char *strncat(char *dest, const char *src, size_t n)
{
	char *tmp = dest;
	while (*dest) dest++;
	while (n && (*dest++ = *src++) != '\0') n--;
	if (!n) *dest = 0;
	return tmp;
}

char * strrchr(const char * s, int c)
{
	const char *p = s + strlen(s);
	do {
		if (*p == (char)c)
			return (char *)p;
	} while (--p >= s);
	return 0;
}

char *strdup(const char *str)
{
	extern void *malloc(int);
	char *ret;
	ret = malloc(strlen(str) + 1);
	strcpy(ret, str);
	return ret;
}

__inline__ int tolower(int c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
	return c;
}

int strcasecmp(const char *cs,const char *ct)
{
	register signed char __res;
	while (1)
		if ((__res = tolower(*cs) - tolower(*ct++)) != 0 || !*cs++)
			break;
	return __res;
}

int strncasecmp(const char *cs,const char *ct,size_t n)
{
	register signed char __res = 0;
	while (n--)
		if ((__res = tolower(*cs) - tolower(*ct++)) != 0 || !*cs++)
			break;
	return __res;
}

char * strstr(const char * s1,const char * s2)
{
	int l1, l2;

	l2 = strlen(s2);
	if (!l2)
		return (char *) s1;
	l1 = strlen(s1);
	while (l1 >= l2) {
		l1--;
		if (!memcmp(s1,s2,l2))
			return (char *) s1;
		s1++;
	}
	return 0;
}
