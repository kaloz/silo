/* a simple utility to dump binary into a header file
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>

int main (int argc, char **argv)
{
    FILE *f;
    char buffer[512];
    int i, j;
    
    if (argc != 3) {
	fprintf(stderr, "Usage: %s <array name> <file>\n", argv[0]);
	exit (1);
    }

    if ((f = fopen(argv[2], "r")) == NULL) {
	perror("fopen");
	exit (1);
    }

    fread (buffer, 1, sizeof(buffer), f);
    fclose (f);
    printf ("char %s[] = {\n", argv[1]);
    for (i = 0; i < 32; i++) {
        for (j = 0; j < 16; j++)
            printf ("0x%02X, ", (unsigned char)buffer[16 * i + j]);
        printf ("\n");
    }
    printf ("};\n");
    exit (0);
}
