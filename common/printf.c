/* Dumb printing routines
   
   Copyright (C) 1996 Pete A. Zaitcev
   Copyright (C) 1997 Jakub Jelinek
   
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

void putchar (char c)
{
    if (c == '\n')
	putchar_1 ('\r');
    putchar_1 (c);
}

/*
 * Print an unsigned integer in base b, avoiding recursion.
 */
static void printn (long n, int b)
{
    char prbuf[24];
    register char *cp;

    if (b == 10 && n < 0) {
	putchar ('-');
	n = -n;
    }
    cp = prbuf;
    do
	*cp++ = "0123456789ABCDEF"[(unsigned int) (((unsigned long)n) % b)];
    while ((n = ((unsigned long)n) / b & 0x0FFFFFFF));
    do
	putchar (*--cp);
    while (cp > prbuf);
}

void vprintf (char *fmt, va_list adx)
{
    register int c;
    char *s;

    for (;;) {
	while ((c = *fmt++) != '%') {
	    if (c == '\0') {
		putchar (0);
		return;
	    }
	    putchar (c);
	}
	c = *fmt++;
	if (c == 'd' || c == 'o' || c == 'x' || c == 'X') {
	    printn ((long) va_arg (adx, unsigned),
		    c == 'o' ? 8 : (c == 'd' ? 10 : 16));
	} else if (c == 'c') {
	    putchar (va_arg (adx, unsigned));
	} else if (c == 's') {
	    s = va_arg (adx, char *);
	    if (s == NULL)
		s = (char *)"(null)";
	    while ((c = *s++))
		putchar (c);
	} else if (c == 'l' || c == 'O') {
	    printn ((long) va_arg (adx, long), c == 'l' ? 10 : 8);
	}
    }
}

/*
 * Scaled down version of C Library printf.
 * Only %c %s %u %d (==%u) %o %x %l %O are recognized.
 */

void prom_printf (char *fmt,...)
{
    va_list x1;

    va_start (x1, fmt);
    vprintf (fmt, x1);
    va_end (x1);
}
