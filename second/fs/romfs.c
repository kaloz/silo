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
static int link_count = 0;

#define SUPROMFS (struct romfs_super_block *)(fs->io->private_data)
#define BLOCK_SIZE_BITS 9 /* 512 */
#define BLOCK_SIZE (1<<BLOCK_SIZE_BITS)

static int inline min(int a, int b)
{
	return a<b ? a : b;
}

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

    /* The 2048 comes from the space needed to make room for the first
     * stage loader. The image has to be created with "-A 2048,/.."
     */
    rsb = (struct romfs_super_block *) malloc (2048+ROMBSIZE);
    if (!rsb) return NULL;

    if (io_channel_read_blk (fs->io, 0, 1, (char *)rsb))
        return NULL;

    if (strncmp((char *)rsb, "-rom1fs-", 8) || rsb->size < ROMFH_SIZE)
        return NULL;

    if (romfs_checksum(rsb, min(rsb->size,512))) {
    	printf("ROMFS: Bad initial checksum.\n");
    	return NULL;
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
    struct romfs_super_block *rsb = SUPROMFS;
    unsigned long res;
    char buffer[ROMBSIZE];
    int maxsize;

    if ((offset>>ROMBSBITS)<<ROMBSBITS >= rsb->size+ROMBSIZE || count > rsb->size ||
	    offset+count>rsb->size+ROMBSIZE)
	return -1;

    maxsize = min(count, (ROMBSIZE - (offset & ROMBMASK)));
    res = maxsize;

    if (io_channel_read_blk (fs->io, offset>>ROMBSBITS, 1, buffer))
	return -1;

    memcpy(dest, buffer + (offset & ROMBMASK), maxsize);

    while (res < count) {
	offset += maxsize;

	if (io_channel_read_blk (fs->io, offset>>ROMBSBITS, 1, buffer))
	    return -1;

	dest += maxsize;
	maxsize = min(count-res, ROMBSIZE);

	memcpy(dest, buffer, maxsize);

	res += maxsize;
    }
    return 0;
}

static int romfs_read_inode (romfs_filsys fs, ino_t inode, struct romfs_inode *ui)
{
    struct romfs_inode romfsip;
    struct romfs_super_block *rsb = SUPROMFS;

    if (inode < rsb->checksum || inode >= rsb->size)
	return -1;

    if (romfs_copyfrom (fs, &romfsip, inode, ROMFH_SIZE))
    	return -1;

    *ui = romfsip;
    return 0;
}

static mode_t romfs_modemap[] =
{
    0, LINUX_S_IFDIR+0555, LINUX_S_IFREG+0444, LINUX_S_IFLNK+0777,
       LINUX_S_IFBLK+0600, LINUX_S_IFCHR+0600, LINUX_S_IFSOCK+0644,
       LINUX_S_IFIFO+0644
};

static int romfs_lookup (romfs_filsys fs, struct romfs_inode *dirui,
		       const char *name, int len, ino_t *result)
{
    char buffer [8192];
    struct romfs_inode ui;
    ino_t dir = dirui->spec & ROMFH_MASK;
    struct romfs_super_block *rsb = SUPROMFS;

    while (dir && dir < rsb->size) {
    	if (romfs_read_inode (fs, dir, &ui))
    	    return -1;

        if (romfs_copyfrom (fs, buffer, dir + ROMFH_SIZE, ROMFS_MAXFN))
    	    return -1;

	if (result == NULL) {
	    /* We aren't returning an inode, so we must be iterating */
	    char symlink[1024] = {0};
	    unsigned int mode = romfs_modemap[ui.next & ROMFH_TYPE];

	    /* Check for symlinks */
	    if ((ui.next & ROMFH_TYPE) == ROMFH_SYM) {
		int offset = dir + ROMFH_SIZE + ((strlen(buffer) + ROMFH_SIZE) & ROMFH_MASK);
		if (romfs_copyfrom (fs, symlink, offset, ROMFS_MAXFN))
		    return -1;
	    }

	    if (buffer[0] == '.' && (!buffer[1] || (buffer[1] == '.' && !buffer[2])))
		mode = LINUX_S_IFDIR+0555;

	    register_silo_inode(0, ui.size, mode,
				0, 0, buffer, symlink[0] ? symlink : NULL);

	} else if ((!len && buffer[0] == '.' && !buffer[1]) ||
    	    (strlen(buffer) == len && !memcmp(buffer, name, len))) {
	    if ((ui.next & ROMFH_TYPE) == ROMFH_HRD)
		dir = ui.spec;
	    *result = dir;
	    return 0;
    	}
    	dir = ui.next & ROMFH_MASK;
    }
    if (result == NULL)
	return 0;

    return -1;
}

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
        printf ("ROMFS: Symlink loop.\n");
        return -1; /* Loop */
    }
    if (romfs_copyfrom (fs, buffer, inode + ROMFH_SIZE, ROMFS_MAXFN))
    	return -1;
    error = inode + ROMFH_SIZE + ((strlen(buffer) + ROMFH_SIZE) & ROMFH_MASK);
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
	base = root;
	pathname++;
    }

    if (romfs_read_inode (fs, base, &ub)) return -1;
    while (1) {
	thisname = pathname;
	for(len=0;(c = *(pathname++))&&(c != '/');len++);
	if (!c) break;
	if (romfs_lookup (fs, &ub, thisname, len, &inode)) return -1;
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
    if (romfs_lookup (fs, &ub, basename, namelen, &inode)) return -1;
    if (romfs_read_inode (fs, inode, &ub)) return -1;
    if (romfs_follow_link (fs, dir, inode, &ub, &inode)) return -1;
    *res_inode = inode;
    return 0;
}

struct fs_ops rom_fs_ops;

static int namei_follow_romfs (const char *filename)
{
    int ret;
    
    link_count = 0;

    ret = open_namei (fs, filename, &inode, root);
    rom_fs_ops.have_inode = (ret) ? 0 : 1;

    return ret;
}

static void romfs_close(void)
{
    free (fs->io);
    free (fs);
}

static int romfs_block_iterate(void)
{
    struct romfs_inode ub;
    int i;
    blk_t nr;
    int size;
    char buffer[ROMFS_MAXFN];

    if (romfs_read_inode (fs, inode, &ub)) return 0;
    if (romfs_copyfrom (fs, buffer, inode + ROMFH_SIZE, ROMFS_MAXFN)) return 0;
    nr = inode + ROMFH_SIZE + ((strlen(buffer) + ROMFH_SIZE) & ROMFH_MASK);
    if (nr & ROMBMASK) {
    	printf("ROMFS: File not aligned on a %dB boundary.\n", ROMBSIZE);
    	return 0;
    }
    size = (ub.size + ROMBMASK) / ROMBSIZE;
    nr /= ROMBSIZE;
    for (i = 0; i < size; i++, nr++) {
        switch (dump_block (&nr, i)) {
            case BLOCK_ABORT:
            case BLOCK_ERROR:
            	return 0;
        }
    }
    return dump_finish();
}

static int open_romfs (char *device)
{
    fs = (romfs_filsys) malloc (sizeof (struct struct_ext2_filsys));
    if (!fs)
	return 0;

    if (((struct struct_io_manager *)(silo_io_manager))->open (device, 0, &fs->io))
	return 0;

    io_channel_set_blksize (fs->io, ROMBSIZE);

    fs->io->private_data = romfs_read_super(fs);
    if (!fs->io->private_data)
	return 0;

    root = ((struct romfs_super_block *)(fs->io->private_data))->checksum;
    inode = 1;

    return 1;
}

static int ino_size_romfs (void)
{
    struct romfs_inode ri;

    if (romfs_read_inode (fs, inode, &ri))
	return 0;

    if ((ri.next & ROMFH_TYPE) != ROMFH_REG)
	return 0;

    return ri.size;
}

static int ls_romfs (void)
{
    struct romfs_inode ub;
    link_count = 0;

    if (romfs_read_inode (fs, inode, &ub)) return -1;

    if (romfs_lookup (fs, &ub, NULL, 0, NULL))
	return -1;

    return 0;
}

static void print_error_romfs (int error_val) {
    printf("Unknown ROMFS error");
}

struct fs_ops rom_fs_ops = {
    name:               "Linux ROMFS",
    open:               open_romfs,
    ls:                 ls_romfs,
    dump:               romfs_block_iterate,
    close:              romfs_close,
    ino_size:           ino_size_romfs,
    print_error:        print_error_romfs,
    namei_follow:       namei_follow_romfs,
    have_inode:         0,
};
