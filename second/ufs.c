/* UFS filesystem handling
   
   Copyright (C) 1996 Adrian Rodriguez
                 1996 Jakub Jelinek
   
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
#  include <sys/time.h>
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

#include "ext2fs/ext2fs.h"
#include "ufs.h"
#ifndef S_ISLNK
#include <sys/stat.h>
#endif

#include "stringops.h"

#define SUPUFS (struct ufs_superblock *)(fs->io->private_data)
#define cgstart(cg) ((sb->fs_fpg * (cg)) + sb->fs_cgoffset * ((cg) & ~(sb->fs_cgmask)))
#define cgimin(cg) (cgstart(cg) + sb->fs_iblkno)
#define cgdmin(cg) (cgstart(cg) + sb->fs_dblkno)
#define ino2cg(ino) ((ino) / sb->fs_ipg)

static struct ufs_superblock *ufs_read_super(ufs_filsys fs)
{
    struct ufs_superblock *usb;

    usb = (struct ufs_superblock *) malloc (2048);
    if (!usb) return 0;
    if (io_channel_read_blk (fs->io, UFS_SBLOCK/1024, -2048, (char *)usb))
        return 0;
    if (usb->fs_magic != UFS_MAGIC) {
	/* XXX - replace hard-coded constant with a byte-swap macro */
	if (usb->fs_magic == 0x54190100) {
	}
	return 0;
    }
    if (usb->fs_bsize != UFS_BSIZE)
        return 0;
    if (usb->fs_fsize != UFS_FSIZE)
        return 0;
    io_channel_set_blksize (fs->io, usb->fs_fsize);
    return usb;
}

int ufs_open (char *device, io_manager iom, ufs_filsys *pfs)
{
    ufs_filsys fs;
    fs = (ufs_filsys) malloc (sizeof (struct struct_ext2_filsys));
    if (!fs) return -1;
    if (iom->open (device, 0, &fs->io)) return -1;
    io_channel_set_blksize (fs->io, 1024);
    fs->io->private_data = ufs_read_super(fs);
    if (!fs->io->private_data) return -1;
    *pfs = fs;
    return 0;
}

int ufs_read_inode (ufs_filsys fs, ino_t inode, struct ufs_inode *ui)
{
    struct ufs_inode *ufsip;
    struct ufs_superblock *sb = SUPUFS;
    char *buffer;

    if (inode < 2 || inode > (sb->fs_ncg * sb->fs_ipg - 1))
	return -1;

    ufsip = (struct ufs_inode *) malloc (1024);
    buffer = (char *) ufsip;
    if (io_channel_read_blk (fs->io, 
    	cgimin (ino2cg(inode)) + (inode % sb->fs_ipg) / (sb->fs_inopb / sb->fs_frag), 
    	-1024, (char *)ufsip)) {
    	printf ("Couldn't read inode\n");
        return -1;
    }
    ufsip += (inode%(sb->fs_inopb / sb->fs_frag));
    *ui = *ufsip;
    free (buffer);
    return 0;
}

static int block_bmap (ufs_filsys fs, int block, int nr)
{
    struct ufs_superblock *sb = SUPUFS;
    int tmp = nr >> (sb->fs_fshift - 2);
    static int lastbuftmp = -1;
    static __u32 *lastdata = 0;
    
    nr &= ~(sb->fs_fmask) >> 2;
    if (block + tmp != lastbuftmp) {
        if (!lastdata) lastdata = (__u32 *) malloc (sb->fs_fsize);
        lastbuftmp = block + tmp;
        if (io_channel_read_blk (fs->io, block + tmp, -sb->fs_fsize, lastdata))
            return 0;
    }
    return lastdata[nr];
}

static int ufs_bmap (ufs_filsys fs, ino_t inode, struct ufs_inode *ui, int block)
{
    struct ufs_superblock *sb = SUPUFS;
    int i;
    int addr_per_block = sb->fs_bsize >> 2;
    int addr_per_block_bits = sb->fs_bshift - 2;
    int lbn = block >> (sb->fs_bshift - sb->fs_fshift);
    int boff = (block & ((sb->fs_fmask - sb->fs_bmask) >> sb->fs_fshift));

    if (lbn < 0) return 0;
    if (lbn >= UFS_NDADDR + addr_per_block +
	(1 << (addr_per_block_bits * 2)) +
	((1 << (addr_per_block_bits * 2)) << addr_per_block_bits))
	return 0;
    if (lbn < UFS_NDADDR)
	return ufsi_db(ui)[lbn] + boff;
    lbn -= UFS_NDADDR;
    if (lbn < addr_per_block) {
	i = ufsi_ib(ui)[0];
	if (!i)
	    return 0;
	return block_bmap (fs, i, lbn) + boff;
    }
    lbn -= addr_per_block;
    if (lbn < (1 << (addr_per_block_bits * 2))) {
	i = ufsi_ib(ui)[1];
	if (!i) return 0;
	i = block_bmap (fs, i, lbn >> addr_per_block_bits);
	if (!i) return 0;
	return block_bmap (fs, i, lbn & (addr_per_block-1)) + boff;
    }
    lbn -= (1 << (addr_per_block_bits * 2));
    i = ufsi_ib(ui)[2];
    if (!i) return 0;
    i = block_bmap (fs, i, lbn >> (addr_per_block_bits * 2));
    if (!i) return 0;
    i = block_bmap (fs, i, (lbn >> addr_per_block_bits) & (addr_per_block - 1));
    if (!i) return 0;
    return block_bmap (fs, i, lbn & (addr_per_block-1)) + boff;
}

static int ufs_match (int len, const char *const name, struct ufs_direct * d)
{
    if (!d || len > UFS_MAXNAMLEN) return 0;
    if (!len && (ufsd_namlen(d) == 1) && (d->d_name[0] == '.') && (d->d_name[1] == '\0'))
	return 1;
    if (len != ufsd_namlen(d)) return 0;
    return !memcmp(name, d->d_name, len);
}

static int ufs_lookup (ufs_filsys fs, ino_t dir, struct ufs_inode *dirui,
		       const char *name, int len, ino_t *result)
{
    unsigned long int lfragno, fragno;
    struct ufs_direct * d;
    char buffer [8192];
    struct ufs_superblock *sb = SUPUFS;

    for (lfragno = 0; lfragno < (dirui->ui_blocks)>>1; lfragno++) {
	fragno = ufs_bmap(fs, dir, dirui, lfragno);
	if (!fragno) return -1;
	if (io_channel_read_blk (fs->io, fragno, -sb->fs_fsize, buffer)) {
	    printf ("Couldn't read directory\n");
	    return -1;
	}
	d = (struct ufs_direct *)buffer;
	while (((char *)d - buffer + d->d_reclen) <= sb->fs_fsize) {
	    if (!d->d_reclen || !ufsd_namlen(d)) break;
	    if (ufsd_namlen(d) == len && ufs_match(len, name, d)) {
	        *result = d->d_ino;
	        return 0;
	    }
	    d = (struct ufs_direct *)((char *)d + d->d_reclen);
	}
    }
    return -1;
}

static int link_count = 0;

static int open_namei(ufs_filsys, const char *, ino_t *, ino_t);

static int ufs_follow_link(ufs_filsys fs, ino_t dir, ino_t inode,
			   struct ufs_inode *ui, ino_t *res_inode)
{
    unsigned long int block;
    int error;
    char *link;
    char buffer[1024];

    if (!S_ISLNK(ui->ui_mode)) {
	*res_inode = inode;
	return 0;
    }
    if (link_count > 5) {
        printf ("Symlink loop\n");
        return -1; /* Loop */
    }
    if (ui->ui_blocks) {
	/* read the link from disk */
	block = ufs_bmap(fs, inode, ui, 0);
	
	if (io_channel_read_blk (fs->io, block, -1024, buffer)) {
	    printf ("Couldn't readlink\n");
	    return -1;
	}
	link = buffer;
    } else {
	/* fast symlink */
	link = (char *)&(ufsi_db(ui)[0]);
    }
    link_count++;
    error = open_namei (fs, link, res_inode, dir);
    link_count--;
    return error;
}

static int dir_namei(ufs_filsys fs, const char *pathname, int *namelen, 
		     const char **name, ino_t base, ino_t *res_inode)
{
    char c;
    const char *thisname;
    int len;
    struct ufs_inode ub;
    ino_t inode;

    if ((c = *pathname) == '/') {
	base = (ino_t)fs->private;
	pathname++;
    }
    if (ufs_read_inode (fs, base, &ub)) return -1;
    while (1) {
	thisname = pathname;
	for(len=0;(c = *(pathname++))&&(c != '/');len++);
	if (!c) break;
	if (ufs_lookup (fs, base, &ub, thisname, len, &inode)) return -1;
	if (ufs_read_inode (fs, inode, &ub)) return -1;
	if (ufs_follow_link (fs, base, inode, &ub, &base)) return -1;
	if (base != inode && ufs_read_inode (fs, base, &ub)) return -1;
    }
    *name = thisname;
    *namelen = len;
    *res_inode = base;
    return 0;
}

static int open_namei(ufs_filsys fs, const char *pathname, 
		      ino_t *res_inode, ino_t base)
{
    const char *basename;
    int namelen;
    ino_t dir, inode;
    struct ufs_inode ub;

    if (dir_namei(fs, pathname, &namelen, &basename, base, &dir)) return -1;
    if (!namelen) {			/* special case: '/usr/' etc */
	*res_inode=dir;
	return 0;
    }
    if (ufs_read_inode (fs, dir, &ub)) return -1;
    if (ufs_lookup (fs, dir, &ub, basename, namelen, &inode)) return -1;
    if (ufs_read_inode (fs, inode, &ub)) return -1;
    if (ufs_follow_link (fs, dir, inode, &ub, &inode)) return -1;
    *res_inode = inode;
    return 0;
}

int ufs_namei (ufs_filsys fs, ino_t root, ino_t cwd, char *filename, ino_t *inode)
{
    fs->private = (void *)root;
    link_count = 0;
    return open_namei (fs, filename, inode, cwd);
}

void ufs_close(ufs_filsys fs)
{
    free (fs->io);
    free (fs);
}

int ufs_block_iterate(ufs_filsys fs, ino_t inode, 
		      int (*func)(ufs_filsys, blk_t *, int, void *), 
		      void *private)
{
    struct ufs_inode ub;
    int i;
    blk_t nr;
    int frags;
    struct ufs_superblock *sb = SUPUFS;
    
    if (ufs_read_inode (fs, inode, &ub)) return -1;
    frags = (ufsi_size(&ub) + sb->fs_fsize - 1) / sb->fs_fsize;
    for (i = 0; i < frags; i++) {
        nr = ufs_bmap (fs, inode, &ub, i);
        if (!nr) return -1;
        switch ((*func) (fs, &nr, i, private)) {
            case BLOCK_ABORT:
            case BLOCK_ERROR:
            	return -1;
        }
    }
    return 0;
}
