/* Filesystem interface abstraction.
   
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

#include <silo.h>
#include <file.h>
#include <stringops.h>
#include <setjmp.h>

ext2_filsys fs = 0;

unsigned int bs;
unsigned char *filebuffer;
ino_t root, cwd;

static int do_gunzip = 0;
static unsigned int *gzipped_blocks;
static unsigned int *cur_gzipped_block;

static unsigned char *filelimit;
static int first_block;
static int block_no;
static int block_cnt;
static int last_blockcnt;
static char *gunzip_buffer;
static char *gunzip_inp;
static char *gunzip_endbuf;
static char *match;

/* Externally provided filesystem interfaces */
extern struct fs_ops ext2_fs_ops;
extern struct fs_ops iso_fs_ops;
extern struct fs_ops rom_fs_ops;
extern struct fs_ops ufs_fs_ops;

/* Array of our supported ops */
static struct fs_ops *silo_fs_ops[] = {
    &ext2_fs_ops,
    &iso_fs_ops,
    &rom_fs_ops,
    &ufs_fs_ops,
    NULL,
};

static struct fs_ops *cur_ops;

extern jmp_buf gunzip_env;

void register_silo_inode (unsigned int mtime, unsigned int size,
			  unsigned int mode, unsigned int uid,
			  unsigned int gid, const char *name,
			  const char *symlink)
{
    struct silo_inode *sino = (struct silo_inode *)filebuffer;
    unsigned char *p;
    int name_len = strlen(name);

    if (match != NULL)
        if (strlen(match) > name_len || strncmp(match, name, strlen(match)))
            return;

    strncpy(sino->name, name, name_len);
    sino->name[name_len] = 0;
    sino->mtime = mtime;
    sino->size = size;
    sino->mode = mode;
    sino->uid = uid;
    sino->gid = gid;

    p = strchr (sino->name, 0) + 1;
    if (symlink) {
        strncpy (p, symlink, size);
        p[size] = 0;
        p += size + 1;
    }
    if ((long)p & 3) p += 4 - ((long)p & 3);
    sino->inolen = p - filebuffer;
    filebuffer = p;

    return;
}

static unsigned char get_gzip_input (void)
{

    if (gunzip_inp < gunzip_endbuf)
	return *gunzip_inp++;
    {
	int count = 1;
	unsigned int first = *cur_gzipped_block++;

	if (!first) {
	    printf ("\nDecompression error: ran out of compressed data\n");
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
static int do_rotate = 0;

static void rotate (void)
{
    static int i = 0;
    static char rot[] = "\\|/-";

    if (!do_rotate)
	return;

    printf ("%c\b", rot[i % 4]);

    i++;
}

int dump_block (blk_t * blocknr, int blockcnt)
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
                if (*filebuffer == 037 && (filebuffer[1] == 0213 || filebuffer[1] == 0236)) {   /* gzip magic */
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
	    rotate();
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

int dump_finish (void)
{
    if (block_no) {
	blk_t tmp = 0;
	if (dump_block (&tmp, 0))
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

static int dump_device_range (char *filename, char *bogusdev, int *len,
			      void (*lenfunc)(int, char **, char **))
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
	if (!((struct struct_io_manager *)(silo_io_manager))->open (bogusdev, 0, &fs->io)) {
	    blk_t tmp;

	    first_block = do_gunzip;
	    block_no = 0;
	    last_blockcnt = 0;
	    block_cnt = 0;
	    for (tmp = start; tmp < end; tmp++) {
		if (dump_block (&tmp, tmp - start))
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

int load_file (char *device, int partno, char *filename, char *buffer,
	       char *limit, int *len, int cmd,
	       void (*lenfunc)(int, char **, char **))
{
    struct silo_inode *sino;
    int retval = 0, i;
    int size = -1;
    char bogusdev[] = "/dev/sdaX";
    char *bogdev;
    void *mmark;
    char *dir = NULL;
    size_t fn_len;

    mark (&mmark);
    if (!device)
	device = bootdevice;

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
	goto done_2;

    do_gunzip = cmd & LOADFILE_GZIP;
    if (cmd & ~LOADFILE_GZIP)
	do_rotate = 0;
    else
	do_rotate = 1;

    filebuffer = buffer;
    filelimit = limit;

    if (*filename == '[') {
	if (cmd & LOADFILE_LS) {
	    if (!(cmd & LOADFILE_QUIET))
		printf ("You cannot ls a device range\n");
	    goto done_2;
	}
    	solaris = 0;
	retval = dump_device_range (filename, bogdev, len, lenfunc);
	goto done_2;
    }

    solaris = 0; /* The UFS module will set this if needed */
    for (i = 0; silo_fs_ops[i]; i++)
	if (silo_fs_ops[i]->open(bogdev))
	    break;

    if (!silo_fs_ops[i]) {
	if (!(cmd & LOADFILE_QUIET))
	    fatal ("Unable to open filesystem");
	goto done_2;
    } else
	cur_ops = silo_fs_ops[i];

    if (cmd & LOADFILE_LS && cur_ops->ls == NULL) {
	if (!(cmd & LOADFILE_MATCH))
	    printf("\nls is not supported for the `%s' filesystems\n", cur_ops->name);
	goto done_1;
    }

    fn_len = strlen(filename);

    /* Find the inode for the filename. If we are matching, always parse
     * basedir and basename.  */
    if (cmd & LOADFILE_MATCH && fn_len > 1 && filename[fn_len - 1] != '/') {
	dir = strdup(filename);
	if ((match = strrchr(dir, '/')) != NULL && strlen(match) > 1) {
	    char *base = "/";
	    if (match != dir) base = dir;
	    *match = '\0';
	    match++;
	    retval = cur_ops->namei_follow (base);
	}
    } else {
	match = NULL;
	retval = cur_ops->namei_follow (filename);
    }

    if (retval) {
	if (!(cmd & LOADFILE_QUIET)) {
	    printf ("\nCannot find %s (", dir != NULL ? dir : filename);
	    cur_ops->print_error (retval);
	    printf (")\n");
	}
	retval = 0;
	goto done_1;
    }

    if (lenfunc) {
        do_gunzip = 0;
	size = cur_ops->ino_size();
        (*lenfunc)(size, (char **)&filebuffer, (char **)&filelimit);
        do_gunzip = cmd & LOADFILE_GZIP;
    }

    first_block = do_gunzip;
    last_blockcnt = 0;
    block_no = 0;
    block_cnt = 0;
    retval = 0;

    if (cur_ops->have_inode) {
	if (cmd & LOADFILE_LS) {
	    if ((retval = cur_ops->ls())) {
		if (!(cmd & LOADFILE_MATCH)) {
		    printf("\nError: could not list (");
		    cur_ops->print_error(retval);
		    printf(").\n");
		}
		retval = 0;
	    } else {
		sino = (struct silo_inode *)filebuffer;
		sino->inolen = 0;
		retval = 1;
	    }
	} else {
	    retval = cur_ops->dump();
	    if (!retval && !(cmd & LOADFILE_MATCH))
		printf("\nError loading %s\n", filename);
	}
    }

    if (retval && len) {
	if (size != -1)
	    *len = size;
	else if (do_gunzip)
	    *len = gunzipped_len;
	else
	    *len = cur_ops->ino_size();
    }

done_1:
    if (dir) free(dir);
    cur_ops->close();
done_2:
    release (mmark);

    return retval;
}
