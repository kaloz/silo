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

#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <silo.h>
#include <file.h>

#include <features.h>
#ifdef __GLIBC__
#  define _LINUX_TIME_H
#endif
#include <linux/ufs_fs.h>

/* Reuse and abuse */
typedef ext2_filsys ufs_filsys;

ino_t inode = 0;

#ifdef UFS_CIGAM
/* Apparently new header */

#define ufsi_size(x) ((unsigned int)((x)->ui_size))
#define ufsi_db(x) ((unsigned int *)((x)->ui_u2.ui_addr.ui_db))
#define ufsi_ib(x) ((unsigned int *)((x)->ui_u2.ui_addr.ui_ib))
#define ufsd_namlen(x) ((unsigned char)((x)->d_u.d_44.d_namlen))

#ifdef UFS_MINFREE
/* Apparently even newer header */
#define ufs_superblock ufs_super_block
#define ufs_direct ufs_dir_entry
#endif

#else

#define ufsi_size(x) (((x)->ui_size.val[1]))
#define ufsi_db(x) ((unsigned int *)((x)->ui_db))
#define ufsi_ib(x) ((unsigned int *)((x)->ui_ib))
#define ufsd_namlen(x) ((unsigned char)((x)->d_namlen))

#endif

#ifndef S_ISLNK
#include <sys/stat.h>
#endif

#include <stringops.h>

#define SUPUFS (struct ufs_superblock *)(fs->io->private_data)
#define cgstart(cg) ((sb->fs_fpg * (cg)) + sb->fs_cgoffset * ((cg) & ~(sb->fs_cgmask)))
#define cgimin(cg) (cgstart(cg) + sb->fs_iblkno)
#define cgdmin(cg) (cgstart(cg) + sb->fs_dblkno)
#define ino2cg(ino) ((ino) / sb->fs_ipg)

static char *get_archstr(void)
{
        char *p = "sun4c";

        switch (get_architecture()) {
        case sun4: p = "sun4"; break;
        case sun4c: p = "sun4c"; break;
        case sun4m: p = "sun4m"; break;
        case sun4d: p = "sun4d"; break;
        case sun4e: p = "sun4e"; break;
        case sun4u: p = "sun4u"; break;
        case sun4p: p = "sun4p"; break;
        default: break;
        }
        return p;
}

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

static int ufs_read_inode (ufs_filsys fs, ino_t inode, struct ufs_inode *ui)
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

static int ufs_namei (ufs_filsys fs, ino_t root, ino_t cwd, const char *filename, ino_t *inode)
{
    fs->private = (void *)root;
    link_count = 0;
    return open_namei (fs, filename, inode, cwd);
}

static void ufs_close(ufs_filsys fs)
{
    free (fs->io);
    free (fs);
}

static int ufs_block_iterate(ufs_filsys fs, ino_t inode, 
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

struct fs_ops ufs_fs_ops;

static int namei_follow_ufs (const char *filename) {
    int syspkg = 0;
    cwd = root;
    ufs_fs_ops.have_inode = 0;

    if (solaris) {
	if (!ufs_namei (fs, root, root, "/platform", &cwd)) {
	    if (!ufs_namei (fs, root, cwd, get_syspackage(), &inode)) {
		cwd = inode;
		syspkg = 1;
	    } else {
		if (!ufs_namei (fs, root, cwd, get_archstr(), &inode))
		    cwd = inode;
	    }
	}
	if (cwd != root && *filename == '/') filename++;
    }
    if (ufs_namei (fs, root, cwd, filename, &inode)) {
	if (syspkg) {
	    syspkg = 0;
	    ufs_namei (fs, root, root, "/platform", &cwd);
	    if (!ufs_namei (fs, root, cwd, get_archstr(), &inode)) {
		cwd = inode;
		if (!ufs_namei (fs, root, cwd, filename, &inode))
		    syspkg = 1;
	    }
	}
	if (!syspkg)
	    return 1;
    }
    if (solaris) {
	ino_t sinode;

	if (ufs_namei (fs, root, cwd, "ufsboot", &sinode)) {
	    printf ("\nCannot find Solaris kernel bootloader `ufsboot'. Will try to load it,\n"
		    "but it may fail\n");
	    solaris = 0;
	} else
	    inode = sinode;
    }
    ufs_fs_ops.have_inode = 0;
    return 0;
}

static int open_ufs (char *device)
{
    fs = (ufs_filsys) malloc (sizeof (struct struct_ext2_filsys));
    if (!fs)
	return 0;

    if (((struct struct_io_manager *)(silo_io_manager))->open (device, 0, &fs->io))
	return 0;

    io_channel_set_blksize (fs->io, 1024);

    if (!(fs->io->private_data = ufs_read_super(fs)))
	return 0;

    root = UFS_ROOTINO;
    solaris = 1;

    return 1;
}

static int dump_ufs (char *filename)
{
    if (ufs_block_iterate (fs, inode, dump_block, 0)) {
	printf ("Error while loading of %s", filename);
	return 0;
    }
    return dump_finish ();
}

static int ino_size_ufs (void)
{
    struct ufs_inode ui;

    if (ufs_read_inode (fs, inode, &ui))
	return 0;

    /* Hope nobody is so stupid to load 4GB+
     * kernel into core :)))) */
    return ufsi_size(&ui);
}

static void print_error_ufs (int error_val) {
    printf("Unknown ufs error");
}

struct fs_ops iso_fs_ops = {
    name:               "SunOS UFS",
    open:               open_ufs,
    ls:                 NULL/*ls_ufs*/,
    dump:               dump_ufs,
    close:              ufs_close,
    ino_size:           ino_size_ufs,
    print_error:        print_error_ufs,
    namei_follow:       namei_follow_ufs,
    have_inode:         0,
};
