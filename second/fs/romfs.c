/* Linux ROMFS Interface for SILO filesystem access routines
   
   Copyright (C) 1998 Jakub Jelinek <jj@ultra.linux.cz>
                 1997 Janos Farkas <chexum@shadow.banki.hu>
		 2001 Ben Collins <bcollins@debian.org>
   
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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
   USA.  */

#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <silo.h>
#include <file.h>
#include <stringops.h>
#include <linux/romfs_fs.h>

/* Reuse and abuse */
typedef ext2_filsys romfs_filsys;

static ino_t inode = 0;

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

static int romfs_read_inode (romfs_filsys fs, ino_t inode, struct romfs_inode *ui)
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

struct fs_ops rom_fs_ops;

static int namei_follow_romfs (const char *filename)
{
    int ret;
    
    fs->private = (void *)root;
    link_count = 0;

    ret = open_namei (fs, filename, &inode, root);
    rom_fs_ops.have_inode = (ret) ? 0 : 1;

    return ret;
}

static void romfs_close(romfs_filsys fs)
{
    free (fs->io);
    free (fs);
}

static int romfs_block_iterate(int (*func)(blk_t *, int))
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
        switch ((*func) (&nr, i)) {
            case BLOCK_ABORT:
            case BLOCK_ERROR:
            	return -1;
        }
    }
    return 0;
}

static int open_romfs (char *device)
{
    fs = (romfs_filsys) malloc (sizeof (struct struct_ext2_filsys));
    if (!fs)
	return 0;

    if (((struct struct_io_manager *)(silo_io_manager))->open (device, 0, &fs->io))
	return 0;

    io_channel_set_blksize (fs->io, 512);

    fs->io->private_data = romfs_read_super(fs);
    if (!fs->io->private_data)
	return 0;

    root = ((struct romfs_super_block *)(fs->io->private_data))->checksum;

    return 1;
}

static int dump_romfs (char *filename)
{
    printf(__FUNCTION__": called\n");
    if (romfs_block_iterate (dump_block)) {
	printf ("Error while loading of %s", filename);
	return 0;
    }
    printf(__FUNCTION__": romfs_block_iterate done, calling dump_finish\n");
    return dump_finish ();
}

static int ino_size_romfs (void)
{
    struct romfs_inode ri;

    if (romfs_read_inode (fs, inode, &ri))
	return 0;
    if ((ri.next & ROMFH_TYPE) != ROMFH_REG) {
	printf("romfs: get length on non-reg file?\n");
	return 0;
    }
    return ri.size;
}

static void print_error_romfs (int error_val) {
    printf("Unknown romfs error");
}

struct fs_ops rom_fs_ops = {
    name:               "Linux ROMFS",
    open:               open_romfs,
    ls:                 NULL/*ls_romfs*/,
    dump:               dump_romfs,
    close:              romfs_close,
    ino_size:           ino_size_romfs,
    print_error:        print_error_romfs,
    namei_follow:       namei_follow_romfs,
    have_inode:         0,
};
