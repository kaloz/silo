/* prom - prom handling routines via /dev/openprom prototypes
   
   Copyright (C) 1996 Jakub Jelinek
   
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

int prom_init (void);
int prom_set_root_node (void);
int prom_search_siblings (char *);
int prom_next_sibling (void);
int prom_getchild (void);
char *prom_getstring (char *);
char *prom_getopt (char *);
int prom_getversion();
