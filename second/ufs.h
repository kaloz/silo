#ifdef __linux__

#include <features.h>
#ifdef __GLIBC__
#  define _LINUX_TIME_H
#endif
#include <linux/ufs_fs.h>
#else

#include <non-linux/ufs_fs.h>

#endif

typedef ext2_filsys ufs_filsys;

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

int ufs_open(char *device, io_manager io, ufs_filsys *fs);
int ufs_namei(ufs_filsys fs, ino_t root, ino_t cwd, char *filename, ino_t *inode);
void ufs_close(ufs_filsys fs);
int ufs_read_inode(ufs_filsys fs, ino_t inode, struct ufs_inode *ui);
int ufs_block_iterate(ufs_filsys, ino_t,
		      int (*)(ufs_filsys, blk_t *, int, void *), void *);
