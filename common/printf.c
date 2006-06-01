/* Dumb printing routines
   
   Copyright (C) 1996 Pete A. Zaitcev
                 1997 Jakub Jelinek
		 2001 Ben Collins
   
   
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

#include "promlib.h"

/*
 * This part is rewritten by Igor Timkin <ivt@msu.su>. Than I
 * rewritten it again but kept the MISS style, originated from
 * Wladimir Butenko. --P3
 */
#define OBSIZE      200

#ifndef NULL
#define NULL (void *)0
#endif

static void putchar_1 (char c)
{
    /* P3: This buffer can be static while xprintf flushes it on exit. */
    static char buff[OBSIZE + 1];
    static int ox;

    if ((buff[ox] = c) == 0) {
	prom_puts (buff, ox);
	ox = 0;
    } else {
	if (++ox >= OBSIZE) {
	    buff[ox] = 0;
	    prom_puts (buff, ox);
	    ox = 0;
	}
    }
}

int putchar (int __c)
{
    char c = (char) __c;
    if (c == '\n')
	putchar_1 ('\r');
    putchar_1 (c);

    return __c;
}

/*
 * Print an unsigned integer in base b, avoiding recursion.
 */
static int printn (long long n, int b)
{
    static char prbuf[33];
    register char *cp;
    int count = 0;

    if (b == 10 && n < 0) {
	putchar ('-');
	count++;
	n = -n;
    }
    cp = prbuf;
    do
	*cp++ = "0123456789ABCDEF"[(unsigned int) (((unsigned long)n) % b)];
    while ((n = ((unsigned long long)n) / b & 0x0FFFFFFFFFFFFFFFULL));
    do {
	putchar (*--cp);
	count++;
    } while (cp > prbuf);

    return count;
}

int vprintf (char *fmt, va_list adx)
{
    register int c;
    char *s;
    int count = 0;

    for (;;) {
	while ((c = *fmt++) != '%') {
	    if (c == '\0') {
		putchar (0);
		return count;
	    }
	    putchar (c);
	}
	c = *fmt++;
	if (c == 'd' || c == 'o' || c == 'x' || c == 'X') {
	    count += printn ((long long) va_arg (adx, unsigned),
			     c == 'o' ? 8 : (c == 'd' ? 10 : 16));
	} else if (c == 'c') {
	    putchar (va_arg (adx, unsigned));
	    count++;
	} else if (c == 's') {
	    if ((s = va_arg (adx, char *)) == NULL)
		s = (char *)"(null)";
	    while ((c = *s++)) {
		putchar (c);
		count++;
	    }
	} else if (c == 'l' || c == 'O') {
	    count += printn ((long long) va_arg (adx, long), c == 'l' ? 10 : 8);
	} else if (c == 'L') {
	    int hex = 0;
	    if (*fmt == 'x') {
		fmt++;
		hex = 1;
	    }
	    count += printn ((long long) va_arg (adx, long long), hex ? 16 : 10);
	} else {
	    /* This is basically what libc's printf does */
	    putchar('%'); putchar(c);
	    count += 2;
	}
    }

    return count;
}

/*
 * Scaled down version of C Library printf.
 * Only %c %s %d (==%u) %o %x %X %l %O are recognized.
 */

void prom_printf (char *fmt,...)
{
    va_list x1;

    va_start (x1, fmt);
    vprintf (fmt, x1);
    va_end (x1);
}
