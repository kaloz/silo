/* silo - bootblock installation program
   
   Copyright (C) 1996 Maurizio Plaza
                 1996,98,99,2000 Jakub Jelinek
   
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

#if !defined(VERSION) || !defined(IMGVERSION)
#error VERSION and IMGVERSION must be defined
#endif
#define DFL_CONFIG "/etc/silo.conf"
#define DFL_BACKUP "/boot/old.b"
#define DFL_PRIMARY "/boot/first.b"
#define DFL_PRIMARY_U "/boot/ultra.b"
#define DFL_SECONDARY "/boot/second.b"

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <features.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifdef __GLIBC__
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <sys/time.h>
#  define _LINUX_STAT_H
#  define _LINUX_VFS_H
#  define _LINUX_TIME_H
#  define _LINUX_STRING_H_
#endif
#ifdef __linux__
#  include <linux/fs.h>
#  include <linux/ext2_fs.h>
#  include <scsi/scsi.h>
#  include <sys/vfs.h>
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
#  include <md-int.h>
#elif defined (__solaris__)
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <non-linux/ext2_fs.h>
#  include <ext2fs/ext2fs.h>
#  include "../second/ufs.h"
#  include <limits.h>
struct hwdevice;
static int ufs_blocks (struct hwdevice *, ino_t);
#else
#  error "Unknown system"
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <ctype.h>
#include "floppy.h"
#ifndef __intel__
#include "prom.h"
#endif
#include "../first/first.h"
#include "../first/ultra.h"
#include "../first/fd.h"
#include "../first/ieee32.h"
#define DIGIT_OFFSET (DIGIT_OFFSET_TMP + 0x223)
#define LETTER_OFFSET (LETTER_OFFSET_TMP + 0x223)
#define NUMBER_OFFSET (NUMBER_OFFSET_TMP + 0x223)
#define ULTRA_LETTER_OFFSET (ULTRA_LETTER_OFFSET_TMP + 0x223)
#define ULTRA_NUMBER_OFFSET (ULTRA_NUMBER_OFFSET_TMP + 0x223)
#define FD_DIGIT_OFFSET (FD_DIGIT_OFFSET_TMP + 0x223)
#define FD_LETTER_OFFSET (FD_LETTER_OFFSET_TMP + 0x223)

/* Checks our config file for errors */
int confcheck(char*);
static int allow_confchk_fail = 0;

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

enum typeenum { TYPE_UNKNOWN, TYPE_SCSI, TYPE_IDE };

struct hwdevice {
    unsigned char *wholedev;
    unsigned char *dev;
    enum typeenum type;
    int id;
    int part;
    int partat0;
    unsigned char nsect;
    unsigned short bs;
    unsigned long doff;
    __u32 blocks[512];
    struct hwdevice *next;
} *hwdevs = NULL;

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

int ultra = 0;
int prombug = 1; /* Number of gigabytes PROM is able to boot from. if ultra, it is always inf. */
int raid1 = 0;
int second_b_len;
int masterboot = 1;
int floppy_image = 0;		/* Calculate cross checksums. */
char *flash_image = NULL;	/* Open this and calculate flash checksum. */
char *first, *second, *old;	/* File names */
int promver = -1;
int oldroot = -1;
enum fs_type { unknownfs, ext2fs, ufsfs, romfs } fstype = unknownfs;

void fatal (char *fmt,...)
{
    va_list ap;
    va_start (ap, fmt);
    fprintf (stderr, "Fatal error: ");
    vfprintf (stderr, fmt, ap);
    putc ('\n', stderr);
    va_end (ap);
    exit (1);
}

int devopen (const char *name, int flags)
{
    int fd = open (name, flags);
    if (fd < 0 && errno == ENOENT) {
	if (oldroot != -1) {
	    fchdir(oldroot);
	    if (*name == '/') name++;
	    fd = open (name, flags);
	    chdir("/");
	}
    }
    return fd;
}

int check_fs (int fd)
{
    struct silo_ufs_super_block ufs;
    struct ext2_super_block sb;	/* Super Block Info */
    struct romfs_super_block {
	__u32 word0;
	__u32 word1;
	__u32 size;
	__u32 checksum;
    } rsd;

    if (lseek (fd, 1024, 0) != 1024 || read (fd, &sb, sizeof (sb)) != sizeof (sb))
	fatal ("Cannot read Super Block!");
    if (swab16 (sb.s_magic) == EXT2_SUPER_MAGIC) {
        if (fstype == unknownfs) fstype = ext2fs;
        return 1024 << swab32 (sb.s_log_block_size);
    }
    if (lseek (fd, 0, 0) != 0 || read (fd, &rsd, sizeof(rsd)) != sizeof (rsd))
    	return -1;
    if (!strncmp ((char *)&rsd, "-rom1fs-", 8)) {
        if (fstype == unknownfs) fstype = romfs;
    	return 512;
    }
    if (lseek (fd, 8192, 0) != 8192 || read (fd, &ufs, sizeof (ufs)) != sizeof (ufs))
        return -1;
    if (bswab32 (ufs.fs_magic) == SILO_UFS_MAGIC) {
        if (fstype == unknownfs) fstype = ufsfs;
        return ufs.fs_fsize;
    }
    return -1;
}

void read_sb (struct hwdevice *hwdev)
{
    int fd;
    char buff[512];
    struct sun_disklabel *sdl;
    char *p;
    int i;

    hwdev->partat0 = 0;
    if ((fd = devopen (hwdev->dev, O_RDONLY)) == -1)
	fatal ("Cannot open superblock on %s", hwdev->dev);
    hwdev->bs = check_fs (fd);
    if (hwdev->bs == (unsigned short)-1)
	fatal ("File systems other than ext2, ufs and romfs not yet supported", hwdev->dev);
    close (fd);
    hwdev->nsect = hwdev->bs / 512;
    if (hwdev->part == -1)
	return;
    sdl = (struct sun_disklabel *) &buff;
    if ((fd = devopen (hwdev->wholedev, O_RDONLY)) == -1)
	fatal ("Error opening %s", hwdev->wholedev);
    if (read (fd, buff, sizeof (buff)) != sizeof (buff))
        fatal ("Error reading %s's label", hwdev->wholedev);
    for (i = 0; i < 8; i++) {
	if (i == 2) continue;
	if (!sdl->partitions[i].start_cylinder && sdl->partitions[i].num_sectors) {
	    hwdev->partat0 = i;
#ifdef __linux__
	    p = strchr (hwdev->wholedev, 0);
	    *p++ = '1' + hwdev->partat0;
	    *p = 0;
#endif
	    break;
	}
    }
    if (i == 8 && !sdl->partitions[2].start_cylinder && sdl->partitions[2].num_sectors) {
	i = 2;
	hwdev->partat0 = 2;
#ifdef __linux__
	p = strchr (hwdev->wholedev, 0);
	*p++ = '1' + hwdev->partat0;
	*p = 0;
#endif
    }
    if (i == 8) {
	for (i = 0; i < 8; i++) {
	    if (!sdl->partitions[i].start_cylinder) {
		hwdev->partat0 = i;
		break;
	    }
        }
    }
    if (i == 8) {
	fatal ("In order to install SILO you must start at least one partition on cylinder\n"
	       "0. Consider setting %s3 as WHOLE_DISK (starting at 0 and ending at disk end)\n"
	       "and %s1 starting on cylinder 0 (both will make your life easier)\n", hwdev->wholedev, hwdev->wholedev);
    }
    hwdev->doff = bswab16(sdl->ntrks) * bswab16(sdl->nsect) * bswab32(sdl->partitions[hwdev->part].start_cylinder);
    close (fd);
}

#define SECOND_B_TEMPL "%s___@#$%d___"

void gpb_cleanup(char *filename, int n)
{
    char buffer[1024];
    int i;
    
    for (i = 1; i <= n; i++) {
	sprintf (buffer, SECOND_B_TEMPL, filename, i);
	unlink (buffer);
    }
}

void make_room_for_raid1 (struct hwdevice *hwdev, char *name)
{
    char buf[512];
    int len, i;
    int fd;
    struct stat st;

    if (raid1 <= 1)
	return;
    if ((fd = open (name, O_RDWR)) == -1)
	fatal ("Cannot open %s", name);
    if (fstat (fd, &st) < 0)
        fatal ("Couldn't stat %s", name);
    if (lseek (fd, 0x90c, 0) != 0x90c || read(fd, buf, 12) != 12)
	fatal ("Could not read %s version and length header", name);
    if (memcmp (buf, "SILO", 4))
	fatal ("Second stage %s too old", name);
    len = *(int *)&buf[8];
    if (len % 512 || st.st_size < len || (st.st_size - len) % 2048)
	fatal ("Second stage %s has wrong size", name);
    i = raid1 - 1 - (st.st_size - len) / 2048;
    if (i > 0) {
	if (lseek (fd, len, 0) != len)
	    fatal ("Could not seek in %s", name);
	memset(buf, 0, 512);
	/* Should not use ftruncate here, because we cannot have fs
	   holes in */
	for (i = 4 * i; i > 0; i--) {
	    if (write (fd, buf, 512) != 512)
		fatal ("Could not write to %s", name);
	}
    }
    second_b_len = len;
    close (fd);
}

int get_partition_blocks (struct hwdevice *hwdev, char *filename)
{
    int fd;
    int block, i, j, k;
    struct stat st;
    int movecount = 0;
    char buffer[1024];
    char *name = filename;
    char *buf = NULL;
    int size = 0;
    
again:
    if ((fd = open (name, O_RDONLY)) == -1) {
    	gpb_cleanup(filename, movecount);
	fatal ("Cannot find %s", name);
    }
    if (fstat (fd, &st) < 0) {
    	gpb_cleanup(filename, movecount);
        fatal ("Couldn't stat %s", name);
    }
#ifdef __linux__
    if (!movecount)
    	size = st.st_size;
    if (fstype == romfs) {
    	char *p = strrchr (name, '/');
    	long start;
    	if (p) p++; else p = name;
    	start = (long)st.st_ino + 16 + ((strlen(p) + 16) & ~15);
    	if (start & 511)
    		fatal ("File %s on romfs not aligned on 512B boundary. Use genromfs -a -r to generate the image", name);
    	start = hwdev->doff + start / 512;
	if (flash_image) start += 1024 / 512;	/* make room for ieee32 */
    	for (j = 0; j * 512 < size; j++)
    		hwdev->blocks[j] = start + j;
    	hwdev->blocks[j] = 0;
    	close(fd);
    	return 0;
    }
    for (i = 0, j = 0;; i++) {
	block = i;
	if ((j << 9) >= size || ioctl (fd, FIBMAP, &block) < 0)
	    break;
	if (!block) {
	    if ((j << 9) < size)
	        fatal ("Filesystem holes are not yet supported for second stage loader. Mail jj@sunsite.mff.cuni.cz");
	    else
	        break;
	}
	if (prombug && block * hwdev->nsect + hwdev->doff >= 0x200000 * prombug) {
	    if (movecount < 5) {
	    	if (!movecount) {
			if (hwdev->id)
				fatal ("Your %s is located above the magic %dGB boundary from the start of the disk\n"
				       "on one of the RAID1 mirrors. Please make sure it is below the limit on all\n"
				       "mirrors.", filename, prombug);
	    		buf = malloc (size);
	    		if (!buf)
	    			fatal ("Not enough memory");
	    		if (read (fd, buf, size) != size)
	    			fatal ("Cannot read from %s", filename);
	    	}
	    	close (fd);
	    	movecount++;
	    	sprintf (buffer, SECOND_B_TEMPL, filename, movecount);
	    	name = buffer;
	    	fd = creat (name, 0644);
	    	if (!fd || write (fd, buf, size) != size) {
    			gpb_cleanup(filename, movecount);
	    		fatal ("Your %s is located above the magic %dGB boundary from the start of the disk.\n"
	    	   	       "Please move it down, so that SILO first stage loader can load it.", filename, prombug);
	    	}
	    	close (fd);
	    	goto  again;
	    }
    	    gpb_cleanup(filename, movecount);
	    fatal ("Your %s is located above the magic %dGB boundary from the start of the disk.\n"
	    	   "Please move it down, so that SILO first stage loader can load it.", filename, prombug);
	}
	for (k = 0; k < hwdev->nsect; k++)
	    hwdev->blocks[j++] = block * hwdev->nsect + hwdev->doff + k;
    }
    hwdev->blocks[j] = 0;
    if (raid1 > 1) {
	if (hwdev->id) {
	    if (!masterboot) {
		/* As current RAID1 does not reserve any space for bootblocks, non-masterboot
		   would resync away the bootblock unless second.b starts on exactly the same
		   offsets in each device */
		if (memcmp (hwdevs->blocks, hwdev->blocks, sizeof(hwdev->blocks)))
			fatal ("With silo -t on RAID1 all mirrors must start at the same offset\n"
			       "from the start of the disk.");
	    } else
		for (i = 0; i < 4; i++)
		    hwdev->blocks[i] = hwdev->blocks[second_b_len / 512 + (hwdev->id - 1) * 4 + i];
	}
    }
    if (buf) free (buf);
    if (movecount) {
    	if (rename (name, filename) < 0) {
    	    gpb_cleanup(filename, movecount - 1);
    	    fatal ("Cannot rename a suitably located copy (below 1GB from start of disk) of %s to it's old position.\n"
    	    	   "Please check %s\n", filename, filename);
    	}
    	gpb_cleanup(filename, movecount - 1);
    }
#elif defined(__solaris__)
    ufs_blocks (hwdev, st.st_ino);
#endif
    close (fd);
    return 0;
}

char *new_root = NULL;

void write_block_device (struct hwdevice *hwdev)
{
    int fd, rc;
    int offset, off;
    __u32 tmp;
    unsigned char part;

    if ((fd = devopen (masterboot ? hwdev->wholedev : hwdev->dev, O_RDWR)) == -1)
	fatal ("Cannot open %s", hwdev->dev);
    if (flash_image) off = IEEE32_OFFSET;
    else if (floppy_image) off = 1020 + 512 - 4;
    else off = 1020;
    if (lseek (fd, off, SEEK_SET) != off)
	fatal ("Seek error on %s", hwdev->dev);
    if (floppy_image) {
    	int j;
    	for (j = 0; j < 512 && hwdev->blocks[j]; j++);
    	j *= 512;
    	tmp = bswab32 (j);
    	rc = write (fd, &tmp, 4);
	if (rc != 4)
    	    fatal ("Couldn't write to %s", hwdev->dev);
    }
    tmp = bswab32 (hwdev->blocks[0]);
    rc = write (fd, &tmp, 4);
    if (rc != 4)
    	fatal ("Couldn't write to %s", hwdev->dev);
    if (!flash_image) {
        if (!ultra || floppy_image) {
    	    if (floppy_image)
    	        offset = FD_DIGIT_OFFSET;
    	    else
    	        offset = DIGIT_OFFSET;
	    if (lseek (fd, offset, SEEK_SET) != offset)
		fatal ("Seek error on %s", hwdev->dev);
	    part = hwdev->partat0 + '0';
	    rc = write (fd, &part, 1);
	    if (rc != 1)
	    	fatal ("Couldn't write to %s", hwdev->dev);
        }
        if (floppy_image)
            offset = FD_LETTER_OFFSET;
        else if (ultra)
            offset = ULTRA_LETTER_OFFSET;
        else
            offset = LETTER_OFFSET;
        if (lseek (fd, offset, SEEK_SET) != offset)
	    fatal ("Seek error on %s", hwdev->dev);
        part = hwdev->partat0 + 'a';
        rc = write (fd, &part, 1);
        if (rc != 1)
    	    fatal ("Couldn't write to %s", hwdev->dev);
    	if (!floppy_image) {
	    if (ultra)
		offset = ULTRA_NUMBER_OFFSET;
	    else
		offset = NUMBER_OFFSET;
	    if (lseek (fd, offset, SEEK_SET) != offset)
		fatal ("Seek error on %s", hwdev->dev);
	    part = 0;
	    if (raid1)
		part = hwdev->id + 1;
	    rc = write (fd, &part, 1);
	    if (rc != 1)
	    	fatal ("Couldn't write to %s", hwdev->dev);
    	}
    }
    close (fd);
}

void write_block_tables (struct hwdevice *hwdev, char *filename, char *config_file, int partno)
{
    int fd, rc, i;
    struct {
	unsigned char l;
	unsigned char partno;
	unsigned char partat0;
	unsigned char raid_dsk_number;
	unsigned char silo_conf[256];
	unsigned char silover[8];
	unsigned int len;
	unsigned char partnos[32];
	unsigned char partat0s[32];
    } buffer;
    int tordonly = 0;
    struct hwdevice *d;

    if ((fd = open (filename, O_RDWR)) == -1) {
	if (raid1)
	    fatal ("Cannot open %s", filename);
        if ((fd = open (filename, O_RDONLY)) >= 0) {
            close (fd); /* Maybe it is romfs */
            if ((fd = devopen (hwdev->wholedev, O_RDWR)) == -1)
	    	fatal ("Cannot open %s", hwdev->wholedev);
	    tordonly = 1;
        } else
	    fatal ("Cannot open %s", filename);
    }
    if (tordonly) {
        char *p = (char *)hwdev->blocks, *pend = p + sizeof (hwdev->blocks);

	if (raid1)
	    fatal ("RAID1 not supported with read-only filesystems");
        for (i = 0, rc = 0; p < pend; i++, p+=512) {
            if (lseek (fd, hwdev->blocks [i] * 512, SEEK_SET) != hwdev->blocks [i] * 512)
        	fatal ("Cannot seek in %s", filename);
            rc += write (fd, p, 512);
        }
    } else {
	if (lseek (fd, 0, SEEK_SET) != 0)
            fatal ("Cannot seek in %s", filename);
        rc = write (fd, hwdev->blocks, sizeof (hwdev->blocks));
        if (raid1 > 1) {
	    if (rc != sizeof (hwdev->blocks)) fatal ("Couldn't write to %s", filename);
	    if (lseek (fd, second_b_len, SEEK_SET) != second_b_len)
		fatal ("Cannot seek in %s", filename);
	    for (d = hwdev->next; d; d = d->next)
		rc = write (fd, d->blocks, sizeof (d->blocks));
	}
    }
    if (rc != sizeof (hwdev->blocks)) fatal ("Couldn't write to %s", filename);
    if (tordonly)
    	i = hwdev->blocks [0x808/512] * 512 + 0x808 % 512;
    else
    	i = 0x808;
    if (lseek (fd, i, SEEK_SET) != i)
        fatal ("Cannot seek in %s", filename);
    if (read (fd, &buffer, sizeof(buffer)) != sizeof(buffer))
	fatal ("Couldn't read from %s", filename);
    if (lseek (fd, i, SEEK_SET) != i)
        fatal ("Cannot seek in %s", filename);
    if (buffer.l != 'L' || memcmp(buffer.silover, "SILO", 4))
	fatal ("Corrupted %s or second stage with incorrect version", filename);
    buffer.partno = partno;
    buffer.partat0 = hwdev->partat0;
    buffer.raid_dsk_number = 0;
    strncpy (buffer.silo_conf, config_file, 255);
    buffer.silo_conf[255] = 0;
    if (raid1) {
	for (d = hwdev; d; d = d->next) {
	    buffer.partnos[d->id] = d->part + 1;
	    buffer.partat0s[d->id] = d->partat0;
	}
    }
    if (write (fd, &buffer, sizeof(buffer)) != sizeof(buffer))
        fatal ("Couldn't write to %s", filename);
    close (fd);
    for (d = hwdev; d; d = d->next)
	write_block_device (d);
}

void usage (char *s)
{
    fprintf (stderr,
            "SILO " VERSION " Sparc Improved boot LOader\n"
            "Usage: %s [options]\n"
            "Options:\n"
            " -r root_path   chroots into root_path (all paths relative to this)\n"
            " -b secondary   use secondary as second stage loader instead of /boot/second.b\n"
            " -i primary     install primary as first stage loader (instead of\n"
            "                /boot/first.b). If -i is specified, your boot block will be\n"
            "                always overwritten (by default only if it is not SILO or has\n"
            "                wrong version)\n"
            " -C config      specify alternate config file instead of /etc/silo.conf\n"
            "                (your config file has to reside on the same physical disk as\n"
            "                secondary - perhaps on different partitions - on that disk's\n"
            "                bootblock will be primary installed)\n"
            " -S backup      force saving your old bootblock into backup\n"
            " -s backup      save your old bootblock only if backup doesn't exist yet\n"
            " -p 0|2         force prom version to be 0 or 2 (default is auto-detection)\n"
            " -t             store bootblock on the same partition as second stage loader\n"
            "                (default is into the bootblock of the whole disk)\n"
            " -f             force overwriting of bootblock\n"
            " -V             show version\n"
            " -v             print your PROM major version (0 or 2) and exit\n"
            " -u             force v9 (ultrasparc) target, even on v8\n"
            " -U             force v8 target, even on v9\n"
            " -F             make a romfs bootable floppy\n"
            " -J image       make a romfs bootable flash image\n"
            "                -J and -F are incompatible\n"
	    " -a             Allow silo.conf syntax check to fail\n"
            ,s);
    exit (1);
}

int examine_bootblock (char *device, char *filename, int do_backup)
{
    char buffer[512 * 15];
    int fd, rc;
    FILE *fp;
    int ret = 0;

    if ((fd = devopen (device, O_RDONLY)) == -1)
	fatal ("Cannot open %s", device);
    if (lseek (fd, 512, 0) != 512)
        fatal ("Couldn't seek on %s", device);
    if (read (fd, buffer, sizeof (buffer)) != sizeof(buffer))
        fatal ("Couldn't read your old bootblock");
    close (fd);
    if (memcmp (buffer + 24, "SILO" IMGVERSION, 8))
        ret = 1;

    if (do_backup) {
    	if ((fp = fopen (filename, "w")) == NULL) {
    	    if (do_backup >= 2)
    	    	return ret;
	    fatal ("Cannot open file for saving backup of your bootblock");
	}
        if ((rc = fwrite (buffer, 1, sizeof (buffer), fp)) != sizeof (buffer))
            fatal ("Couldn't write to %s backup of your bootblock", filename);
        fclose (fp);
    }
    return ret;
}

void flash_check_sum (char *data, unsigned int dsize)
{
    unsigned int sum;

    sum = 0;
    while (dsize-- != 0) {
        sum += *((unsigned char *)data)++;
    }
    data[0] = sum >> 24;
    data[1] = sum >> 16;
    data[2] = sum >> 8;
    data[3] = sum;
}

void install_first_stage (char *device, char *filename)
{
    char buff[1024];
    int rc;
    int fd;
    FILE *fp;
    struct sun_disklabel sdl;

    if ((fd = devopen (device, floppy_image ? O_RDWR : O_WRONLY)) == -1)
	fatal ("Couldn't open device %s for writing", device);
    if ((fp = fopen (filename, "r")) == NULL)
	fatal ("Couldn't open file %s", filename);
    rc = fread (buff, 1, (floppy_image || flash_image) ? 1024 : 512, fp);
    if (rc <= 0)
	fatal ("Couldn't read new silo bootblock from %s", filename);
    if (floppy_image) {
        unsigned short *ush;
        unsigned short x;
        unsigned int s, d;
        int i;

	if (lseek (fd, 0, 0))
		fatal ("Couldn't seek on %s", device);
	if (read(fd, (char *)&sdl, 512) != 512)
		fatal ("Couldn't read on %s", device);
	if (lseek (fd, 0, 0))
		fatal ("Couldn't seek on %s", device);
	if (strncmp ((char *)&sdl, "-rom1fs-", 8))
		fatal ("Couldn't find romfs image on %s", device);
        memcpy (((char *)&sdl) + 128, floppy_label + 128, 512 - 128);
        ush = (unsigned short *)&sdl;

        /* This is a bit difficult, since we have to make two checksums right:
           partition table expects xor of all u16 in the first 512 bytes to be 0,
           romfs expects sum of all u32 in the first 512 bytes to be 0.
           Algorithm written by Gert-jan Los (los@lsdb.bwl.uni-mannheim.de). */
        sdl.csum = 0xdead; /* Could be zero... */
        ush[6] = 0; ush[7] = 0;
        memset (ush + 60, 0, 8);
	for (i = 0, d = 0; i < 256; i+=2)
        	d += *(unsigned int *)(ush + i);
	for (i = 0, x = 0; i < 256; i++)
        	x ^= ush[i];
        if (d & 1)
        	s = (x ^ 1) << 16 ^ 1;
        else
        	s = x << 16;
        *(unsigned int *)(ush + 6) = s;
        d += s;
        *(unsigned int *)(ush + 60) = (-d/2);
        *(unsigned int *)(ush + 62) = (-d/2);

        if (write (fd, &sdl, 512) != 512)
            fatal ("Couldn't write to %s your new partition table", device);
    } else if (flash_image) {
	/*
	 * Make sure that both block table address and checksum fit.
	 */
        if (rc > 1016)
            fatal ("Flash bootblock is too large");
        /*
         * Here we do an assumption which is not quite safe.
         * PROM gets the size looking at section headers,
         * we use the file size for that. If there is any padding,
         * these sizes will diverge. So far everything was ok.
         */
        flash_check_sum (buff, rc);
        rc += 4;
    } else if (lseek (fd, 512, 0) != 512)
        fatal ("Couldn't seek on %s", device);
    if (write (fd, buff, rc) != rc)
        fatal ("Couldn't write to %s your new silo bootblock", device);
    close (fd);
    fclose (fp);
}

int get_prom_ver(int use_prom)
{
#if defined(__linux__)
    FILE *f = fopen("/proc/cpuinfo","r");
    int ver = -1;
    char buffer[1024];
    char *p;

    if (f) {
    	while (fgets(buffer, 1024, f)) {
    	    if (!strncmp (buffer, "promlib", 7)) {
    	        p = strstr (buffer, "Version ");
    	        if (p) {
    	            p += 8;
    	            if (*p == '0' || (*p >= '2' && *p <= '3')) {
    	                ver = *p - '0';
    	            }
		}
    	    } else if (!strncmp (buffer, "prom", 4) &&
		       isspace(buffer[4])) {
		p = strchr (buffer, ':');
		if (p) {
		    p++;
		    while (isspace (*p)) p++;
		    switch (*p) {
		    case '2': if (p[1] == '.' && isdigit(p[2]) &&
				  (p[2] >= '6' || isdigit(p[3])))
				prombug = 2; break;
		    case '3': prombug = 0; break;
		    }
		}
	    }
    	}
    }
    return ver;
#elif defined(__solaris__)
    int ver = -1;
    
    if (use_prom)
        ver = prom_getversion ();
    return ver;
#endif
}

/* Canonicalize path, and return a new path. Do everything in situ.
   The new path differs from path in:
	Multiple `/'s are collapsed to a single `/'.
	Leading `./'s and trailing `/.'s are removed.
	Trailing `/'s are removed.
	Non-leading `../'s and trailing `..'s are handled by removing
	portions of the path. */
char *canonicalize_pathname (char *path)
{
    int i, start;
    char stub_char;

    stub_char = (*path == '/') ? '/' : '.';

    /* Walk along path looking for things to compact. */
    i = 0;
    for (;;) {
        if (!path[i])
	    break;

      	while (path[i] && path[i] != '/')
	    i++;

      	start = i++;

      	/* If we didn't find any slashes, then there is nothing left to do. */
      	if (!path[start])
	    break;

        /* Handle multiple `/'s in a row. */
        while (path[i] == '/')
	    i++;

        if ((start + 1) != i) {
	    strcpy (path + start + 1, path + i);
	    i = start + 1;
	}

        /* Handle backquoted `/'. */
        if (start > 0 && path[start - 1] == '\\')
	    continue;

        /* Check for trailing `/'. */
        if (start && !path[i]) {
	zero_last:
	    path[--i] = '\0';
	    break;
	}

        /* Check for `../', `./' or trailing `.' by itself. */
        if (path[i] == '.') {
	    /* Handle trailing `.' by itself. */
	    if (!path[i + 1])
	        goto zero_last;

	    /* Handle `./'. */
	    if (path[i + 1] == '/') {
	        strcpy (path + i, path + i + 1);
	        i = start;
	        continue;
	    }

	    /* Handle `../' or trailing `..' by itself. 
	       Remove the previous ?/ part with the exception of
	       ../, which we should leave intact. */
	    if (path[i + 1] == '.' && (path[i + 2] == '/' || !path[i + 2])) {
	        while (--start > -1 && path[start] != '/');
	        if (!strncmp (path + start + 1, "../", 3))
	            continue;
	        strcpy (path + start + 1, path + i + 2);
	        i = start;
	        continue;
	    }
	}
    }

    if (!*path) {
        *path = stub_char;
        path[1] = '\0';
    }
    return path;
}

char *find_dev(dev_t number)
{
#ifdef __linux__
#  define DEVNAME "/dev"
#else
#  define DEVNAME "/dev/dsk"
#endif
    DIR *dp;
    char *p;
    struct dirent *dir;
    static char name[PATH_MAX+2];
    struct stat s;
    char *ret = NULL;
    int i;

    if (!number) return NULL;
    for (i = 0; i < 2; i++) {
	p = DEVNAME;
	if (i == 1 && oldroot != -1) {
	    fchdir(oldroot);
	    p++;
	}
	if ((dp = opendir(p)) == NULL) continue;
	strcpy(name + 1,p);
	p = strchr (name + 1, 0);
	*p++ = '/';
	while ((dir = readdir(dp))) {
	    strcpy(p,dir->d_name);
	    if (stat(name + 1,&s) < 0) continue;
	    if (S_ISBLK(s.st_mode) && s.st_rdev == number) {
		ret = name + 1;
        	if (i == 1 && oldroot != -1) {
		    name[0] = '/';
		    ret--;
        	}
        	break;
	    }
        }
        if (ret) break;
    }
    if (i && oldroot != -1)
    	chdir("/");
    return ret;
}

struct hwdevice *get_device(int majno, int minno)
{
    char dev[1024], wholedev[1024];
    struct hwdevice *hwdev;
    int unit, part = -1;
    enum typeenum type = TYPE_UNKNOWN;
    switch (majno) {
#ifdef __linux__
	case 8:
	case 65:
	case 66:
	case 67:
	case 68:
	case 69:
	case 70:
	case 71:
		unit = (majno & 7) * 16 + ((minno & 0xf0) >> 4);
		part = minno & 0xf;
		if (unit < 26) {
			sprintf (dev, "/dev/sd%c%c", unit + 'a', part + '0');
			strcpy (wholedev, dev);
			wholedev [8] = 0;
		} else {
			sprintf (dev, "/dev/sd%c%c%c", (unit / 26) + 'a' - 1, (unit % 26) + 'a', part + '0');
			strcpy (wholedev, dev);
			wholedev [9] = 0;
		}
		part--;
		type = TYPE_SCSI;
		break;
	case 3: type = TYPE_IDE; unit = 0; goto do_ide;
	case 22:type = TYPE_IDE; unit = 2; goto do_ide;
	case 33:
	case 34:type = TYPE_IDE; unit = 2*(majno - 33) + 4;
	do_ide:	
		unit += ((minno & 64) >> 6);
		part = (minno & 0x3f) - 1;
		sprintf (dev, "/dev/hd%c%d", unit + 'a', part + 1);
		strcpy (wholedev, dev);
		wholedev [8] = 0;
		break;
	case 7:		/* Loop device */
		unit = minno;
		sprintf (dev, "/dev/loop%d", unit);
		dev [1023] = 0;
		strcpy (wholedev, dev);
		break;
	case 9:		/* RAID device */
		{
			md_array_info_t md_array_info;
			md_disk_info_t md_disk_info;
			int md_fd, i, id = 0;
			struct hwdevice *d, *last;

			sprintf (dev, "/dev/md%d", minno);
			md_fd = devopen (dev, O_RDONLY);
			if (md_fd < 0) fatal ("Could not open RAID device");
			if (ioctl (md_fd, GET_ARRAY_INFO, &md_array_info) < 0)
				fatal ("Could not get RAID array info");
			if (md_array_info.major_version == 0 && md_array_info.minor_version < 90)
				fatal ("Raid versions < 0.90 are not supported");
			if (md_array_info.level != 1)
				fatal ("Only RAID1 supported");
			hwdev = NULL;
			last = NULL;
			for (i = 0; i < md_array_info.nr_disks; i++) {
				md_disk_info.number = i;
				if (ioctl (md_fd, GET_DISK_INFO, &md_disk_info) < 0)
					fatal ("Could not get RAID disk info for disk %d\n", i);
				d = get_device (md_disk_info.majorno, md_disk_info.minorno);
				if (md_disk_info.state == MD_DISK_FAULTY) {
					printf ("disk %s marked as faulty, skipping\n", d->dev);
					continue;
				}
				if (hwdev)
					last->next = d;
				else
					hwdev = d;
				while (d->next != NULL) d = d->next;
				last = d;
			}
			if (!hwdev)
				fatal ("No non-faulty disks found in RAID1");
			for (d = hwdev; d; d = d->next)
				d->id = id++;
			raid1 = id;
			close (md_fd);
			return hwdev;
		}
#endif
	default: {
		char *p = find_dev (makedev (majno, minno));

		if (!p) fatal ("Couldn't find out what device is second stage on");
		strcpy (dev, p);
		strcpy (wholedev, p);
#ifdef __solaris__
		if (strlen (p) == strlen ("/dev/dsk/c0t0d0s0")) {
		    p = strchr (p, 0) - 2;
		    if (*p == 's' && p[1] >= '0' && p[1] <= '7') {
	        	p = strchr (wholedev, 0) - 1;
		        *p = '2';
		        type = TYPE_SCSI;
		        part = p[1] - '0';
		    }
		}
#endif
		}
		break;
    }
    hwdev = malloc(sizeof(struct hwdevice) + strlen(dev) + strlen(wholedev) + 4);
    if (!hwdev) fatal("Not enough memory");
    memset(hwdev, 0, sizeof(*hwdev));
    hwdev->dev = (char *)(hwdev + 1);
    strcpy (hwdev->dev, dev);
    hwdev->wholedev = strchr(hwdev->dev, 0) + 2;
    strcpy (hwdev->wholedev, wholedev);
    hwdev->part = part;
    hwdev->partat0 = -1;
    hwdev->type = type;
    return hwdev;
}

int main(int argc,char **argv)
{
    char *name = NULL, *config_file, *install = NULL, *secondary, *backup, *p;
    struct utsname uts;
    int version = 0;
    struct stat st1, st2, st3;
    int force_backup = 0;
    int config_file_partno = 1;
    int force = 0, f;
    struct hwdevice *hwdev;
    
    int use_prom = 0;
    int print_prom_version = 0;

#ifdef __solaris__
    if (prom_init () >= 0)
        use_prom = 1;
#endif
    if (uname (&uts) >= 0 && !strcmp (uts.machine, "sparc64"))
    	ultra = 1;
    config_file = DFL_CONFIG;
    backup = DFL_BACKUP;
    secondary = DFL_SECONDARY;
    new_root = NULL;
    name = *argv++;
    argc--;
    while (argc && **argv == '-') {
	argc--;
	if (argv[0][2] && argv[0][1] != 'p') usage(name);
	switch ((*argv++)[1]) {
	    case 'b':
	        if (!argc) usage(name);
	        secondary = *argv++;
	        argc--;
	        break;
	    case 'i':
		if (!argc) usage(name);
		install = *argv++;
		argc--;
		break;
	    case 'F':
	        floppy_image = 1;
	        break;
	    case 'p':
	        if ((argv[-1])[2] == '0')
	            promver = 0;
	        else if ((argv[-1])[2] == '2')
	            promver = 2;
	        else if (argv[-1][2])
	            usage(name);
	        else if (argc && **argv == '0') {
	            promver = 0;
	            argv++;
	            argc--;
	        } else if (argc && **argv == '2') {
	            promver = 2;
	            argv++;
	            argc--;
	        } else usage(name);
	        break;
	    case 'f':
	    	force = 1;
	    	break;
	    case 'C':
		if (!argc) usage(name);
		config_file = *argv++;
		argc--;
		break;
	    case 'S':
		if (!argc) usage(name);
		backup = *argv++;
		force_backup = 1;
		argc--;
		break;
	    case 's':
		if (!argc) usage(name);
		backup = *argv++;
		argc--;
		break;
	    case 'r':
		if (!argc) usage(name);
		new_root = *argv++;
		argc--;
		break;
	    case 'V':
		version = 1;
		break;
	    case 't':
	    	masterboot = 0;
	    	break;
	    case 'v':
	    	print_prom_version = 1;
	    	break;
	    case 'u':
	        ultra = 1;
	        break;
	    case 'U':
	        ultra = 0;
	        break;
	    case 'J':
		if (!argc) usage(name);
		flash_image = *argv++;
		argc--;
		break;
	    case 'a':
		allow_confchk_fail = 1;
		break;
	    default:
		usage(name);
	}
    }
    if (promver == -1 && !ultra)
	promver = get_prom_ver(use_prom);
    else if (!ultra)
	prombug = 1;
    else
	prombug = 0;
    if (print_prom_version) {
        if (ultra)
        	printf("IEEE 1275\n");
        else
        	printf ("%d\n", promver);
        return 0;
    }
    if (!new_root) new_root = getenv("ROOT");
    if (version) {
	fprintf(stderr, "SILO version " VERSION "\n");
	return 0;
    }
    if (argc) usage(name);
    if (floppy_image && flash_image) usage(name);

    if (flash_image) {
	char *p, *q;
	if (*flash_image == '/') {
	    p = malloc(strlen(flash_image) + 2);
	    strcpy(p, flash_image);
	} else {
#ifdef __linux__
	    q = getcwd(NULL, 0);
	    p = malloc(strlen(flash_image) + 3 + strlen(q));
	    strcpy(p, q);
	    free(q);
#else
	    p = malloc(PATH_MAX + 3 + strlen(flash_image));
	    getcwd(p, PATH_MAX);
#endif
	    strcat(p, "/");
	    strcat(p, flash_image);
	}
	flash_image = p;
    }

    if (new_root && strcmp("/", new_root)) {
       if (stat (new_root, &st1) < 0 || !S_ISDIR(st1.st_mode)) {
           fatal ("New root %s is not a directory", new_root);
       }
       oldroot = open("/", O_RDONLY);
       chroot(new_root);
       chdir("/");
    }

    /* This _must_ be done after chrooting */
    if (!confcheck(config_file) && !allow_confchk_fail)
	return 1;

    secondary = strdup (secondary);
    if (stat (secondary, &st1) < 0)
        fatal ("Cannot open second stage loader %s", secondary);
    hwdevs = get_device (mmajor(st1.st_dev), mminor(st1.st_dev));
    if (raid1 > 32)
	fatal ("SILO supports at most 32 disks in the RAID1 array");
    if (raid1 && masterboot) {
	struct hwdevice *d, *d1;
	/* Check if we have to remove some RAID1 mirrors, because
	   we can support only one RAID1 mirror per physical device
	   if masterboot.  */
	for (d = hwdevs; d; d = d->next)
	    for (d1 = d; d1->next; d1 = d1->next)
		if (!strcmp (d->wholedev, d1->next->wholedev)) {
		    d1->next = d1->next->next;
		    raid1--;
		}
    }
    if (flash_image) {
        /*
         * The filesystem starts at offset 1024 into the flash. This
         * makes it impossible to patch the first stage through /dev/loopN.
         * We use argument to -J as a way to access the whole flash image.
         *
         * XXX There is no good way to ensure that argument to -J
         * is what we have mounted off a loopback or a ramdisk.
         * So we do not bother trying.
         */
        if (raid1)
	    fatal ("-J cannot be used with RAID1");
	hwdevs->wholedev = flash_image;
    }
    p = backup;
    backup = malloc (strlen(backup) + 20);
    config_file = strdup (config_file);
    strcpy (backup, p);
    if (!backup || !config_file)
	fatal ("Not enough memory");
    if (stat (config_file, &st2) >= 0) {
	if (raid1 && st1.st_dev != st2.st_dev)
	    fatal ("Config file %s has to be on the same RAID1 device as second stage bootblock", config_file);
	else if (hwdevs->type == TYPE_UNKNOWN && st1.st_dev != st2.st_dev)
	    fatal ("Config file %s has to be on the %s device", config_file, hwdevs->dev);
#ifdef __linux__
        else if ((hwdevs->type == TYPE_SCSI && (mmajor(st2.st_dev) != mmajor(st1.st_dev) || (mminor(st2.st_dev) & (~0xf)) != (mminor(st1.st_dev) & (~0xf)))) ||
            (hwdevs->type == TYPE_IDE && (mmajor(st2.st_dev) != mmajor(st1.st_dev) || (mminor(st2.st_dev) & (~0x3f)) != (mminor(st1.st_dev) & (~0x3f)))))
#elif defined(__solaris__)
	else if (hwdevs->type == TYPE_SCSI && (st2.st_dev & (~0x7)) != (st1.st_dev & (~0x7)))
#else
#  error "Unknown system"
#endif
            fatal ("Config file %s has to be on the %s device (on any partition there)", config_file, hwdevs->wholedev);
        else {
            char *p, *q, *r, c;
     	    char readlinkbuf[2048];
     	    int len;
     	    char buffer[2048], buffer2[2048];

     	    strcpy (buffer, config_file);
	    for (p = buffer;;) {
	        q = strchr (p, '/');
	        if (q) {
	            c = *q;
	            *q = 0;
	        } else c = 0;
	        if (lstat (*buffer ? buffer : "/", &st3) < 0)
	            fatal ("Couldn't stat %s\n", config_file);
	        if (st3.st_dev == st2.st_dev) {
	            *q = c;
	            config_file = q;
	            break;
	        }
	        if (S_ISLNK(st3.st_mode)) {
	            len = readlink (buffer, readlinkbuf, 2048);
	            if (len < 0)
	                fatal ("Couldn't readlink %s\n", config_file);
	            readlinkbuf[len] = 0;
	            if (*readlinkbuf == '/') {
	                if (c) {
	                    r = readlinkbuf+len;
	                    if (readlinkbuf[len - 1] != '/')
	                        *r++ = '/';
	                    strcpy (r, q + 1);
	                }
	                strcpy (buffer, readlinkbuf);
	        	p = buffer + 1;
	        	continue;
	            } else {
			r = strrchr (buffer, '/');
			memcpy (buffer2, buffer, r - buffer + 1);
	            	strcpy (buffer2 + (r - buffer + 1), readlinkbuf);
	            	if (c) {
	            	    strcat (buffer2, "/");
	            	    strcat (buffer2, q + 1);
	            	}
	            	strcpy (buffer, buffer2);
	            	p = buffer + 1;
	            	continue;
	            }
	        } else {
	            *q = c;
	            p = q + 1;
	            if (!c)
	            	fatal ("Internal error\n");
	        }
	    }     	               
        }
        if (raid1)
	    config_file_partno = -1;
	else if (hwdevs->type == TYPE_SCSI)
#ifdef __linux__	
            config_file_partno = (mminor(st2.st_dev) & 0x0f);
#elif defined(__solaris__)
	    config_file_partno = (st2.st_dev & 7) + 1;
#else
#  error "Unknown system"
#endif
#ifdef __linux__	
	else if (hwdevs->type == TYPE_IDE)
            config_file_partno = (mminor(st2.st_dev) & 0x3f);
#endif
	else
	    config_file_partno = 1;
    }
    for (hwdev = hwdevs; hwdev; hwdev = hwdev->next)
	read_sb (hwdev);
    for (hwdev = hwdevs, f = 0; hwdev; hwdev = hwdev->next) {
	int fb = force_backup;

	p = NULL;
	if (raid1) {
	    p = strchr (backup, 0);
	    sprintf (p, "-raid1-%d", hwdev->id);
	}
	if (!force_backup) {
	    if (stat (backup, &st2) < 0)
		fb = 2;
	}
	f |= examine_bootblock (masterboot ? hwdev->wholedev : hwdev->dev, backup, fb);
	if (p)
	    *p = 0;
    }
    if (f || install || force) {
        if (!install)
	    install = strdup (ultra ? DFL_PRIMARY_U : DFL_PRIMARY);
        else if (*install == '/')
            install = strdup (install);
	for (hwdev = hwdevs; hwdev; hwdev = hwdev->next)
	    install_first_stage (masterboot ? hwdev->wholedev : hwdev->dev, install);
    }
    make_room_for_raid1(hwdevs, secondary);
    for (hwdev = hwdevs; hwdev; hwdev = hwdev->next)
	get_partition_blocks (hwdev, secondary);
    write_block_tables (hwdevs, secondary, config_file, config_file_partno);
    sync();
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
    std_fd = devopen (name, O_RDONLY);
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
    struct hwdevice *hwdev = (struct hwdevice *)private;
    for (j = 0; j < hwdev->nsect; j++)
        blocks [ufs_block_idx++] = *block * hwdev->nsect + hwdev->doff + j;
    return 0;
}

static int ufs_blocks (struct hwdevice *hwdev, ino_t inode)
{
    if (ufs_open (hwdev->dev, std_io_manager, &fs))
    	fatal ("Cannot open ufs filesystem containing second stage");
    hwdev->nsect = cbs / 512;
    if (ufs_block_iterate (fs, inode, ufs_blocks_dump, hwdev))
        fatal ("Block iterating error on second stage");
    blocks [ufs_block_idx] = 0;
    return 0;
}

#endif
