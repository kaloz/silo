/* EXT2 Interface for SILO filesystem access routines
   
   Copyright (C) 1996 Maurizio Plaza
   		 1996,1997,1999 Jakub Jelinek
		 2001 Ben Collins
		 2012 David S. Miller
   
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

#define EXT2_SUPER_MAGIC	0xEF53

static __u32 ext2_to_cpu_32(__u32 v)
{
	const __u8 *p = (const __u8 *) &v;

	return ((__u32)p[3] << 24 |
		(__u32)p[2] << 16 |
		(__u32)p[1] <<  8 |
		(__u32)p[0]);
}

static __u16 ext2_to_cpu_16(__u16 v)
{
	const __u8 *p = (const __u8 *) &v;

	return ((__u16)p[1] << 8) | ((__u16)p[0]);
}

struct silo_ext2_state {
	struct ext2_super_block	*super;

	void			*scratch_block[4];
	__u32			scratch_block_blk[4];

	__u32			inode_size;
	__u32			block_size;
	__u32			addr_per_block;
	__u32			addr_per_block_bits;
	__u32			desc_per_block;
	__u32			desc_per_block_bits;
};
static struct silo_ext2_state *sstate;

static __u32 ext2_inode_size(struct silo_ext2_state *s)
{
	__u32 rev = ext2_to_cpu_32(s->super->s_rev_level);

	if (rev == 0)
		return 128;

	return ext2_to_cpu_16(s->super->s_inode_size);
}

static __u32 ext2_block_size(struct silo_ext2_state *s)
{
	__u32 x = ext2_to_cpu_32(s->super->s_log_block_size);

	x += 1;

	return ((__u32) 1 << x) * 512;
}

static void read_scratch_block(struct silo_ext2_state *s, __u32 block, int N)
{
	if (s->scratch_block_blk[N] == block)
		return;
	io_channel_read_blk(fs->io, block, 1, s->scratch_block[N]);
	s->scratch_block_blk[N] = block;
}

static void read_data(struct silo_ext2_state *s, __u32 block, __u32 off, __u32 len, void *buf)
{
	read_scratch_block(s, block, 0);
	memcpy(buf, s->scratch_block[0] + off, len);
}

static int has_extents(struct ext2_inode *ip)
{
	if (ext2_to_cpu_32(ip->i_flags) & 0x80000)
		return 1;
	return 0;
}

#define EXT4_EXT_MAGIC		0xf30a

struct ext4_extent_header {
	__u16	eh_magic;
	__u16	eh_entries;
	__u16	eh_max;
	__u16	eh_depth;
	__u32	eh_generation;
};

struct ext4_extent_idx {
	__u32	ei_block;
	__u32	ei_leaf_lo;
	__u16	ei_leaf_hi;
	__u16	ei_unused;
};

struct ext4_extent {
	__u32	ee_block;
	__u16	ee_len;
	__u16	ee_start_hi;
	__u32	ee_start_lo;
};

static struct ext4_extent_header *search_leaf(struct silo_ext2_state *s,
					      struct ext2_inode *ip,
					      unsigned long long file_block)
{
	struct ext4_extent_header *ehp;

	ehp = (struct ext4_extent_header *) &ip->i_block[0];
	for (;;) {
		unsigned long long ext_block, hi, lo;
		struct ext4_extent_idx *idxp;
		int i;

		idxp = (struct ext4_extent_idx *) (ehp + 1);

		if (ext2_to_cpu_16(ehp->eh_magic) != EXT4_EXT_MAGIC)
			return (struct ext4_extent_header *) 0;

		if (ehp->eh_depth == ext2_to_cpu_16(0))
			return ehp;

		for (i = 0; i < ext2_to_cpu_16(ehp->eh_entries); i++, idxp++)
			if (ext2_to_cpu_32(idxp->ei_block) > file_block)
				break;

		if (i == 0)
			return (struct ext4_extent_header *) 0;

		idxp -= 1;

		hi = ((unsigned long long)ext2_to_cpu_16(idxp->ei_leaf_hi)) << 32;
		lo = ext2_to_cpu_32(idxp->ei_leaf_lo);
		ext_block = hi | lo;

		read_scratch_block(s, ext_block, 0);
		ehp = (struct ext4_extent_header *) s->scratch_block[0];
	}
}

#define BLOCK_MAP_ERROR (~0ULL)

static int block_to_path(struct silo_ext2_state *s, struct ext2_inode *ip,
			 int offsets[4], long file_block)
{
	int ptrs_bits = s->addr_per_block_bits;
	int ptrs = s->addr_per_block;
	const long direct_blocks = EXT2_NDIR_BLOCKS,
		indirect_blocks = ptrs,
		double_blocks = (1 << (ptrs_bits * 2));
	int n = 0;

	if (file_block < 0) {
		printf("EXT2: Illegal file block %ld\n", file_block);
	} else if (file_block < direct_blocks) {
		offsets[n++] = file_block;
	} else if ((file_block -= direct_blocks) < indirect_blocks) {
		offsets[n++] = EXT2_IND_BLOCK;
		offsets[n++] = file_block;
	} else if ((file_block -= indirect_blocks) < double_blocks) {
		offsets[n++] = EXT2_DIND_BLOCK;
		offsets[n++] = file_block >> ptrs_bits;
		offsets[n++] = file_block & (ptrs - 1);
	} else if (((file_block -= double_blocks) >> (ptrs_bits * 2)) < ptrs) {
		offsets[n++] = EXT2_TIND_BLOCK;
		offsets[n++] = file_block >> (ptrs_bits * 2);
		offsets[n++] = (file_block >> ptrs_bits) & (ptrs - 1);
		offsets[n++] = file_block & (ptrs - 1);
	} else {
		printf("EXT2: File block %ld is too big\n", file_block);
	}

	return n;
}

static unsigned long long resolve_path(struct silo_ext2_state *s,
				       struct ext2_inode *ip,
				       int *offsets, int depth)
{
	__u32 *p = ip->i_block + *offsets;

	while (--depth) {
		__u32 block = ext2_to_cpu_32(*p);

		read_scratch_block(s, block, depth);
		p = (__u32 *) s->scratch_block[depth] + *++offsets;
	}

	return ext2_to_cpu_32(*p);
}

static unsigned long long resolve_extent(struct silo_ext2_state *s,
					 struct ext4_extent_header *ehp,
					 unsigned long long file_block)
{
	unsigned long long hi, lo;
	struct ext4_extent *ep;
	int i;

	ep = (struct ext4_extent *) (ehp + 1);

	for (i = 0; i < ext2_to_cpu_16(ehp->eh_entries); i++, ep++)
		if (ext2_to_cpu_32(ep->ee_block) > file_block)
			break;

	if (i == 0)
		return BLOCK_MAP_ERROR;

	file_block -= ext2_to_cpu_32(ep->ee_block);
	if (ext2_to_cpu_16(ep->ee_len) < file_block)
		return BLOCK_MAP_ERROR;

	hi = ((unsigned long long)ext2_to_cpu_16(ep->ee_start_hi)) << 32;
	lo = ext2_to_cpu_32(ep->ee_start_lo);

	return (hi | lo) + file_block;
}

static unsigned long long file_to_disk_block(struct silo_ext2_state *s,
					     struct ext2_inode *ip,
					     unsigned long long file_block)
{
	if (has_extents(ip)) {
		struct ext4_extent_header *ehp = search_leaf(s, ip, file_block);

		if (ehp)
			return resolve_extent(s, ehp, file_block);

		printf("EXT2: Extent leaf search for block %d failed\n",
		       (int) file_block);

		return BLOCK_MAP_ERROR;
	} else {
		int depth, offsets[4];

		depth = block_to_path(s, ip, offsets, file_block);
		if (depth)
			return resolve_path(s, ip, offsets, depth);

		printf("EXT2: block --> path on block %d failed\n",
		       (int) file_block);

		return BLOCK_MAP_ERROR;
	}
}

static void read_file(struct silo_ext2_state *s, struct ext2_inode *ip,
		      __u32 off, __u32 len, void *buf)
{
	unsigned long long disk_block;

	disk_block = file_to_disk_block(s, ip, off / s->block_size);

	read_scratch_block(s, disk_block, 0);
	memcpy(buf, s->scratch_block[0] + (off % s->block_size), len);
}

static void read_group(struct silo_ext2_state *s, __u32 grp_no,
		       struct ext2_group_desc *grp)
{
	__u32 first = ext2_to_cpu_32(s->super->s_first_data_block);
	__u32 blk, offset;

	blk = first + 1 + (grp_no >> s->desc_per_block_bits);
	offset = (grp_no & (s->desc_per_block - 1)) * sizeof(*grp);

	read_data(s, blk, offset, sizeof(*grp), grp);
}

static void read_inode(struct silo_ext2_state *s, __u32 ino, struct ext2_inode *ip)
{
	__u32 grp_no, blk_no, inode_in_block, ipg, ipb;
	struct ext2_super_block *sb = s->super;
	struct ext2_group_desc gd;

	ipg = ext2_to_cpu_32(sb->s_inodes_per_group);

	grp_no = (ino - 1) / ipg;
	read_group(s, grp_no, &gd);

	blk_no = (ino - 1) % ipg;
	inode_in_block = blk_no;

	ipb = s->block_size / s->inode_size;
	blk_no /= ipb;
	inode_in_block %= ipb;

	read_data(s,
		  ext2_to_cpu_32(gd.bg_inode_table) + blk_no,
		  inode_in_block * s->inode_size,
		  sizeof(struct ext2_inode), ip);
}

static int calc_ilog2(__u32 n)
{
	int i = 0;

	while ((n >>= 1) != 0)
		i++;

	return i;
}

static int open_ext2(char *device)
{
	struct silo_ext2_state *s = malloc(sizeof(*s));
	struct ext2_super_block *sb;
	int err, i;

	if (!s) {
		printf("Cannot allocate silo_ext2_state\n");
		return 0;
	}
	memset(s, 0, sizeof(*s));

	fs = malloc(sizeof(*fs));
	if (!fs) {
		printf("Cannot allocate ext2_filsys\n");
		goto out_free_s;
	}
	memset(fs, 0, sizeof(*fs));
	err = silo_io_manager->open(device, 0, &fs->io);
	if (err) {
		printf("I/O manager open failed (err=%d)\n", err);
		goto out_free_fs;
	}
	fs->io->app_data = fs;

	sb = s->super = malloc(1024);
	if (!sb) {
		printf("Cannot allocate ext2 super block\n");
		goto out_free_fs;
	}

	io_channel_set_blksize(fs->io, 1024);
	err = io_channel_read_blk(fs->io, 1, -1024, sb);

	if (ext2_to_cpu_16(sb->s_magic) != EXT2_SUPER_MAGIC) {
		printf("EXT2 superblock magic is wrong\n");
		goto out_free_super;
	}

	s->inode_size = ext2_inode_size(s);
	s->block_size = ext2_block_size(s);

	io_channel_set_blksize(fs->io, s->block_size);

	s->addr_per_block = s->block_size / sizeof(__u32);
	s->addr_per_block_bits = calc_ilog2(s->addr_per_block);
	s->desc_per_block = s->block_size / sizeof(struct ext2_group_desc);
	s->desc_per_block_bits = calc_ilog2(s->desc_per_block);

	s->scratch_block[0] = malloc(s->block_size * 4);
	if (!s->scratch_block[0]) {
		printf("Cannot allocate ext2 scratch blocks\n");
		goto out_free_super;
	}
	for (i = 1; i < 4; i++)
		s->scratch_block[i] =
			s->scratch_block[i - 1] + s->block_size;
	for (i = 0; i < 4; i++)
		s->scratch_block_blk[i] = ~(__u32)0;

	root = EXT2_ROOT_INO;

	sstate = s;

	return 1;

out_free_super:
	free(s->super);

out_free_fs:
	free(fs);
	fs = NULL;

out_free_s:
	free(s);
	return 0;
}

void close_ext2 (void)
{
	struct silo_ext2_state *s = sstate;

	if (s) {
		free(s->scratch_block[0]);
		free(s->super);
		free(s);

		sstate = (struct silo_ext2_state *) 0;
	}
}

static int dump_ext2 (void)
{
	struct silo_ext2_state *s = sstate;
	struct ext2_inode ei;
	long file_offset;
	int sz, blk_sz;
	int blk_cnt;

	read_inode(s, inode, &ei);

	sz = ext2_to_cpu_32(ei.i_size);
	blk_sz = s->block_size;

	blk_cnt = 0;
	for (file_offset = 0; file_offset < sz; file_offset += blk_sz) {
		blk_t disk_block;

		disk_block = file_to_disk_block(s, &ei,
						file_offset / blk_sz);
		if (disk_block == BLOCK_MAP_ERROR)
			return 0;

		if (disk_block)
			dump_block(&disk_block, blk_cnt);

		blk_cnt++;
	}

	return dump_finish ();
}

static char *read_symlink(struct silo_ext2_state *s, struct ext2_inode *ip, char *namebuf)
{
	__u32 isize = ext2_to_cpu_32(ip->i_size);

	if (isize <= 60)
		return (char *) &ip->i_block[0];

	read_data(s, ext2_to_cpu_32(ip->i_block[0]), 0, isize, namebuf);

	return namebuf;
}

static int is_symlink(__u16 mode)
{
	if ((mode & 0xf000) == 0xa000)
		return 1;
	return 0;
}

typedef int (*dir_callback_t)(struct silo_ext2_state *, struct ext2_dir_entry_2 *, void *);

static int ls_callback(struct silo_ext2_state *s, struct ext2_dir_entry_2 *dirent, void *priv_data)
{
	struct ext2_inode e_ino;
	char *sym = (char *) 0;
	char symlink_buf[256];

	read_inode(s, ext2_to_cpu_32(dirent->inode), &e_ino);

	if (is_symlink(ext2_to_cpu_16(e_ino.i_mode)))
		sym = read_symlink(s, &e_ino, symlink_buf);

	register_silo_inode(ext2_to_cpu_32(e_ino.i_mtime),
			    ext2_to_cpu_32(e_ino.i_size),
			    ext2_to_cpu_16(e_ino.i_mode),
			    ext2_to_cpu_16(e_ino.i_uid),
			    ext2_to_cpu_16(e_ino.i_gid),
			    dirent->name, sym);
	return 0;
}

static void iterate_dir(struct silo_ext2_state *s, __u32 ino, dir_callback_t cb, void *cb_arg)
{
	struct ext2_dir_entry_2 e;
	struct ext2_inode ei;
	__u32 off, size;

	read_inode(s, ino, &ei);
	size = ext2_to_cpu_32(ei.i_size);

	for (off = 0; off < size; ) {
		read_file(s, &ei, off, 8, &e);

		if (ext2_to_cpu_16(e.rec_len) == 0)
			break;

		if (ext2_to_cpu_32(e.inode) == 0 ||
		    e.name_len == 0) {
			off += ext2_to_cpu_16(e.rec_len);
			continue;
		}

		read_file(s, &ei, off + 8, e.name_len, &e.name[0]);
		e.name[e.name_len] = 0;

		if (cb(s, &e, cb_arg))
			break;

		off += ext2_to_cpu_16(e.rec_len);
	}
}

static int ls_ext2 (void)
{
	struct silo_ext2_state *s = sstate;

	iterate_dir(s, inode, ls_callback, 0);

	return 0;
}

static int ino_size_ext2 (void)
{
	struct silo_ext2_state *s = sstate;
	struct ext2_inode ei = { };

	read_inode(s, inode, &ei);

	return ext2_to_cpu_32(ei.i_size);
}

struct namei_cb_arg {
	const char	*name;
	int		len;
	ino_t		*ino;
	int		found;
};

static int lookup_cb(struct silo_ext2_state *s, struct ext2_dir_entry_2 *dirent, void *priv_data)
{
	struct namei_cb_arg *p = priv_data;

	if (p->len != dirent->name_len)
		return 0;
	if (strncmp(p->name, dirent->name, p->len))
		return 0;
	p->found = 1;
	*p->ino = ext2_to_cpu_32(dirent->inode);
	return 1;
}

static int do_lookup(struct silo_ext2_state *s, __u32 dir_ino, const char *name,
		     int namelen, ino_t *ret_ino)
{
	struct namei_cb_arg arg;

	arg.name = name;
	arg.len = namelen;
	arg.ino = ret_ino;
	arg.found = 0;

	iterate_dir(s, dir_ino, lookup_cb, &arg);

	return arg.found ? 0 : -1;
}

static int open_namei(struct silo_ext2_state *s, const char *name,
		      int namelen, ino_t root, ino_t base, ino_t *ret_ino);

static int follow_link(struct silo_ext2_state *s, ino_t root, ino_t dir,
		       ino_t inode, ino_t *res_inode)
{
	struct ext2_inode ei = { };
	char symlink_buf[256];
	char *sym;

	read_inode(s, inode, &ei);
	if (!is_symlink(ext2_to_cpu_16(ei.i_mode))) {
		*res_inode = inode;
		return 0;
	}
	sym = read_symlink(s, &ei, symlink_buf);
	return open_namei(s, sym, ext2_to_cpu_32(ei.i_size), root, dir, res_inode);
}

static int dir_namei(struct silo_ext2_state *s, ino_t root, ino_t dir,
		     const char *pathname, int pathlen,
		     const char **name, int *namelen,
		     ino_t *res_inode)
{
	const char *thisname;
	int len, retval;
	ino_t inode;
	char c;

	if ((c = *pathname) == '/') {
		dir = root;
		pathname++;
		pathlen--;
	}
	while (1) {
		thisname = pathname;
		for (len = 0; --pathlen >= 0; len++) {
			c = *pathname++;
			if (c == '/')
				break;
		}
		if (pathlen < 0)
			break;
		retval = do_lookup(s, dir, thisname, len, &inode);
		if (retval)
			return retval;
		retval = follow_link(s, root, dir, inode, &dir);
		if (retval)
			return retval;
	}
	*name = thisname;
	*namelen = len;
	*res_inode = dir;
	return 0;
}

static int open_namei(struct silo_ext2_state *s, const char *name,
		      int namelen, ino_t root, ino_t base, ino_t *ret_ino)
{
	const char *base_name;
	ino_t dir_ino, inode;
	int ret;

	ret = dir_namei(s, root, base, name, namelen,
			&base_name, &namelen, &dir_ino);
	if (ret)
		return ret;

	if (!namelen) {
		*ret_ino = dir_ino;
		return 0;
	}

	ret = do_lookup(s, dir_ino, base_name, namelen, &inode);
	if (ret)
		return ret;

	ret = follow_link(s, root, dir_ino, inode, &inode);
	if (ret)
		return ret;

	*ret_ino = inode;
	return 0;
}

static int namei_follow_ext2 (const char *filename)
{
	int ret;

	ret = open_namei(sstate, filename, strlen(filename),
			 root, root, &inode);

	ext2_fs_ops.have_inode = inode ? 1 : 0;

	return ret;
}

static void print_error_ext2 (int error_val)
{
	printf("error: %d", error_val);
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
