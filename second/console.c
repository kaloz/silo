/* $Id: console.c,v 1.1 2001/04/27 21:55:24 bencollins Exp $
 * console.c: Routines that deal with sending and receiving IO
 *            to/from the current console device using the PROM.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include "promlib.h"

/* Non blocking get character from console input device, returns -1
 * if no input was taken.  This can be used for polling.
 */
int
prom_nbgetchar(void)
{
	static char inc;
	int i = -1;

	switch(prom_vers) {
	case PROM_V0:
		i = (*(romvec->pv_nbgetchar))();
		break;
	case PROM_V2:
	case PROM_V3:
		if((*(romvec->pv_v2devops).v2_dev_read)(prom_stdin , &inc, 1) == 1)
			i = inc;
		break;
	case PROM_P1275:
		if (p1275_cmd ("read", 3, prom_stdin, &inc, 1) == 1)
			i = inc;
		break;
	}
	return i;
}

/* Non blocking put character to console device, returns -1 if
 * unsuccessful.
 */
int
prom_nbputchar(char c)
{
	static char outc;
	int i = -1;

	switch(prom_vers) {
	case PROM_V0:
		i = (*(romvec->pv_nbputchar))(c);
		break;
	case PROM_V2:
	case PROM_V3:
		outc = c;
		if( (*(romvec->pv_v2devops).v2_dev_write)(prom_stdout, &outc, 1) == 1)
			i = 0;
		break;
	case PROM_P1275:
		outc = c;
		if (p1275_cmd ("write", 3, prom_stdout, &outc, 1) == 1)
			i = 0;
		break;
	}
	return i;
}

/* Blocking version of get character routine above. */
char
prom_getchar(void)
{
	int character;
	while((character = prom_nbgetchar()) == -1) ;
	return (char) character;
}

/* Blocking version of put character routine above. */
void
prom_putchar(char c)
{
	if (prom_vers == PROM_P1275)
	    prom_nbputchar(c);
	else
	    while(prom_nbputchar(c) == -1) ;
}

void
prom_puts (char *s, int len)
{
	switch(prom_vers) {
	case PROM_V0:
	case PROM_V2:
		(*(romvec->pv_printf))(s);
		break;
	case PROM_V3:
		(*(romvec->pv_v2devops).v2_dev_write)(prom_stdout, s, len);
		break;
	case PROM_P1275:
		p1275_cmd ("write", 3, prom_stdout, s, len);
		break;
	}
}
