/* File related stuff
   
   Copyright (C) 1996 Maurizio Plaza
   		 1996,1997,1999 Jakub Jelinek
   
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

/* TODO: This file is a good candidate for rewrite from scratch */

#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <silo.h>
typedef int FILE;
#include <linux/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#define ec ,},{
static struct {
    int errnum;
    char *desc;
} ext2errors[] = { 
{ 0, "OK"
#include <ext2fs/ext2_err.et>
}
};
#include <stringops.h>
#include "ufs.h"
#include "romfs.h"
#include "9660.h"
#include <setjmp.h>

static errcode_t linux_open (const char *name, int flags, io_channel * channel);
static errcode_t linux_close (io_channel channel);
static errcode_t linux_set_blksize (io_channel channel, int blksize);
static errcode_t linux_read_blk (io_channel channel, unsigned long block, int count, void *data);
static errcode_t linux_write_blk (io_channel channel, unsigned long block, int count, const void *data);
static errcode_t linux_flush (io_channel channel);

static struct struct_io_manager struct_linux_manager =
{
    EXT2_ET_MAGIC_IO_MANAGER,
    "linux I/O Manager",
    linux_open,
    linux_close,
    linux_set_blksize,
    linux_read_blk,
    linux_write_blk,
    linux_flush
};

ext2_filsys fs = 0;
static ino_t root, cwd;
static io_manager linux_io_manager = &struct_linux_manager;
static int do_gunzip = 0;
static unsigned int *gzipped_blocks;
static unsigned int *cur_gzipped_block;

static unsigned int bs;		/* Block Size */
static unsigned long long doff;	/* Block where partition starts */

static unsigned char *filebuffer;
static unsigned char *filelimit;
static int first_block;
static int block_no;
static int block_cnt;
static int last_blockcnt;
static char *gunzip_buffer;
static char *gunzip_inp;
static char *gunzip_endbuf;
static enum { ext2, ufs, romfs, isofs } type;

extern int solaris;

static int read_sun_partition (int partno)
{
    int rc;
    sun_partition sdl;
    unsigned short csum, *ush;

    rc = read ((char *) &sdl, 512, 0);
    if (rc != 512) {
	fatal ("Cannot read partition");
	return 0;
    }
    if (sdl.magic != SUN_LABEL_MAGIC)
	fatal ("Wrong disklabel magic");
    for (csum = 0, ush = ((unsigned short *) ((&sdl) + 1)) - 1; ush >= (unsigned short *) &sdl;)
	csum ^= *ush--;
    if (csum)
	printf ("\nWarning: Your disklabel has wrong checksum. Use fdisk to correct it.");
    doff = (((unsigned long long)sdl.ntrks) * sdl.nsect * sdl.partitions[partno - 1].start_cylinder) << 9;
    return 1;
}

void com_err (const char *a, long i, const char *fmt,...)
{
    printf ((char *) fmt);
}

static errcode_t linux_open (const char *name, int flags, io_channel * channel)
{
    int partno;
    io_channel io;

    if (!name)
	return EXT2_ET_BAD_DEVICE_NAME;
    io = (io_channel) malloc (sizeof (struct struct_io_channel));
    if (!io)
	return EXT2_ET_BAD_DEVICE_NAME;
    memset (io, 0, sizeof (struct struct_io_channel));
    io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
    io->manager = linux_io_manager;
    io->name = (char *) malloc (strlen (name) + 1);
    strcpy (io->name, name);
    io->block_size = bs;
    io->read_error = 0;
    io->write_error = 0;

    doff = 0LL;
    if (strncmp (name, "/dev/fd0", 8) && partitionable()) {
        partno = *(name + strlen (name) - 1) - '0';
        if (partno && !read_sun_partition (partno))
	    return EXT2_ET_BAD_DEVICE_NAME;
    }
    *channel = io;
    return 0;
}

static errcode_t linux_close (io_channel channel)
{
    return 0;
}

static errcode_t linux_set_blksize (io_channel channel, int blksize)
{
    channel->block_size = bs = blksize;
    return 0;
}

static errcode_t linux_read_blk (io_channel channel, unsigned long block, int count, void *data)
{
    int size;

    size = (count < 0) ? -count : count * bs;
    if (read (data, size, ((unsigned long long)block) * bs + doff) != size) {
	printf ("\nRead error on block %d\n", block);
	return EXT2_ET_SHORT_READ;
    }
    return 0;
}

static errcode_t linux_write_blk (io_channel channel, unsigned long block, int count, const void *data)
{
    return 0;
}

static errcode_t linux_flush (io_channel channel)
{
    return 0;
}

static void ext2fs_error (int errcode)
{
    int i;
    
    for (i = 0; i < sizeof (ext2errors) / sizeof (ext2errors[0]); i++)
    	if (ext2errors [i].errnum == errcode) {
    	    printf ("%s", ext2errors [i].desc);
    	    return;
    	}
    printf ("Unknown ext2 error");
}

static int open_ext2 (char *device)
{
    int retval;

    retval = ext2fs_open (device, EXT2_FLAG_RW, 0, 0, linux_io_manager, &fs);
    if (retval == EXT2_ET_BAD_MAGIC)
        return 0;
    if (retval) {
        printf ("\n");
        ext2fs_error (retval);
        printf ("\n");
	return 0;
    }
    root = EXT2_ROOT_INO;
    return 1;
}

static int open_ufs (char *device)
{
    if (ufs_open (device, linux_io_manager, &fs)) return 0;
    root = UFS_ROOTINO;
    return 1;
}

static int open_romfs (char *device)
{
    if (romfs_open (device, linux_io_manager, &fs)) return 0;
    root = ((struct romfs_super_block *)(fs->io->private_data))->checksum;
    return 1;
}

static int open_isofs (char *device)
{
    return !iso9660_open (device, linux_io_manager, &fs);
}

static int dump_block (ext2_filsys fs, blk_t * blocknr, int blockcnt,
		       void *private)
{
    if (blockcnt < 0)
	return 0;

    if (!first_block && do_gunzip) {
    	if (blockcnt != last_blockcnt + 1) {
            int i;
            
            for (i = blockcnt - last_blockcnt - 1; i > 0; i--)
            	*cur_gzipped_block++ = 0xffffffff;
        }
	*cur_gzipped_block++ = *blocknr;
	last_blockcnt = blockcnt;
	return 0;
    }
    if (*blocknr || block_no) {
	if (first_block || !*blocknr || blockcnt != last_blockcnt + 1 || (block_no && *blocknr != block_no + block_cnt)) {
	    if (first_block) {
		block_no = *blocknr;
		block_cnt = 1;
		if (blockcnt) {
		    fatal ("File cannot have a hole at beginning");
		    return BLOCK_ABORT;
		}
		last_blockcnt = -1;
	    }
	    if (filebuffer + (block_cnt + ((*blocknr) ? (blockcnt - last_blockcnt - 1) : 0)) * bs > filelimit) {
		fatal ("Image too large to fit in destination");
		return BLOCK_ABORT;
	    }
	    if (block_cnt > 0 && io_channel_read_blk (fs->io, block_no, block_cnt, filebuffer))
		return BLOCK_ABORT;
	    if (first_block) {
		first_block = 0;
		last_blockcnt = 0;
		if (*filebuffer == 037 && (filebuffer[1] == 0213 || filebuffer[1] == 0236)) {	/* gzip magic */
		    unsigned long sa = (unsigned long)&_start;
		    gunzip_buffer = malloc (16 * bs);
		    memcpy (gunzip_buffer, filebuffer, bs);
		    gzipped_blocks = (unsigned int *) malloc ((sa / 512) * sizeof (int));
		    cur_gzipped_block = gzipped_blocks;
		    *cur_gzipped_block++ = *blocknr;
		    printf ("Uncompressing image...\n");
		    return 0;
		}
		do_gunzip = 0;
		filebuffer += bs;
		block_no = 0;
		block_cnt = 0;
		return 0;
	    }
	    filebuffer += block_cnt * bs;
	    if (*blocknr && blockcnt && blockcnt != last_blockcnt + 1) {
	    	memset (filebuffer, 0, (blockcnt - last_blockcnt - 1) * bs);
	    	filebuffer += (blockcnt - last_blockcnt - 1) * bs;
	    }
	    block_no = 0;
	}
	if (*blocknr) {
	    if (!block_no) {
		block_no = *blocknr;
		block_cnt = 1;
	    } else
		block_cnt++;
	    last_blockcnt = blockcnt;
	}
    }
    return 0;
}

extern jmp_buf gunzip_env;
static unsigned char get_gzip_input (void)
{

    if (gunzip_inp < gunzip_endbuf)
	return *gunzip_inp++;
    {
	int count = 1;
	unsigned int first = *cur_gzipped_block++;

	if (!first) {
	    printf ("\nDecompression error: run out of compressed data\n");
	    longjmp (gunzip_env, 1);
	}
	if (first == 0xffffffff) {
	    /* Hole */
	    
	    while (*cur_gzipped_block == 0xffffffff && count < 16) {
	    	count++;
	    	cur_gzipped_block++;
	    }
	    memset (gunzip_buffer, 0, count * bs);
	} else {
	    while (*cur_gzipped_block == first + count && count < 16) {
	        count++;
	        cur_gzipped_block++;
	    }
	    if (io_channel_read_blk (fs->io, first, count, gunzip_buffer)) {
	        printf ("\nRead error\n");
	        longjmp (gunzip_env, 1);
	    }
	}
	gunzip_endbuf = gunzip_buffer + count * bs;
	gunzip_inp = gunzip_buffer;
    }
    return *gunzip_inp++;
}

static void unget_gzip_input (void)
{
    if (gunzip_inp > gunzip_buffer)
	gunzip_inp--;
    else {
	printf ("\nInternal error\n");
	longjmp (gunzip_env, 1);
    }
}

static int gunzipped_len = 0;

static int dump_finish (void)
{
    if (block_no) {
	blk_t tmp = 0;
	if (dump_block (fs, &tmp, 0, 0))
	    return 0;
    }
    if (do_gunzip) {
	*cur_gzipped_block++ = 0;
	cur_gzipped_block = gzipped_blocks + 1;
	gunzip_endbuf = gunzip_buffer + bs;
	gunzip_inp = gunzip_buffer;
	if ((gunzipped_len = decompress (filebuffer, filelimit, get_gzip_input, unget_gzip_input)) < 0) {
	    free (gzipped_blocks);
	    free (gunzip_buffer);
	    return 0;
	}
    }
    return 1;
}

static int dump_ext2 (ino_t inode, char *filename)
{
    errcode_t retval;

    retval = ext2fs_block_iterate (fs, inode, 0, 0,
				   dump_block, 0);
    if (retval) {
        printf ("\n");
    	ext2fs_error (retval);
        printf ("\n");
	return 0;
    }
    return dump_finish ();
}

static int ls_ext2_proc(struct ext2_dir_entry *dirent, int offset,
			int blocksize, char *buf, void *private)
{
    struct silo_inode *sino = (struct silo_inode *)filebuffer;
    struct ext2_inode ino;
    unsigned char *p;

    strncpy(sino->name, dirent->name, dirent->name_len & 0xFF);
    sino->name[dirent->name_len & 0xFF] = 0;
    if (ext2fs_read_inode(fs, dirent->inode, &ino))
	strcpy (sino->name, "--- error ---");
    sino->mtime = ino.i_mtime;
    sino->size = ino.i_size;
    sino->mode = ino.i_mode;
    sino->uid = ino.i_uid;
    sino->gid = ino.i_gid;
    p = strchr (sino->name, 0) + 1;
    if (LINUX_S_ISLNK (ino.i_mode)) {
	if (ino.i_blocks) {
	    if (io_channel_read_blk(fs->io, ino.i_block[0], 1, p))
		ino.i_size = 0;
	} else {
	    strncpy (p, (char *)&(ino.i_block[0]),ino.i_size);
	}
	p[ino.i_size] = 0;
	p += ino.i_size + 1;
    }
    if ((long)p & 3) p += 4 - ((long)p & 3);
    sino->inolen = p - filebuffer;
    filebuffer = p;
    return 0;
}

static int ls_ext2 (ino_t inode)
{
    errcode_t retval;
    struct silo_inode *sino;

    retval = ext2fs_dir_iterate (fs, inode, DIRENT_FLAG_INCLUDE_EMPTY,
				 0, ls_ext2_proc, 0);
    if (retval) {
        printf ("\n");
    	ext2fs_error (retval);
        printf ("\n");
	return 0;
    }
    sino = (struct silo_inode *)filebuffer;
    sino->inolen = 0;
    return 1;
}

static int dump_ufs (ino_t inode, char *filename)
{
    if (ufs_block_iterate (fs, inode, dump_block, 0)) {
	printf ("Error while loading of %s", filename);
	return 0;
    }
    return dump_finish ();
}

static int dump_romfs (ino_t inode, char *filename)
{
    if (romfs_block_iterate (fs, inode, dump_block, 0)) {
	printf ("Error while loading of %s", filename);
	return 0;
    }
    return dump_finish ();
}

static int dump_isofs (struct iso9660_inode *inode, char *filename)
{
    if (iso9660_block_iterate (fs, inode, dump_block, 0)) {
	printf ("Error while loading of %s", filename);
	return 0;
    }
    return dump_finish ();
}

int dump_device_range (char *filename, char *bogusdev, int *len, void (*lenfunc)(int, char **, char **))
{
    /* Range of blocks on physical block device */
    int start = 0, end = -1;
    char *p;

    bs = 512;
    p = strchr (filename, '-');
    filename++;
    if (p && *filename >= '0' && *filename <= '9') {
	*p = 0;
	start = atoi (filename);
	filename = p + 1;
	p = strchr (filename, ']');
	if (p && *filename >= '0' && *filename <= '9' && !p[1]) {
	    *p = 0;
	    end = atoi (filename);
	}
    }
    if (end == -1) {
    	if (prom_vers == PROM_V0)
	    printf ("\nRanges of physical blocks are specified as {device_name}{partno}[xx-yy]"
		    "\nwhere {} means optional part and xx is the starting block"
		    "\n(chunk of 512 bytes) and yy end (not inclusive, i.e. yy won't be read)\n");
	else
	    printf ("\nRanges of physical blocks are specified as {prom_path;}{partno}[xx-yy]"
		    "\nwhere {} means optional part, partno defaults to 0 (i.e. whole disk)"
		    "\nand xx is the starting block (chunk of 512 bytes) and yy end (not"
		    "\ninclusive, i.e. yy won't be read)\n");
	return 0;
    }
    if (lenfunc)
        (*lenfunc)((end - start) << 9, (char **)&filebuffer, (char **)&filelimit);
    fs = (ext2_filsys) malloc (sizeof (struct struct_ext2_filsys));
    if (fs) {
	if (!linux_open (bogusdev, 0, &fs->io)) {
	    blk_t tmp;

	    first_block = do_gunzip;
	    block_no = 0;
	    last_blockcnt = 0;
	    block_cnt = 0;
	    for (tmp = start; tmp < end; tmp++) {
		if (dump_block (fs, &tmp, tmp - start, 0))
		    break;
	    }

	    if (tmp == end && dump_finish ()) {
	        if (len) {
	            if (do_gunzip)
	                *len = gunzipped_len;
	            else
	                *len = (end - start) << 9;
	        }
		return 1;
	    }
	}
    }
    printf ("\nInternal error while loading physical blocks from device\n");
    return 0;
}

static int get_len (ino_t inode)
{
    if (type == ext2) {
	struct ext2_inode ei;
	int retval;
	    
	if ((retval = ext2fs_read_inode (fs, inode, &ei))) {
            printf ("\n");
	    ext2fs_error (retval);
            printf ("\n");
	    return 0;
	}
	return ei.i_size;
    } else if (type == ufs) {
	struct ufs_inode ui;
	    
	if (ufs_read_inode (fs, inode, &ui))
	    return 0;
	/* Hope nobody is so stupid to load 4GB+ kernel into core :)))) */
	return ufsi_size(&ui);
    } else if (type == romfs) {
    	struct romfs_inode ri;
    	
    	if (romfs_read_inode (fs, inode, &ri))
    	    return 0;
    	if ((ri.next & ROMFH_TYPE) != ROMFH_REG) {
    	    printf("romfs: get_len on non-reg file?\n");
    	    return 0;
    	}
    	return ri.size;
    } else if (type == isofs) {
    	/* XXX */
    	return 0;
    }
    return 0;
}

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

int load_file (char *device, int partno, char *filename, char *buffer, char *limit, int *len, int cmd, void (*lenfunc)(int, char **, char **))
{
    ino_t inode = 0;
    struct iso9660_inode iso_inode;
    int retval;
    int syspkg = 0;
    int size = -1;
    char bogusdev[] = "/dev/sdaX";
    char *bogdev;
    void *mmark;

    mark (&mmark);
    if (!device)
	device = bootdevice;
#if 0
    printf ("Loading %s from %d\n", filename, partno);
#endif
    bogdev = bogusdev;
    if (prom_vers == PROM_V0) {
    	if (device[0] == 'f' && device[1] == 'd') {
    	    device = "fd()";
    	    bogdev = "/dev/fd0";
    	} else
            bogusdev[8] = partno + '0';
    } else
        bogusdev[8] = partno + '0';
    if (setdisk (device) < 0)
	return 0;
    do_gunzip = cmd & 1;
    filebuffer = buffer;
    filelimit = limit;
    if (*filename == '[') {
	if (cmd & 2) {
		printf ("You cannot ls a device range\n");
		return 0;
	}
    	solaris = 0;
	retval = dump_device_range (filename, bogdev, len, lenfunc);
	release (mmark);
	return retval;
    }
    if (!open_ext2 (bogdev)) {
        if (!open_romfs (bogdev)) {
            if (!open_isofs (bogdev)) {
		if (!open_ufs (bogdev)) {
                    fatal ("Unable to open filesystem");
                    release (mmark);
                    return 0;
                } else type = ufs;
            } else
                type = isofs;
        } else
            type = romfs;
    } else
        type = ext2;
    if (type != ufs)
    	solaris = 0;
    if (type == ext2) {
        if ((retval = ext2fs_namei_follow (fs, root, root, filename, &inode))) {
	    printf ("\nCannot find %s (", filename);
	    ext2fs_error (retval);
	    printf (")\n");
	    ext2fs_close (fs);
	    release (mmark);
	    return 0;
        }
    } else if (type == romfs) {
        if (romfs_namei (fs, root, root, filename, &inode)) {
	    printf ("\nCannot find %s\n", filename);
	    release (mmark);
	    return 0;
        }
    } else if (type == isofs) {
	if (iso9660_namei (fs, filename, &iso_inode)) {
	    printf ("\nCannot find %s\n", filename);
	    release (mmark);
	    return 0;
	}
	inode = 1;
    } else if (type == ufs) {
    	cwd = root;
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
            if (!syspkg) {
	        printf ("\nCannot find %s.", filename);
	        ufs_close (fs);
	        release (mmark);
	        return 0;
	    }
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
    }
    if (lenfunc) {
        do_gunzip = 0;
        if (type == isofs)
	    size = iso_inode.size;
	else
	    size = get_len (inode);
        (*lenfunc)(size, (char **)&filebuffer, (char **)&filelimit);
        do_gunzip = cmd & 1;
    }
    first_block = do_gunzip;
    last_blockcnt = 0;
    block_no = 0;
    block_cnt = 0;
    retval = 0;
    if (inode) {
	if (cmd & 2)
	    switch (type) {
	    case ext2: retval = ls_ext2 (inode); break;
	    default: printf ("ls not supported outside of ext2\n"); retval = 0; break;
#if 0
            case ufs: retval = ls_ufs (inode); break;
            case romfs: retval = ls_romfs (inode); break;
            case isofs: retval = ls_isofs (&iso_inode); break;
#endif
	    }
	else
	    switch (type) {
            case ext2: retval = dump_ext2 (inode, filename); break;
            case ufs: retval = dump_ufs (inode, filename); break;
            case romfs: retval = dump_romfs (inode, filename); break;
            case isofs: retval = dump_isofs (&iso_inode, filename); break;
	    }
    }	    
    if (retval && len) {
	if (size != -1)
	    *len = size;
	else if (do_gunzip)
	    *len = gunzipped_len;
	else if (type == isofs)
	    *len = iso_inode.size;
	else
	    *len = get_len (inode);
    }
    switch (type) {
	case ext2: ext2fs_close (fs); break;
	case ufs: ufs_close (fs); break;
	case romfs: romfs_close (fs); break;
	case isofs: iso9660_close (fs); break;
    }
    release (mmark);
    return retval;
}
