#include <ext2fs/ext2fs.h>

#ifdef __linux__

#include <features.h>
#ifdef __GLIBC__
#  define _LINUX_TIME_H
#endif
#include <linux/version.h>
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,47)
#include <linux/romfs_fs.h>
#else
#include <non-linux/romfs_fs.h>
#endif

#else

#include <non-linux/romfs_fs.h>

#endif

typedef ext2_filsys romfs_filsys;

int romfs_open(char *device, io_manager io, romfs_filsys *fs);
int romfs_namei(romfs_filsys fs, ino_t root, ino_t cwd, char *filename, ino_t *inode);
void romfs_close(romfs_filsys fs);
int romfs_read_inode(romfs_filsys fs, ino_t inode, struct romfs_inode *ui);
int romfs_block_iterate(romfs_filsys, ino_t,
		      int (*)(romfs_filsys, blk_t *, int, void *), void *);
