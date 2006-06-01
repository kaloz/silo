/* EXT2 Interface for SILO filesystem access routines
   
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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
   USA.  */

#include <sys/types.h>
#include <silo.h>
#include <file.h>
#include <stringops.h>

static ino_t inode = 0;
struct fs_ops ext2_fs_ops;

void com_err (const char *a, long i, const char *fmt,...)
{
    printf ((char *) fmt);
}

static void ext2fs_error (int errcode)
{
#if 0
    int i;
    for (i = 0; i < sizeof (ext2errors) / sizeof (ext2errors[0]); i++)
    	if (ext2errors[i].errnum == errcode) {
    	    printf ("%s", ext2errors [i].desc);
    	    return;
    	}
#endif
    printf ("Unknown ext2 error");
}

static int open_ext2 (char *device)
{
    int retval;

    retval = ext2fs_open (device, EXT2_FLAG_RW, 0, 0, silo_io_manager, &fs);
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

static int dump_block_ext2 (ext2_filsys fs, blk_t *blocknr,
			    int blockcnt, void *private)
{
    return dump_block(blocknr, blockcnt);
}

static int dump_ext2 (void)
{
    if (ext2fs_block_iterate (fs, inode, 0, 0, dump_block_ext2, 0))
	return 0;

    return dump_finish ();
}

static int ls_ext2_proc(struct ext2_dir_entry *dirent, int offset,
			int blocksize, char *buf, void *private)
{
    struct ext2_inode ino;
    int sl = 0, name_len = dirent->name_len & 0xFF;
    char name[256], symlink[256];

    strncpy(name, dirent->name, name_len);
    name[name_len] = 0;

    if (ext2fs_read_inode(fs, dirent->inode, &ino))
	strcpy (name, "--- error ---");

    if (LINUX_S_ISLNK (ino.i_mode)) {
	sl = 1;
	if (ext2fs_inode_data_blocks(fs, ino)) {
	    if (io_channel_read_blk(fs->io, ino.i_block[0], 1, symlink))
		ino.i_size = 0;
	} else {
	    strncpy (symlink, (char *)&(ino.i_block[0]),ino.i_size);
	}
	symlink[ino.i_size] = 0;
    }

    register_silo_inode(ino.i_mtime, ino.i_size, ino.i_mode, ino.i_uid,
			ino.i_gid, name, sl ? symlink : NULL);

    return 0;
}

static int ls_ext2 (void)
{
    return ext2fs_dir_iterate (fs, inode, DIRENT_FLAG_INCLUDE_EMPTY,
				 0, ls_ext2_proc, NULL);
}

static int ino_size_ext2 (void) {
    struct ext2_inode ei;
    int retval;

    if ((retval = ext2fs_read_inode (fs, inode, &ei))) {
	printf ("\n");
	ext2fs_error (retval);
	printf ("\n");
	return 0;
    }
    return ei.i_size;
}

static int namei_follow_ext2 (const char *filename) {
    int ret = ext2fs_namei_follow (fs, root, root, filename, &inode);

    ext2_fs_ops.have_inode = (inode) ? 1 : 0;

    return ret;
}

static void print_error_ext2 (int error_val) {
    ext2fs_error (error_val);
}

void close_ext2 (void) {
    ext2fs_close(fs);
}

struct fs_ops ext2_fs_ops = {
    .name		= "Linux EXT2",
    .open		= open_ext2,
    .ls			= ls_ext2,
    .dump		= dump_ext2,
    .close		= close_ext2,
    .ino_size		= ino_size_ext2,
    .print_error	= print_error_ext2,
    .namei_follow	= namei_follow_ext2,
    .have_inode		= 0,
};

/* These are silly stubs to satisfy libext2fs symbols */
unsigned long time(void)
{
        return 0;
}

void *realloc(void *p, int size)
{
        return NULL;
}
