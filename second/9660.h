typedef ext2_filsys iso9660_filsys;

struct iso9660_inode {
	unsigned int extent;
	unsigned int size;
};

int iso9660_open(char *device, io_manager io, iso9660_filsys *fs);
int iso9660_namei(iso9660_filsys fs, char *filename, struct iso9660_inode *inode);
void iso9660_close(iso9660_filsys fs);
int iso9660_block_iterate(iso9660_filsys fs, struct iso9660_inode *inode, 
			  int (*func)(iso9660_filsys, blk_t *, int, void *), 
			  void *private);
