/* iso9660 filesystem handling
   
   Copyright (C) 1999 Jakub Jelinek
   Copyright (C) 1993 Eric Youngdale
   
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

#ifdef __linux__

#  include <ctype.h>
#  include <sys/types.h>
#  include <errno.h>
#  include "silo.h"
typedef int FILE;
#  include <linux/ext2_fs.h>
#  include <linux/iso_fs.h>

#else

#  include <stdio.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <errno.h>
#  include <non-linux/ext2_fs.h>
#  include <non-linux/iso_fs.h>

#endif

#include "ext2fs/ext2fs.h"
#include "9660.h"

#define SUPISO ((struct iso_primary_descriptor *)fs->io->private_data)
#define ROOTDIR ((struct iso9660_inode *)SUPISO->unused2)

static int isonum_731 (char * p)
{
	return ((p[0] & 0xff)
		| ((p[1] & 0xff) << 8)
		| ((p[2] & 0xff) << 16)
		| ((p[3] & 0xff) << 24));
}

#define isonum_733(p) isonum_731(p)

static struct iso_primary_descriptor *iso9660_read_super(iso9660_filsys fs)
{
    int i;
    struct iso_primary_descriptor *iso = (struct iso_primary_descriptor *) malloc (2048);
    struct iso9660_inode *root;
    
    for (i = 16; i < 100; i++) {
        if (io_channel_read_blk (fs->io, i, -2048, (char *)iso))
            return 0;
        if (!strncmp (iso->id, ISO_STANDARD_ID, sizeof (iso->id)))
            break;
    }
    
    if (i == 100) return 0;

    root = (struct iso9660_inode *)iso->unused2;
    root->extent = isonum_733 (((struct iso_directory_record *)(iso->root_directory_record))->extent);
    root->size = isonum_733 (((struct iso_directory_record *)(iso->root_directory_record))->size);

    return iso;
}

int iso9660_open (char *device, io_manager iom, iso9660_filsys *pfs)
{
    iso9660_filsys fs;
    fs = (iso9660_filsys) malloc (sizeof (struct struct_ext2_filsys));
    if (!fs) return -1;
    if (iom->open (device, 0, &fs->io)) return -1;
    io_channel_set_blksize (fs->io, 2048);
    fs->io->private_data = iso9660_read_super(fs);
    if (!fs->io->private_data) return -1;
    *pfs = fs;
    return 0;
}

static void parse_rr (iso9660_filsys fs, unsigned char *rr, unsigned char *end, char *name, char *symlink)
{
    int cont_extent = 0, cont_offset = 0, cont_size = 0;
    while (rr < end) {
	if (rr[3] != 1) {
	    printf ("Bad RR version\n");
	    break;
	}
	if (!strncmp (rr, "NM", 2)) {
	    strncpy (name, rr + 5, rr[2] - 5);
	    name[rr[2] - 5] = 0;
	} else if (!strncmp (rr, "SL", 2)) {
	    int len = rr[2] - 5;
	    unsigned char *p = rr + 5;
	    while (len > 1) {
		switch (p[0] & ~1) {
		case 0: strncat (symlink, p+2, p[1]); break;
		case 2: strcat (symlink, "."); break;
		case 4: strcat (symlink, ".."); break;
		case 8: strcat (symlink, "/"); break;
		default: printf ("Unhandled bit in SL\n"); break;
		}
		len -= p[1] + 2;
		if ((p[0] & 1) == 0 && p[0] != 8 && len > 1)
		    strcat (symlink, "/");
		p += p[1] + 2;
	    }
	} else if (!strncmp (rr, "CE", 2)) {
	    cont_extent = isonum_733 (rr + 4);
	    cont_offset = isonum_733 (rr + 12);
	    cont_size = isonum_733 (rr + 20);
	}
	rr += rr[2];
	if (rr >= end && cont_extent) {
	    char *sect = alloca (2048);
	    if (io_channel_read_blk (fs->io, cont_extent, 1, sect))
		return;
	    parse_rr (fs, &sect [cont_offset], &sect [cont_offset + cont_size - 3],
		      name, symlink);
	}
    }
}

static int link_count = 0;

static int open_namei(iso9660_filsys, const char *, struct iso9660_inode *, struct iso9660_inode *);

static int iso9660_lookup (iso9660_filsys fs, struct iso9660_inode *dir,
		       const char *name, int len, struct iso9660_inode *result)
{
    char buffer [2048];
    char namebuf [512];
    char symlink [512];
    int block, size, i;
    struct iso_directory_record *idr;
    unsigned char *rr;

    size = dir->size;
    block = dir->extent;
    while (size > 0) {
	if (io_channel_read_blk (fs->io, block, 1, buffer)) {
	    printf ("Could not read directory\n");
	    return -1;
	}
	size -= 2048;
	block++;
	for (i = 0;;) {
	    idr = (struct iso_directory_record *) (buffer + i);
	    if (!idr->length[0]) break;
	    i += (unsigned char)idr->length[0];
	    strncpy(namebuf, idr->name, (unsigned char)idr->name_len[0]);
	    namebuf[(unsigned char)idr->name_len[0]] = 0;
	    rr = (unsigned char *)(idr + 1);
	    rr += ((unsigned char)idr->name_len[0]) - sizeof(idr->name);
	    if (!(idr->name_len[0] & 1)) rr++;
	    *symlink = 0;
	    parse_rr (fs, rr, &buffer[i-3], namebuf, symlink);
	    if (idr->name_len[0] == 1 && !idr->name[0])
		strcpy(namebuf, ".");
	    else if (idr->name_len[0] == 1 && idr->name[0] == 1)
		strcpy(namebuf, "..");
	    if ((!len && namebuf[0] == '.' && !namebuf[1]) || 
	        (strlen(namebuf) == len && !memcmp(namebuf, name, len))) {
		if (*symlink) {
		    int error;
		    if (link_count > 5) {
			printf ("Symlink loop\n");
			return -1; /* Loop */
		    }
		    link_count++;
		    error = open_namei (fs, symlink, result, dir);
		    link_count--;
		    return error;
		}
		result->extent = isonum_733 (idr->extent);
		result->size = isonum_733 (idr->size);
		return 0;
	    }
	    if (i >= 2048 - sizeof(struct iso_directory_record) + sizeof(idr->name))
		break;
	}
    }
    return -1;
}

static int dir_namei(iso9660_filsys fs, const char *pathname, int *namelen, 
		     const char **name, struct iso9660_inode *base,
		     struct iso9660_inode *res_inode)
{
    char c;
    const char *thisname;
    int len;
    struct iso9660_inode inode;

    if ((c = *pathname) == '/') {
	base = ROOTDIR;
	pathname++;
    }
    while (1) {
	thisname = pathname;
	for(len=0;(c = *(pathname++))&&(c != '/');len++);
	if (!c) break;
	if (iso9660_lookup (fs, base, thisname, len, &inode)) return -1;
	base = &inode;
    }
    *name = thisname;
    *namelen = len;
    *res_inode = *base;
    return 0;
}

static int open_namei(iso9660_filsys fs, const char *pathname, 
		      struct iso9660_inode *res_inode,
		      struct iso9660_inode *base)
{
    const char *basename;
    int namelen;
    struct iso9660_inode dir, inode;

    if (dir_namei(fs, pathname, &namelen, &basename, base, &dir)) return -1;
    if (!namelen) {			/* special case: '/usr/' etc */
	*res_inode=dir;
	return 0;
    }
    if (iso9660_lookup (fs, &dir, basename, namelen, &inode)) return -1;
    *res_inode = inode;
    return 0;
}

int iso9660_namei (iso9660_filsys fs, char *filename, struct iso9660_inode *inode)
{
    link_count = 0;
    return open_namei (fs, filename, inode, ROOTDIR);
}

void iso9660_close(iso9660_filsys fs)
{
    free (fs->io);
    free (fs);
}

int iso9660_block_iterate(iso9660_filsys fs, struct iso9660_inode *inode, 
			  int (*func)(iso9660_filsys, blk_t *, int, void *), 
			  void *private)
{
    int i;
    blk_t nr;
    int size;

    nr = inode->extent;    
    size = (inode->size + 2047) / 2048;
    for (i = 0; i < size; i++, nr++) {
        switch ((*func) (fs, &nr, i, private)) {
            case BLOCK_ABORT:
            case BLOCK_ERROR:
            	return -1;
        }
    }
    return 0;
}
