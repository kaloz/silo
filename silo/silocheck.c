/* silocheck - checks whether a file is located below 1GB, so that
   SILO can load it
   
   Copyright (C) 1996 Jakub Jelinek
   
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifdef __GLIBC__
#  include <sys/stat.h>
#  define _LINUX_STAT_H
#  define _LINUX_TIME_H
#  define _LINUX_STRING_H_
#endif
#ifdef __linux__
#  include <linux/fs.h>
#  include <linux/ext2_fs.h>
#  include <scsi/scsi.h>
#  include <endian.h>
#  ifdef __GLIBC__
#    include <sys/sysmacros.h>
#    define mmajor(x) major(x)
#    define mminor(x) minor(x)
#    define mmakedev(x,y) makedev(x,y)
#  else
#    define mmajor(x) (int)((x) >> 8)
#    define mminor(x) (int)((x) & 0xff)
#    define mmakedev(major, minor) (((major) << 8) | (minor))
#  endif
#elif defined (__solaris__)
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <non-linux/ext2_fs.h>
#  include <ext2fs/ext2fs.h>
#  include "ufs.h"
#  include <limits.h>
#  include <sys/byteorder.h>
#  ifdef _BIG_ENDIAN
#    define BYTE_ORDER 4321
#  elif defined (_LITTLE_ENDIAN)
#    define BYTE_ORDER 1234
#  else
#    error "Unknown byteorder"
#  endif
static int ufs_blocks (char *, ino_t);
#else
#  error "Unknown system"
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#ifndef __intel__
#include "prom.h"
#endif

/* This is just so that we don't have to fight with incompatible ufs_fs.h headers */
#define SILO_UFS_MAGIC 0x00011954
struct silo_ufs_super_block {
	unsigned char xxx1[36];
	unsigned int fs_fsize;
	unsigned char xxx2[1332];
	unsigned int fs_magic;
};
                                
struct sun_disklabel {
    unsigned char info[128];	/* Informative text string */
    unsigned char spare[292];	/* Boot information etc. */
    unsigned short rspeed;	/* Disk rotational speed */
    unsigned short pcylcount;	/* Physical cylinder count */
    unsigned short sparecyl;	/* extra sects per cylinder */
    unsigned char spare2[4];	/* More magic... */
    unsigned short ilfact;	/* Interleave factor */
    unsigned short ncyl;	/* Data cylinder count */
    unsigned short nacyl;	/* Alt. cylinder count */
    unsigned short ntrks;	/* Tracks per cylinder */
    unsigned short nsect;	/* Sectors per track */
    unsigned char spare3[4];	/* Even more magic... */
    struct sun_partition {
	unsigned long start_cylinder;
	unsigned long num_sectors;
    } partitions[8];
    unsigned short magic;	/* Magic number */
    unsigned short csum;	/* Label xor'd checksum */
};

#if BYTE_ORDER == 4321
__u32 swab32 (__u32 value)
{
    return ((value >> 24) | ((value >> 8) & 0xff00) |
	    ((value << 8) & 0xff0000) | (value << 24));
}

__u16 swab16 (__u16 value)
{
    return (value >> 8) | (value << 8);
}
#define bswab32(x) (x)
#define bswab16(x) (x)
#else
#define swab32(x) (x)
#define swab16(x) (x)
__u32 bswab32 (__u32 value)
{
    return ((value >> 24) | ((value >> 8) & 0xff00) |
	    ((value << 8) & 0xff0000) | (value << 24));
}

__u16 bswab16 (__u16 value)
{
    return (value >> 8) | (value << 8);
}
#endif

unsigned char nsect;
unsigned short bs;
unsigned long doff;
int nblocks = 0;
__u32 blocks[16384];

void fatal (char *fmt,...)
{
    va_list ap;
    va_start (ap, fmt);
    fprintf (stderr, "Fatal error: ");
    vfprintf (stderr, fmt, ap);
    putc ('\n', stderr);
    va_end (ap);
    exit (2);
}

int check_fs (int fd)
{
    struct silo_ufs_super_block ufs;
    struct ext2_super_block sb;	/* Super Block Info */

    if (lseek (fd, 1024, 0) != 1024 || read (fd, &sb, sizeof (sb)) != sizeof (sb))
	fatal ("Cannot read Super Block!");
    if (swab16 (sb.s_magic) == EXT2_SUPER_MAGIC)
        return 1024 << swab32 (sb.s_log_block_size);
    if (lseek (fd, 8192, 0) != 8192 || read (fd, &ufs, sizeof (ufs)) != sizeof (ufs))
        return -1;
    if (bswab32 (ufs.fs_magic) == SILO_UFS_MAGIC)
        return ufs.fs_fsize;
    return -1;
}

void read_sb (char *device, char *bootdev)
{
    int fd, partno;
    char buff[512];
    struct sun_disklabel *sdl;
    int offset;

    if ((fd = open (device, O_RDONLY)) == -1)
	fatal ("Cannot open superblock on %s", device);
    bs = check_fs (fd);
    if (bs == (unsigned short)-1)
	fatal ("File systems other than ext2, ufs and romfs not yet supported", device);
    close (fd);
    nsect = bs / 512;
    doff = 0;
#ifdef __linux__    
    partno = (int) (*(device + strlen (device) - 1) - '0') - 1;
#elif defined(__solaris__)
    partno = (int) (*(device + strlen (device) - 1) - '0');
#endif
    sdl = (struct sun_disklabel *) &buff;
    if ((fd = open (bootdev, O_RDONLY)) == -1)
	fatal ("Error opening %s", bootdev);
    if (read (fd, buff, sizeof (buff)) != sizeof (buff))
        fatal ("Error reading %s's label", bootdev);
    doff = bswab16(sdl->ntrks) * bswab16(sdl->nsect) * bswab32(sdl->partitions[partno].start_cylinder);
    offset = bswab16(sdl->ntrks) * bswab16(sdl->nsect) * bswab32(sdl->partitions[0].start_cylinder);
    close (fd);
}

int get_partition_blocks (char *device, char *filename)
{
    int fd;
    int block, i, j, k;
    struct stat st;
    int size;
    
    if ((fd = open (filename, O_RDONLY)) == -1) {
	fatal ("Cannot find %s", filename);
    }
    if (fstat (fd, &st) < 0) {
        fatal ("Couldn't stat %s", filename);
    }
#ifdef __linux__
    size = st.st_size;
    for (i = 0, j = 0;; i++) {
	block = i;
	if ((j << 9) >= size || ioctl (fd, FIBMAP, &block) < 0)
	    break;
	if (!block) {
	    for (k = 0; k < nsect; k++)
	        blocks[j++] = 0;
	} else {
	    for (k = 0; k < nsect; k++)
	        blocks[j++] = block * nsect + doff + k;
	}
    }
    blocks[j] = 0;
    nblocks = j;
#elif defined(__solaris__)
    ufs_blocks (device, st.st_ino);
#endif
    close (fd);
    return 0;
}

char *find_dev(int number)
{
#ifdef __linux__
#  define DEVNAME "/dev"
#else
#  define DEVNAME "/dev/dsk"
#endif
    DIR *dp;
    char *p;
    struct dirent *dir;
    static char name[PATH_MAX+1];
    struct stat s;
    
    if (!number) return NULL;
    if ((dp = opendir(DEVNAME)) == NULL) return NULL;
    strcpy(name,DEVNAME "/");
    p = strchr (name, 0);
    while ((dir = readdir(dp)) != NULL) {
        strcpy(p,dir->d_name);
        if (stat(name,&s) < 0) return NULL;
        if (S_ISBLK(s.st_mode) && s.st_rdev == number) return name;
    }
    return NULL;
}

void usage (char *s)
{
    fprintf (stderr,
            "SILOCHECK\n"
            "Usage: %s [options] filename\n"
            "Options:\n"
            " -v 	     print disk blocks\n"
            " -c	     print disk blocks in a start-len format\n"
            ,s);
    exit (2);
}

int main(int argc,char **argv)
{
    struct stat st1;
    int i, j, printblocks = 0;
    char *name;
    char bootdev[1024], bootdev2[1024];
    char *filename;

    name = *argv++;
    argc--;
    while (argc && **argv == '-') {
	argc--;
	if (argv[0][2]) usage(name);
	switch ((*argv++)[1]) {
	    case 'v':
	    	printblocks = 1;
	        break;
	    case 'c':
	        printblocks = 2;
	        break;
	    default:
		usage(name);
	}
    }
    if (argc != 1) usage(name);
    filename = *argv;
    if (stat (filename, &st1) < 0)
        fatal ("Cannot open %s", filename);
#ifdef __linux__
    if (mmajor(st1.st_dev) == 8) {
        sprintf (bootdev2, "/dev/sd%c%c", (mminor(st1.st_dev) >> 4) + 'a', (mminor(st1.st_dev) & 0xf) + '0');
        sprintf (bootdev, "/dev/sd%c", (mminor(st1.st_dev) >> 4) + 'a');
    } else
#endif	
    {
	char *p = find_dev (st1.st_dev);

	if (!p) fatal ("Couldn't find out what device is %s on", filename);
	strcpy (bootdev, p);
	strcpy (bootdev2, p);
#ifdef __solaris__
	if (strlen (p) == strlen ("/dev/dsk/c0t0d0s0")) {
	    p = strchr (p, 0) - 2;
	    if (*p == 's' && p[1] >= '0' && p[1] <= '7') {
	        p = strchr (bootdev, 0) - 1;
	        *p = '2';
	    }
	}
#endif
    }
    read_sb (bootdev2, bootdev);
    get_partition_blocks (bootdev2, filename);
    if (printblocks) {
    	printf ("%s: ", filename);
    	for (i = 0; i < nblocks; i++) {
    	     for (j = 0; !blocks[i] && i < nblocks; i++, j++);
    	     if (j) {
    	         if (i) printf (", ");
    	         printf ("%dxHOLE", j);
    	         if (i >= nblocks) break;
    	     }
    	     if (i) printf (", ");
    	     if (printblocks == 2) {
    	     	for (j = 0; blocks[i + j] && blocks[i + j] == blocks[i] + j && j < nblocks; j++);
    	     	if (j > 1) {
    	     	    printf ("%08X(%d)", blocks[i], j);
    	     	    i += j - 1;
    	     	    continue;
    	     	}
    	     }
    	     printf ("%08X", blocks[i]);
    	}
    	printf ("\n");
    }
    for (i = 0; i < nblocks; i++) {
    	if (blocks[i] >= 0x200000) {
    	    fprintf (stderr, "%s extends past the 1GB boundary and thus SILO might not load it correctly\n", filename);
    	    exit (1);
    	}
    }
    exit(0);
}

#ifdef __solaris__

static errcode_t std_open (const char *name, int flags, io_channel * channel);
static errcode_t std_close (io_channel channel);
static errcode_t std_set_blksize (io_channel channel, int blksize);
static errcode_t std_read_blk (io_channel channel, unsigned long block, int count, void *data);
static errcode_t std_write_blk (io_channel channel, unsigned long block, int count, const void *data);
static errcode_t std_flush (io_channel channel);

static struct struct_io_manager struct_std_manager =
{
    EXT2_ET_MAGIC_IO_MANAGER,
    "linux I/O Manager",
    std_open,
    std_close,
    std_set_blksize,
    std_read_blk,
    std_write_blk,
    std_flush
};

static ufs_filsys fs = NULL;
static io_manager std_io_manager = &struct_std_manager;
static int std_fd = 0;

static unsigned int cbs = 1024;		/* Block Size */

static errcode_t std_open (const char *name, int flags, io_channel * channel)
{
    int partno;
    io_channel io;

    if (!name)
	return EXT2_ET_BAD_DEVICE_NAME;
    io = (io_channel) malloc (sizeof (struct struct_io_channel));
    if (!io)
	return EXT2_ET_BAD_DEVICE_NAME;
    std_fd = open (name, O_RDONLY);
    if (std_fd < 0)
    	fatal ("Cannot open %s", name);
    memset (io, 0, sizeof (struct struct_io_channel));
    io->magic = EXT2_ET_MAGIC_IO_CHANNEL;
    io->manager = std_io_manager;
    io->name = (char *) malloc (strlen (name) + 1);
    strcpy (io->name, name);
    io->block_size = cbs;
    io->read_error = 0;
    io->write_error = 0;

    *channel = io;
    return 0;
}

static errcode_t std_close (io_channel channel)
{
    close (std_fd);
}

static errcode_t std_set_blksize (io_channel channel, int blksize)
{
    channel->block_size = cbs = blksize;
}

static errcode_t std_read_blk (io_channel channel, unsigned long block, int count, void *data)
{
    int size;

    size = (count < 0) ? -count : count * cbs;
    if (lseek (std_fd, block * cbs, SEEK_SET) != block * cbs)
    	fatal ("Cannot seek");
    if (read (std_fd, data, size) != size)
	fatal ("Read error on block %d", block);
    return 0;
}

static errcode_t std_write_blk (io_channel channel, unsigned long block, int count, const void *data)
{
}

static errcode_t std_flush (io_channel channel)
{
}

static int ufs_block_idx = 0;
static int ufs_blocks_dump (ufs_filsys fs, blk_t *block, int i, void *private)
{
    int j;
    for (j = 0; j < nsect; j++)
        blocks [ufs_block_idx++] = *block * nsect + doff + j;
    return 0;
}

static int ufs_blocks (char *device, ino_t inode)
{
    if (ufs_open (device, std_io_manager, &fs))
    	fatal ("Cannot open ufs filesystem containing second stage");
    nsect = cbs / 512;
    if (ufs_block_iterate (fs, inode, ufs_blocks_dump, 0))
        fatal ("Block iterating error on second stage");
    blocks [ufs_block_idx] = 0;
    nblocks = ufs_block_idx;
    return 0;
}

#endif
