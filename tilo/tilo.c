/* TILO: The TFTP Image LOader
   
   Copyright (C) 1996 Jakub Jelinek
   		 1998 Jan Vondrak
   
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

#ifndef LARGETILO
#define MOVED_BASE 0x3c0000
#else
#define MOVED_BASE 0x4c0000
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
#ifdef TILO_DEBUG
    printf("Returning from decompress()\n");
#endif
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

extern char start, main_text_start, main_text_end, main_data_start, main_data_end, main_rodata_start, main_rodata_end, __bss_start;

struct ImageInfo
{
unsigned packed_start;
unsigned packed_len;
unsigned unpacked_len;		/* this is meaningful for the kernel images only */
unsigned root_start;		/* this is meaningful for the kernel images only */
};

extern struct ImageInfo image_table[4];	/* Sun4 kernel, Sun4c/d/m kernel, Sun4u kernel, root image */

#define SUN4_KERNEL	0
#define SUN4C_KERNEL	1
#define SUN4U_KERNEL	2
#define ROOT_IMAGE	3

#define HDRS_TAG	(('H'<<24) | ('d'<<16) | ('r'<<8) | 'S')

char *my_main (struct linux_romvec *promvec, void *cifh, void *cifs)
{
char *orig_code,*moved_code,*moved_ramdisk,*moved_kernel,*kernel_base;
unsigned *p,*q = NULL;
int kernel_number;

    prom_init(promvec, cifh, cifs);
    
    printf ("TILO\n");

    if (cifh)
        {
    	kernel_number = SUN4U_KERNEL;		/* Sun4u */
	printf("Selecting sun4u kernel...\n");
    	}
    else if ((long)promvec == 0x4000)
        {
    	kernel_number = SUN4_KERNEL;		/* Sun4 */
	printf("Selecting sun4 kernel...\n");
    	}
    else
    	{
    	kernel_number = SUN4C_KERNEL;		/* Sun4c/d/m */
	printf("Selecting sun4cdm kernel...\n");
    	}
    	
    if (image_table[kernel_number].packed_len == 0)
    	{
    	printf ("ERROR: No kernel for this architecture in this TILO image\n");
    	prom_halt ();
	}
	    				
    orig_code = (char*) 0x4000;
    moved_code = (char*) MOVED_BASE;
    moved_ramdisk = (char*)((long)(moved_code - image_table[ROOT_IMAGE].packed_len) & ~0xfff);
    moved_kernel = (char*)((long)(moved_ramdisk - image_table[kernel_number].packed_len) & ~0xfff);
#ifdef TILO_DEBUG
    printf("Locations: moved_code=%x  moved_ramdisk=%x moved_kernel=%x\n",
	   moved_code, moved_ramdisk, moved_kernel);
#endif
    memmove (moved_ramdisk, orig_code + image_table[ROOT_IMAGE].packed_start, image_table[ROOT_IMAGE].packed_len);
    memmove (moved_kernel, orig_code + image_table[kernel_number].packed_start, image_table[kernel_number].packed_len);

    gzminp = (char*) moved_kernel;		/* decompress kernel */
    kernel_base = (char*) 0x4000;

    if (decompress (kernel_base, kernel_base + ((image_table[kernel_number].unpacked_len
		 + 0xfff) & ~0xfff), get_input, unget_input) == -1)
    	{
        printf ("\nKernel decompression error\n");
        prom_halt();
    	}

    switch (kernel_number)
    	{
    	case SUN4U_KERNEL:
    						/* find HdrS in Sun4u kernel */
   	 	q = (unsigned*)kernel_base + 2;
    		break;
    
    	case SUN4C_KERNEL:
    						/* find HdrS in Sun4c/m/d kernel */
	    	p = (unsigned*)kernel_base;
	    	p += *p & 0xffff;		/* extract jump offset */ 
	    	q = p - 16;			/* from the branch instruction */
    						
    		while (q < p && *q != HDRS_TAG)
    			q++;
    		break;
    		
    	default:
    						/* find HdrS in Sun4 kernel */
    		printf ("Sun4 kernel not supported yet\n");
    		prom_halt ();
    		break;
    	
    	}

    if (*q != HDRS_TAG)
    	{
	printf ("Can't find HdrS tag in kernel\n");
 	prom_halt ();
    	}
    	
    q[2] &= 0xffff0000;				/* reset root flags */
    q[3] = 0x01000000;				/* set root device and flags */
    q[4] = image_table[kernel_number].root_start;
    q[5] = image_table[ROOT_IMAGE].packed_len;
 
 						/* move root image */
    memmove ((void*)(image_table[kernel_number].root_start & 0x3fffff),
    	moved_ramdisk, image_table[ROOT_IMAGE].packed_len);
#ifdef TILO_DEBUG
    printf("Returning from my_main() with address %x\n", kernel_base);
#endif
    return kernel_base;			/* return address to jump into kernel */
}
