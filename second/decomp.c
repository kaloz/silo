/* Self decompression and image decompression routines
   
   Copyright (C) 1993 Hannu Savolainen
   		 1996,1998 Jakub Jelinek
   
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

#include <silo.h>
#include <setjmp.h>
#ifndef NULL
#define NULL (void *)0
#endif

/*
 * gzip declarations
 */

#define OF(args)  args
#define STATIC static

#define memzero(s, n)     memset ((s), 0, (n))

typedef unsigned char uch;
typedef unsigned short ush;
typedef unsigned long ulg;

#define WSIZE 0x8000		/* Window size must be at least 32k, */
				/* and a power of two */
static uch window[WSIZE];	/* Sliding window buffer */

static unsigned outcnt = 0;	/* bytes in output buffer */

/* gzip flag byte */
#define ASCII_FLAG   0x01	/* bit 0 set: file probably ascii text */
#define CONTINUATION 0x02	/* bit 1 set: continuation of multi-part gzip file */
#define EXTRA_FIELD  0x04	/* bit 2 set: extra field present */
#define ORIG_NAME    0x08	/* bit 3 set: original file name present */
#define COMMENT      0x10	/* bit 4 set: file comment present */
#define ENCRYPTED    0x20	/* bit 5 set: file is encrypted */
#define RESERVED     0xC0	/* bit 6,7:   reserved */

#define Assert(cond,msg)
#define Trace(x)
#define Tracev(x)
#define Tracevv(x)
#define Tracec(c,x)
#define Tracecv(c,x)

static void flush_window (void);
static void error (char *);
#define gzip_mark mark
inline void gzip_release (void **p)
{
    release (*p);
}

static long bytes_out;
static uch *output_data, *output_limit;
static unsigned char (*get_input_fun) (void);
static void (*unget_input_fun) (void);

jmp_buf gunzip_env;
#define get_byte() (*get_input_fun)()
#define unget_byte() (*unget_input_fun)()

#include "../common/inflate.c"

static void error (char *m)
{
    printf ("\nDecompression error: %s\n", m);
    longjmp (gunzip_env, 1);
}

static void flush_window ()
{
    ulg c = crc;
    unsigned n;
    uch *in, ch;
    in = window;
    if (output_data + outcnt > output_limit)
	error ("uncompressed image too long - wouldn't fit into destination");
    for (n = 0; n < outcnt; n++) {
	ch = *output_data++ = *in++;
	c = crc_32_tab[((int) c ^ ch) & 0xff] ^ (c >> 8);
    }
    crc = c;
    bytes_out += (ulg) outcnt;
    outcnt = 0;
}

int decompress (char *outptr, char *outptrlim, unsigned char (*get_input) (void), void (*unget_input) (void))
{
    void *save_ptr;
    static int first = 1;

    gzip_mark (&save_ptr);
    if (setjmp (gunzip_env)) {
	gzip_release (&save_ptr);
	return -1;
    }
    output_data = outptr;
    output_limit = outptrlim;
    get_input_fun = get_input;
    unget_input_fun = unget_input;
    bytes_out = 0;
    crc = 0xffffffffL;
    if (first) {
        makecrc ();
        first = 0;
    }
    gunzip ();
    gzip_release (&save_ptr);
    return bytes_out;
}

static unsigned char *gzminp;
static unsigned char get_input(void)
{
    return *gzminp++;
}

static void unget_input(void)
{
    gzminp--;
}

extern char main_text_start, main_text_end, main_data_start, main_data_end, main_rodata_start, main_rodata_end, __bss_start;

/* This has to be in data section, so that it does not get cleared. See crt0.S for details. */
char *gzminpi = (char *)0xdeadbeef;

extern int bootmain(void);

unsigned my_main(struct linux_romvec *promvec, void *cifh, void *cifs)
{
    prom_init(promvec, cifh, cifs);

#ifdef TFTP
    prom_puts ("SILO", 4);
#else
    prom_puts ("O", 1);
#endif
    if (!cifh) {
    	unsigned short *pt = (unsigned short *)gzminpi;
    	
    	while (*pt) pt++;
    	pt++;
    	while (*pt) pt++;
    	pt++;
    	gzminpi = (char *)pt;
    }
    gzminp = gzminpi;
    if (decompress ((char *)0x200000, (char *)&_start, get_input, unget_input) == -1) {
        printf ("\nInternal error\n");
        prom_halt();
    }
    memcpy (&main_text_start, (char *)0x200000, &main_text_end - &main_text_start);
    memcpy (&main_rodata_start, (char *)0x200000 + (&main_text_end - &main_text_start), &main_rodata_end - &main_rodata_start);
    memcpy (&main_data_start, (char *)0x200000 + (&main_text_end - &main_text_start) + (&main_rodata_end - &main_rodata_start), &main_data_end - &main_data_start);
    if (cifh) {
    	unsigned char *cp = (char *)0x380000;
    	unsigned short *pt = (unsigned short *)((char *)0x200000 + (&main_text_end - &main_text_start) + 
    				(&main_rodata_end - &main_rodata_start) + (&main_data_end - &main_data_start));
    	
    	while (*pt) {
    		cp += *pt;
    		pt++;
    		*cp += 4;
    	}
    	pt++;
    	cp = (char *)0x380000;
    	while (*pt) {
    		cp += *pt;
    		pt++;
    		*cp += 16;
    	}
    }
    return bootmain();
}
