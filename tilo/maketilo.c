
/*
	Jan Vondrak (C) 1998
	Jakub Jelinek (C) 1998

	A simple program which appends the kernel and root images
	to the boot loader and patches the addresses in its image_table[]
	
	Parameters:
			kernel4=<filename>
			kernel4c=<filename>
			kernel4u=<filename>
				... compressed kernel images (in a.out format)
			size4=<bytes>
			size4c=<bytes>
			size4u=<bytes>
				... original sizes of kernel images (uncompressed)
			root4=<addr>
			root4c=<addr>
			root4u=<addr>
				... virtual address of root image for each kernel (in hex)
			root=<filename>
				... compressed root image
			out=<filename>
				... the output file
*/


#include <stdio.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include "b.h"
#include "b2.h"

#define MAX_BOOT_LEN	0x400000

char output_buffer[MAX_BOOT_LEN];

struct ImageInfo {
	unsigned packed_start;
	unsigned packed_len;
	unsigned unpacked_len;
	unsigned root_start;
};

int root_tweak (char *s)
{
	unsigned p;

	p = strtoul (s, NULL, 16);		/* read virtual address in hex */
	return p ? (p + 32 + 0x1fff) & ~0x1fff : 0;	/* add 32 bytes and round to 8 KB */
}

int main (int argc, char **argv)
{
	int i,len,rootlen;
	FILE *f, *g;
	struct ImageInfo *ii;

	char *sun4_kernel_start;
	char *sun4c_kernel_start;
	char *sun4u_kernel_start;
	char *root_image_start;
	char *output_end;

	int sun4_size = 0, sun4_root = 0;
	int sun4c_size = 0, sun4c_root = 0;
	int sun4u_size = 0, sun4u_root = 0;

	char *sun4_kernel = 0;
	char *sun4c_kernel = 0;
	char *sun4u_kernel = 0;
	char *root_image = 0;
	char *output_file = 0;

	if (argc < 4) {
		fprintf (stderr, "Usage: maketilo\n"
		"\tsun4=<sun4 gzipped kernel> size4=<sun4 orig size> root4=<sun4 root address>\n"
		"\tsun4c=<sun4c gzipped size> size4c=<sun4c orig size> root4c=<sun4c root address>\n"
		"\tsun4u=<sun4u gzipped kernel> size4u=<sun4u orig size> root4u=<sun4u root address>\n"
		"\troot=<root image>\n"
		"\tout=<output file>\n");
		return -1;
	}

	for (i=1;i<argc;i++) {
		if (!strncmp (argv[i], "sun4=", 5))
			sun4_kernel = argv[i] + 5;
		else if (!strncmp (argv[i], "sun4c=", 6))
			sun4c_kernel = argv[i] + 6;
		else if (!strncmp (argv[i], "sun4u=", 6))
			sun4u_kernel = argv[i] + 6;
		else if (!strncmp (argv[i], "size4=", 6))
			sun4_size = atoi (argv[i]+6);
		else if (!strncmp (argv[i], "size4c=", 7))
			sun4c_size = atoi (argv[i]+7);
		else if (!strncmp (argv[i], "size4u=", 7))
			sun4u_size = atoi (argv[i]+7);
		else if (!strncmp (argv[i], "root4=", 6))
			sun4_root = root_tweak (argv[i]+6);
		else if (!strncmp (argv[i], "root4c=", 7))
			sun4c_root = root_tweak (argv[i]+7);
		else if (!strncmp (argv[i], "root4u=", 7))
			sun4u_root = root_tweak (argv[i]+7);
		else if (!strncmp (argv[i], "root=", 5))
			root_image = argv[i] + 5;
		else if (!strncmp (argv[i], "out=", 4))
			output_file = argv[i] + 4;
	}
	
	if (!sun4_kernel) {
		/*fprintf (stderr, "WARNING: Kernel for Sun4 not specified\n");*/
	} else if (!sun4_size || !sun4_root) {
		fprintf (stderr, "WARNING: Original size and root address must be specified for Sun4\n");
		return -1;
	}
	
	if (!sun4c_kernel) {
		fprintf (stderr, "WARNING: Kernel for Sun4c/m/d not specified\n");
	} else if (!sun4c_size || !sun4c_root) {
		fprintf (stderr, "ERROR: Original size and root address must be specified for Sun4c\n");
		return -1;
	}
	
	if (!sun4u_kernel) {
		fprintf (stderr, "WARNING: Kernel for Sun4u not specified\n");
	} else if (!sun4u_size || !sun4u_root) {
		fprintf (stderr, "ERROR: Original size and root address must be specified for Sun4u\n");
		return -1;
	}
	
	if (!root_image) {
		fprintf (stderr, "ERROR: Root image not specified\n");
		return -1;
	}
		
	if (!output_file) {
		fprintf (stderr, "ERROR: Output file not specified\n");
		return -1;
	}
	
	g = fopen (root_image, "rb");
	if (!g) {
		fprintf (stderr, "Can't load %s\n", root_image);
		return -1;
	}
	
	fseek (g, 0, SEEK_END);
	rootlen = ftell (g);
	fseek (g, 0, SEEK_SET);
	
	if (rootlen + sun4_size + 0x4000 + 0x10000 >= 0x330000 ||
	    rootlen + sun4c_size + 0x4000 + 0x10000 >= 0x330000 ||
	    rootlen + sun4u_size + 0x4000 + 0x10000 >= 0x330000) {
		printf("Images are large. Will load on machines with at least 5M mapped by PROM only\n");
	
		for (i=0; i<LARGE_BOOT_LEN; i++)
			output_buffer[i] = large_boot_loader[i];
	} else
		for (i=0; i<BOOT_LEN; i++)
			output_buffer[i] = boot_loader[i];
	
	sun4_kernel_start = output_buffer + BOOT_LEN;

	if (sun4_kernel) {
		f = fopen (sun4_kernel, "rb");
		if (!f) {
			fprintf (stderr, "Can't load %s\n", sun4_kernel);
			return -1;
		}
		
		fseek (f, 0, SEEK_END);
		len = ftell (f);
		fseek (f, 0, SEEK_SET);
		fread (sun4_kernel_start, 1, len, f);
		fclose (f);
	} else
		len = 0;

	sun4c_kernel_start = sun4_kernel_start + len;

	if (sun4c_kernel) {
		f = fopen (sun4c_kernel, "rb");
		if (!f) {
			fprintf (stderr, "Can't load %s\n", sun4c_kernel);
			return -1;
		}
		
		fseek (f, 0, SEEK_END);
		len = ftell (f);
		fseek (f, 0, SEEK_SET);
		fread (sun4c_kernel_start, 1, len, f);
		fclose (f);
	} else
		len = 0;

	sun4u_kernel_start = sun4c_kernel_start + len;

	if (sun4u_kernel) {
		f = fopen (sun4u_kernel, "rb");
		if (!f) {
			fprintf (stderr, "Can't load %s\n", sun4u_kernel);
			return -1;
		}
		
		fseek (f, 0, SEEK_END);
		len = ftell (f);
		fseek (f, 0, SEEK_SET);
		fread (sun4u_kernel_start, 1, len, f);
		fclose (f);
	} else
		len = 0;

	root_image_start = sun4u_kernel_start + len;
	
	fread (root_image_start, 1, rootlen, g);
	fclose (g);

	output_end = root_image_start + rootlen;

	/* patch code, data and BSS size in the .out header */
	*(unsigned*)(output_buffer+4) = htonl(output_end - output_buffer);
	*(unsigned*)(output_buffer+8) = 0;
	*(unsigned*)(output_buffer+12) = 0;

	/* fill image_table[] in the boot loader */
	ii = (struct ImageInfo*)(output_buffer + 40);			

	if (sun4_kernel) {
		ii[0].packed_start = sun4_kernel_start - output_buffer - 32;
		ii[0].packed_len = sun4c_kernel_start - sun4_kernel_start;
		ii[0].unpacked_len = sun4_size;
		ii[0].root_start = sun4_root;
	} else {
		ii[0].packed_start = 0;
		ii[0].packed_len = 0;
		ii[0].unpacked_len = 0;
		ii[0].root_start = 0;
	}

	if (sun4c_kernel) {
		ii[1].packed_start = sun4c_kernel_start - output_buffer - 32;
		ii[1].packed_len = sun4u_kernel_start - sun4c_kernel_start;
		ii[1].unpacked_len = sun4c_size;
		ii[1].root_start = sun4c_root;
	} else {
		ii[1].packed_start = 0;
		ii[1].packed_len = 0;
		ii[1].unpacked_len = 0;
		ii[1].root_start = 0;
	}
	
	if (sun4u_kernel) {
		ii[2].packed_start = sun4u_kernel_start - output_buffer - 32;
		ii[2].packed_len = root_image_start - sun4u_kernel_start;
		ii[2].unpacked_len = sun4u_size;
		ii[2].root_start = sun4u_root;
	} else {
		ii[2].packed_start = 0;
		ii[2].packed_len = 0;
		ii[2].unpacked_len = 0;
		ii[2].root_start = 0;
	}
	
	ii[3].packed_start = root_image_start - output_buffer - 32;
	ii[3].packed_len = output_end - root_image_start;
	ii[3].unpacked_len = 0;
	ii[3].root_start = 0;

	for (i = 0; i < 4; i++) {
		ii[i].packed_start = htonl(ii[i].packed_start);
		ii[i].packed_len   = htonl(ii[i].packed_len);
		ii[i].unpacked_len = htonl(ii[i].unpacked_len);
		ii[i].root_start   = htonl(ii[i].root_start);
	}

	f = fopen (output_file, "wb");
	if (!f) {
		fprintf (stderr, "Can't open %s for writing\n", output_file);
		return -1;
	}
	
	fwrite (output_buffer, 1, output_end - output_buffer, f);
	fclose (f);
	
	return 0;
}
