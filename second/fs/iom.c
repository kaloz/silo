/* SILO I/O Manager for filesystem operations.
   
   Copyright (C) 1996 Maurizio Plaza
   		 1996,1997,1999 Jakub Jelinek
		 2001 Ben Collins
   
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA.  */

#include <sys/types.h>
#include <silo.h>
#include <file.h>
#include <stringops.h>

static unsigned long long doff;		/* Block where partition starts */

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
    doff = (((unsigned long long)sdl.ntrks) * sdl.nsect * sdl.partitions[partno - 1].start_cylinder) << 9;    return 1;
}

static errcode_t silo_open (const char *name, int flags, io_channel * channel)
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
    io->manager = silo_io_manager;
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

static errcode_t silo_close (io_channel channel)
{
    return 0;
}

static errcode_t silo_set_blksize (io_channel channel, int blksize)
{
    channel->block_size = bs = blksize;
    return 0;
}

static errcode_t silo_read_blk (io_channel channel, unsigned long block, int count, void *data)
{
    int size;

    size = (count < 0) ? -count : count * bs;
    if (read (data, size, ((unsigned long long)block) * bs + doff) != size) {
	printf ("\nRead error on block %d\n", block);
	return EXT2_ET_SHORT_READ;
    }
    return 0;
}

static errcode_t silo_write_blk (io_channel channel, unsigned long block, int count, const void *data)
{
    return 0;
}

static errcode_t silo_flush (io_channel channel)
{
    return 0;
}

/* The actual I/O Manager.  */
static struct struct_io_manager struct_silo_manager =
{
    EXT2_ET_MAGIC_IO_MANAGER,
    "SILO I/O Manager",
    silo_open,
    silo_close,
    silo_set_blksize,
    silo_read_blk,
    silo_write_blk,
    silo_flush
};

io_manager silo_io_manager = &struct_silo_manager;
