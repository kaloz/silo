/* Exported filesystem related stuff

   Copyright (C) 2001 Ben Collins

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

typedef int FILE;
#include <linux/types.h>
#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>

extern unsigned int bs;			/* Block Size */
extern io_manager silo_io_manager;	/* The Filesystem I/O Manager */
extern void *filebuffer;
extern ext2_filsys fs;			/* Generic filesystem */
extern ino_t root, cwd;			/* root and cwd for current fs */
extern int solaris;

extern int dump_block (blk_t *, int);
extern int dump_finish (void);
extern void register_silo_inode (unsigned int, unsigned int,
				 unsigned int, unsigned int,
				 unsigned int, const char *,
				 const char *);

/* Filesystem operations provided by each module */
struct fs_ops {
    char *name;
    int (*open)		(char *);
    int (*ls)		(void);
    int (*dump)		(void);
    int (*ino_size)	(void);
    int (*namei_follow)	(const char *);
    void (*print_error)	(int);
    void (*close)	(void);
    int have_inode;
};
