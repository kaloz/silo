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
    char buffer[512], *array_name, *file_name, *lendef_name = NULL;
    int i, j, offset = 1;
    
    if (argc != 3 && !(argc == 5 && !strcmp(argv[1],"-l"))) {
	fprintf(stderr, "Usage: %s [-l len_def] <array name> <file>\n", argv[0]);
	exit (1);
    }

    if (!strcmp(argv[1],"-l")) {
	lendef_name = argv[offset + 1];
	offset += 2;
    }

    array_name = argv[offset++];
    file_name = argv[offset];

    if ((f = fopen(file_name, "r")) == NULL) {
	perror("fopen");
	exit (1);
    }

    if (lendef_name != NULL) {
	int len;
	if (fseek (f, 0, SEEK_END)) {
	    perror("fseek");
	    exit (1);
	}

	len = ftell (f);

	if (fseek (f, 0, SEEK_SET)) {
	    perror("fseek");
	    exit (1);
	}

	printf ("#define %s %d\n\n",lendef_name, len);
    }

    printf ("char %s[] = {\n", array_name);
    while ((offset = fread (buffer, 1, sizeof(buffer), f))) {
	for (i = 0; i < 32 && offset; i++) {
	    for (j = 0; j < 16 && offset; j++) {
		printf ("0x%02X, ", (unsigned char)buffer[16 * i + j]);
		offset--;
	    }
	    printf ("\n");
	}
    }
    fclose(f);
    printf ("};\n");
    exit (0);
}
