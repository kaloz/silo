/* Tool to make gzipped second stage binary
   
   Copyright (C) 1996,1998 Jakub Jelinek
   
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

FILE *f, *e;
unsigned char buffer[2048];
unsigned char buffer2[2048];
unsigned short diffs[4][2048];
int ndiffs[4];
unsigned int lastv[4];
unsigned int curoff;
unsigned int prevlen;
unsigned int prevoff;

void save(FILE *out, int len, int type)
{
    int i, j, k;
    int curlen = len;

    while (len > 0) {
        i = 2048;
        if (len < i) i = len;
        if (fread (buffer, 1, i, f) != i) {
	    fprintf(stderr, "read %d from arg3 type %d failed\n", i, type);
	    exit(1);
	}
        if (fread (buffer2, 1, i, e) != i) {
	    fprintf(stderr, "read %d from arg4 type %d failed\n", i, type);
	    exit(1);
	}
        for (j = 0; j < i; j++) {
    	    if (buffer[j] != buffer2[j]) {
		if (buffer2[j] == buffer[j] + 4) {
		    if (curoff + j > lastv[type] + 65535) {
			if (type || !prevlen || curoff + j >= prevoff + 65535)
			    goto bad1;
			k = lastv[type] + 65535;
			if (k < prevoff - prevlen)
			    goto bad1;
			if (k >= prevoff)
			    k = prevoff - 1;
			diffs[type][ndiffs[type]] = k - lastv[type];
			lastv[type] = k;
			ndiffs[type]++;
		    }
		    diffs[type][ndiffs[type]] = curoff + j - lastv[type]; 
		    lastv[type] = curoff + j;
		    ndiffs[type]++;
		} else if (buffer2[j] == buffer[j] + 16) {
		    if (curoff + j > lastv[type+1] + 65535) {
			if (type || !prevlen || curoff + j >= prevoff + 65535)
			    goto bad2;
			k = lastv[type+1] + 65535;
			if (k < prevoff - prevlen)
			    goto bad2;
			if (k >= prevoff)
			    k = prevoff - 1;
			diffs[type+1][ndiffs[type+1]] = k - lastv[type+1];
			lastv[type+1] = k;
			ndiffs[type+1]++;
		    }
		    diffs[type+1][ndiffs[type+1]] = curoff + j - lastv[type+1]; 
		    lastv[type+1] = curoff + j;
		    ndiffs[type+1]++;
		} else {
		    fprintf(stderr, "Strange, 2.5MB and 3.5MB images differ in something"
			    " different to R_SPARC_32 and R_SPARC_HI22\n");
		    exit(1);
		}
	    }
        }
	if (fwrite (buffer, 1, i, out) != i) {
	    fprintf(stderr, "write %d failed\n", i);
	    exit(1);
	}
        len -= i;
        curoff += i;
    }
    prevlen = curlen;
    prevoff = curoff;
    return;
bad2:
    type++;
bad1:
    fprintf(stderr, "Distance between two changes larger than 64K %d %d %d\n",
	    type, curoff + j, lastv[type]);
    exit(1);
}

int main(int argc, char **argv)
{
    FILE *g, *h;
    int reloc = 0x280000;
    int first_start, first_end, second_start, second_end;
    int end, rodata_start, rodata_end;
    int net = 0;
    int format = 11, formatchecked = 0;
     
    if (!strcmp (argv[1], "-a"))
    	net = 1;
    f = fopen(argv[2], "r");
    while (fgets (buffer, 256, f)) {
        if (!formatchecked) {
            formatchecked = 1;
            if (!strncmp (buffer, "00000000", 8) && isxdigit(buffer[8]))
            	format += 8;
        }
        if (!strcmp (buffer+format, "start\n"))
            reloc = strtol (buffer, NULL, 16);
        else if (!strcmp (buffer+format, "main_text_start\n"))
            first_start = strtol (buffer, NULL, 16) -  reloc;
        else if (!strcmp (buffer+format, "main_text_end\n"))
            first_end = strtol (buffer, NULL, 16) - reloc;
        else if (!strcmp (buffer+format, "main_data_start\n"))
            second_start = strtol (buffer, NULL, 16) - reloc;
        else if (!strcmp (buffer+format, "main_data_end\n"))
            second_end = strtol (buffer, NULL, 16) - reloc;
        else if (!strcmp (buffer+format, "main_rodata_start\n"))
            rodata_start = strtol (buffer, NULL, 16) - reloc;
        else if (!strcmp (buffer+format, "main_rodata_end\n"))
            rodata_end = strtol (buffer, NULL, 16) - reloc;
        else if (!strcmp (buffer+format, "__bss_start\n"))
            end = strtol (buffer, NULL, 16) - reloc;
    }   
    fclose (f);
    f = fopen(argv[3], "r");
    e = fopen(argv[4], "r");
    g = fopen(argv[5], "w");
    h = fopen(argv[6], "w");
    if (fread(buffer, 1, 32, f) != 32) exit(1);
    if (fread(buffer2, 1, 32, e) != 32) exit(1);
    if (memcmp(buffer, buffer2, 32)) {
    	fprintf(stderr, "Strange. Images for 2.5MB and 3.5MB differ in a.out header\n");
    	exit(1);
    }
    if (!net) {
    	memset (buffer, 0, 2048);
    	if (fwrite(buffer, 1, 2048, g) != 2048) exit(1);
    } else {
    	if (fwrite (buffer, 1, 32, g) != 32) exit(1);
    }
    save (g, first_start, 0);
    save (h, first_end - first_start, 2);
    save (g, rodata_start - first_end, 0);
    save (h, rodata_end - rodata_start, 2);
    save (g, second_start - rodata_end, 0);
    save (h, second_end - second_start, 2);
    save (g, end - second_end, 0);
    fwrite(diffs[0], 2, ndiffs[0]+1, g);
    fwrite(diffs[1], 2, ndiffs[1]+1, g);
    fwrite(diffs[2], 2, ndiffs[2]+1, h);
    fwrite(diffs[3], 2, ndiffs[3]+1, h);
    fclose (f);
    fclose (e);
    fclose (g);
    fclose (h);
    exit (0);
}
