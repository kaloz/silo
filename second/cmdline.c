/* Prompt handling
   
   Copyright (C) 1996 Maurizio Plaza
   		 1996 Jakub Jelinek
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

#include <silo.h>

char cbuff[CMD_LENG];
char passwdbuff[CMD_LENG];
extern int useconf;
extern int tab_ambiguous;

void cmdinit ()
{
    cbuff[0] = 0;
    passwdbuff[0] = 0;
}

void cmdedit (void (*tabfunc) (void), int password)
{
    int x, c;
    char *buff = password ? passwdbuff : cbuff;

    for (x = 0; x < CMD_LENG - 1; x++) {
	if (buff[x] == 0)
	    break;
	else if (password)
	    prom_puts ("*", 1);
    }
    if (!password)
        prom_puts (buff, x);

    for (;;) {
	c = prom_getchar ();
	switch (c) {
	    case -1:
	    case '\n':
	    case '\r':
		goto break_both;

	    case '\t':
		if (tabfunc) (*tabfunc)();
		/* reset x */
		x = strlen(buff);
		break;

	    case '\b':
	    case 0x7F:
		tab_ambiguous = 0;
		if (x > 0) {
		    x--;
		    cbuff[x] = 0;
		    prom_puts ("\b \b", 3);
		}
		break;

	    default:
		if ((c & 0xE0) != 0) {
		    tab_ambiguous = 0;
		    if (x < CMD_LENG - 1) {
			buff[x] = c;
			buff[x + 1] = 0;
			/* For password input, we mask it out */
			prom_puts((password) ? "*" : buff + x, 1);
			x++;
		    }
		    if (x == 1 && !password && useconf) {
			if (cfg_get_flag (cbuff, "single-key"))
			    goto break_both;
		    }
		}
	}
    }
break_both:
    buff[x] = 0;
}
