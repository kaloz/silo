/* ROMFS filesystem handling
   
   Copyright (C) 1998 Jakub Jelinek <jj@ultra.linux.cz>
   Copyright (C) 1997 Janos Farkas <chexum@shadow.banki.hu>
   
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

#else

#  include <stdio.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <errno.h>
#  include <non-linux/ext2_fs.h>

#endif

#include "romfs.h"

#define SUPROMFS (struct romfs_super_block *)(fs->io->private_data)

static __s32
romfs_checksum(void *data, int size)
{
	__s32 sum, *ptr;
	sum = 0; ptr = data;
	size>>=2;
	while (size>0) {
		sum += *ptr++;
	size--;
        }
	return sum;
}

static struct romfs_super_block *romfs_read_super(romfs_filsys fs)
{
    struct romfs_super_block *rsb;

    rsb = (struct romfs_super_block *) malloc (2048+512);
    if (!rsb) return 0;
    if (io_channel_read_blk (fs->io, 0, 1, (char *)rsb))
        return 0;
    if (strncmp((char *)rsb, "-rom1fs-", 8) || rsb->size < ROMFH_SIZE)
        return 0;
    if (romfs_checksum(rsb, 512)) {
    	printf("Bad ROMFS initial checksum\n");
    	return 0;
    }
    rsb->checksum = strlen(rsb->name);
    if (rsb->checksum > ROMFS_MAXFN) rsb->checksum = ROMFS_MAXFN;
    rsb->checksum += (ROMFH_SIZE + 1 + ROMFH_PAD);
    rsb->checksum &= ROMFH_MASK;
    rsb->word0 = -1;
    rsb->word1 = -1;
    rsb->name[0] = 0;
    return rsb;
}

static int romfs_copyfrom(romfs_filsys fs, void *dest, unsigned long offset, unsigned long count)
{
    int off;
    struct romfs_super_block *rsb = SUPROMFS;

    for (;;) {
	if (rsb->word0 != (__u32)-1 && offset >= rsb->word0 && offset < rsb->word0 + 1024) {
	    int cnt = 1024 - (offset & 1023);
	    if (count < cnt)
		cnt = count;
	    memcpy(dest, (char *)rsb + 512 + (offset & 1023), cnt);
	    if (count == cnt) return 0;
	    dest = (char *)dest + cnt;
	    offset += cnt;
	    count -= cnt;
	}
	if (rsb->word1 != (__u32)-1 && offset >= rsb->word1 && offset < rsb->word1 + 1024) {
	    int cnt = 1024 - (offset & 1023);
	    if (count < cnt)
		cnt = count;
	    memcpy(dest, (char *)rsb + 1536 + (offset & 1023), cnt);
	    if (count == cnt) return 0;
	    dest = (char *)dest + cnt;
	    count -= cnt;
	}
	off = offset & ~1023;
	if (io_channel_read_blk (fs->io, off / 512, 2, (char *)rsb + (rsb->name[0] ? 1536 : 512))) {
	    if (rsb->name[0])
		rsb->word1 = -1;
	    else
		rsb->word0 = -1;
	    return -1;
	}
	if (rsb->name[0])
	    rsb->word1 = off;
	else
	    rsb->word0 = off;
	rsb->name[0] ^= 1;
    }
}

int romfs_open (char *device, io_manager iom, romfs_filsys *pfs)
{
    romfs_filsys fs;
    fs = (romfs_filsys) malloc (sizeof (struct struct_ext2_filsys));
    if (!fs) return -1;
    if (iom->open (device, 0, &fs->io)) return -1;
    io_channel_set_blksize (fs->io, 512);
    fs->io->private_data = romfs_read_super(fs);
    if (!fs->io->private_data) return -1;
    *pfs = fs;
    return 0;
}

int romfs_read_inode (romfs_filsys fs, ino_t inode, struct romfs_inode *ui)
{
    struct romfs_inode romfsip;
    struct romfs_super_block *rsb = SUPROMFS;

    if (inode < rsb->checksum || inode >= rsb->size)
	return -1;

    if (romfs_copyfrom (fs, &romfsip, inode, 16))
    	return -1;
    *ui = romfsip;
    return 0;
}

static int romfs_lookup (romfs_filsys fs, ino_t dir, struct romfs_inode *dirui,
		       const char *name, int len, ino_t *result)
{
    char buffer [8192];
    struct romfs_inode ui;

    dir = dirui->spec & ROMFH_MASK;
    while (dir) {
    	if (romfs_read_inode (fs, dir, &ui))
    	    return -1;
        if (romfs_copyfrom (fs, buffer, dir + 16, ROMFS_MAXFN))
    	    return -1;
    	if ((!len && buffer[0] == '.' && !buffer[1]) ||
    	    (strlen(buffer) == len && !memcmp(buffer, name, len))) {
    	    	if ((ui.next & ROMFH_TYPE) == ROMFH_HRD)
    	    	    dir = ui.spec;
    	    	*result = dir;
    	    	return 0;
    	    }
    	dir = ui.next & ROMFH_MASK;
    }
    return -1;
}

static int link_count = 0;

static int open_namei(romfs_filsys, const char *, ino_t *, ino_t);

static int romfs_follow_link(romfs_filsys fs, ino_t dir, ino_t inode,
			   struct romfs_inode *ui, ino_t *res_inode)
{
    int error;
    char buffer[1024];

    if ((ui->next & ROMFH_TYPE) != ROMFH_SYM) {
	*res_inode = inode;
	return 0;
    }
    if (link_count > 5) {
        printf ("Symlink loop\n");
        return -1; /* Loop */
    }
    if (romfs_copyfrom (fs, buffer, inode + 16, ROMFS_MAXFN))
    	return -1;
    error = inode + 16 + ((strlen(buffer) + 16) & ~15);
    if (romfs_copyfrom (fs, buffer, error, ROMFS_MAXFN))
    	return -1;
    link_count++;
    error = open_namei (fs, buffer, res_inode, dir);
    link_count--;
    return error;
}

static int dir_namei(romfs_filsys fs, const char *pathname, int *namelen, 
		     const char **name, ino_t base, ino_t *res_inode)
{
    char c;
    const char *thisname;
    int len;
    struct romfs_inode ub;
    ino_t inode;

    if ((c = *pathname) == '/') {
	base = (ino_t)fs->private;
	pathname++;
    }
    if (romfs_read_inode (fs, base, &ub)) return -1;
    while (1) {
	thisname = pathname;
	for(len=0;(c = *(pathname++))&&(c != '/');len++);
	if (!c) break;
	if (romfs_lookup (fs, base, &ub, thisname, len, &inode)) return -1;
	if (romfs_read_inode (fs, inode, &ub)) return -1;
	if (romfs_follow_link (fs, base, inode, &ub, &base)) return -1;
	if (base != inode && romfs_read_inode (fs, base, &ub)) return -1;
    }
    *name = thisname;
    *namelen = len;
    *res_inode = base;
    return 0;
}

static int open_namei(romfs_filsys fs, const char *pathname, 
		      ino_t *res_inode, ino_t base)
{
    const char *basename;
    int namelen;
    ino_t dir, inode;
    struct romfs_inode ub;

    if (dir_namei(fs, pathname, &namelen, &basename, base, &dir)) return -1;
    if (!namelen) {			/* special case: '/usr/' etc */
	*res_inode=dir;
	return 0;
    }
    if (romfs_read_inode (fs, dir, &ub)) return -1;
    if (romfs_lookup (fs, dir, &ub, basename, namelen, &inode)) return -1;
    if (romfs_read_inode (fs, inode, &ub)) return -1;
    if (romfs_follow_link (fs, dir, inode, &ub, &inode)) return -1;
    *res_inode = inode;
    return 0;
}

int romfs_namei (romfs_filsys fs, ino_t root, ino_t cwd, char *filename, ino_t *inode)
{
    fs->private = (void *)root;
    link_count = 0;
    return open_namei (fs, filename, inode, cwd);
}

void romfs_close(romfs_filsys fs)
{
    free (fs->io);
    free (fs);
}

int romfs_block_iterate(romfs_filsys fs, ino_t inode, 
		      int (*func)(romfs_filsys, blk_t *, int, void *), 
		      void *private)
{
    struct romfs_inode ub;
    int i;
    blk_t nr;
    int size;
    char buffer[ROMFS_MAXFN];
    
    if (romfs_read_inode (fs, inode, &ub)) return -1;
    if (romfs_copyfrom (fs, buffer, inode + 16, ROMFS_MAXFN)) return -1;
    nr = inode + 16 + ((strlen(buffer) + 16) & ~15);
    if (nr & 511) {
    	printf("romfs: File not aligned on a 512B boundary\n");
    	return -1;
    }
    size = (ub.size + 511) / 512;
    nr /= 512;
    for (i = 0; i < size; i++, nr++) {
        switch ((*func) (fs, &nr, i, private)) {
            case BLOCK_ABORT:
            case BLOCK_ERROR:
            	return -1;
        }
    }
    return 0;
}
