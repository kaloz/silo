/* ISO9660 (CDROM) Interface for SILO filesystem access routines
   
   Copyright (C) 1999 Jakub Jelinek
                 1992,1993 Eric Youngdale
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

#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <linux/iso_fs.h>
#include <silo.h>
#include <file.h>
#include <stringops.h>
#include <rock.h>

/* Reuse and abuse */
typedef ext2_filsys isofs_filsys;

struct isofs_inode {
        unsigned int extent;
        unsigned int size;
};


static struct isofs_inode inode, root_inode;
static int link_count = 0;

void *alloca(size_t);
static int isofs_lookup (isofs_filsys, struct isofs_inode *,
			 const char *, int, struct isofs_inode *);
static int open_namei(isofs_filsys, const char *, struct isofs_inode *,
		      struct isofs_inode *);

static int ino_size_isofs (void)
{
    return inode.size;
}

static int ls_isofs (void)
{
    link_count = 0;
    return isofs_lookup (fs, &inode, "", 0, NULL);
}

static int isonum_731 (char * p)
{
	return ((p[0] & 0xff)
		| ((p[1] & 0xff) << 8)
		| ((p[2] & 0xff) << 16)
		| ((p[3] & 0xff) << 24));
}

#define isonum_733(p) isonum_731(p)

static int isofs_read_super(io_channel io)
{
    int i;
    struct iso_primary_descriptor iso;
    
    for (i = 16; i < 100; i++) {
        if (io_channel_read_blk (io, i, -2048, (char *)&iso))
            return -1;
        if (!strncmp (iso.id, ISO_STANDARD_ID, sizeof (iso.id)))
            break;
    }
    
    if (i == 100)
	    return -1;

    root_inode.extent = isonum_733 (((struct iso_directory_record *)
			    (iso.root_directory_record))->extent);
    root_inode.size = isonum_733 (((struct iso_directory_record *)
			    (iso.root_directory_record))->size);

    return 0;
}

static int open_isofs (char *device)
{
    fs = (isofs_filsys) malloc (sizeof (struct struct_ext2_filsys));
    if (!fs)
	return 0;

    if (((struct struct_io_manager *)(silo_io_manager))->open (device, 0, &fs->io))
	return 0;

    io_channel_set_blksize (fs->io, 2048);

    if (isofs_read_super(fs->io))
	return 0;

    return 1;
}

static int iso_date(char * p, int flag)
{
    int year, month, day, hour, minute, second, tz;
    int crtime, days, i;

    year = p[0] - 70;
    month = p[1];
    day = p[2];
    hour = p[3];
    minute = p[4];
    second = p[5];
    if (flag == 0) tz = p[6]; /* High sierra has no time zone */
    else tz = 0;

    if (year < 0) {
	crtime = 0;
    } else {
	int monlen[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

	days = year * 365;
	if (year > 2)
	    days += (year+1) / 4;
	for (i = 1; i < month; i++)
	    days += monlen[i-1];
	if (((year+2) % 4) == 0 && month > 2)
	    days++;
	days += day - 1;
	crtime = ((((days * 24) + hour) * 60 + minute) * 60)
	    + second;

	/* sign extend */
	if (tz & 0x80)
	    tz |= (-1 << 8);

	if (-52 <= tz && tz <= 52)
	    crtime -= tz * 15 * 60;
    }
    return crtime;
}


static void parse_rr (isofs_filsys fs, unsigned char *chr, unsigned char *end,
		      char *name, char *symlink, struct silo_inode *sino)
{
    int cont_extent = 0, cont_offset = 0, cont_size = 0;
    struct rock_ridge *rr;
    int cnt, sig;
    int truncate = 0;
    int symlink_len = 0;
    int rootflag;

    *name = 0;

    while (chr < end) {
	rr = (struct rock_ridge *) chr;
	if (rr->len == 0) goto out;
	sig = (chr[0] << 8) + chr[1];
	chr += rr->len;

	switch(sig){
	    case SIG('R','R'):
		if((rr->u.RR.flags[0] &
		    (RR_PX | RR_TF | RR_SL | RR_CL | RR_NM | RR_PX | RR_TF)) == 0)
		    goto out;
		break;
	    case SIG('N','M'):
		if (truncate) break;
		if (rr->u.NM.flags & 6) break;
		if (rr->u.NM.flags & ~1) {
		    printf ("Unsupported NM flag settings (%d)\n",rr->u.NM.flags);
		    break;
		}
		if((strlen(name) + rr->len - 5) >= 254) {
		    truncate = 1;
		    break;
		}
		strncat(name, rr->u.NM.name, rr->len - 5);
		break;
	    case SIG('S','L'):
		{
		    int slen;
		    struct SL_component * slp;
		    struct SL_component * oldslp;
		    slen = rr->len - 5;
		    slp = &rr->u.SL.link;
		    sino->size = symlink_len;

		    while (slen > 1) {
			rootflag = 0;
			switch(slp->flags &~1) {
			    case 0:
				sino->size += slp->len;
				strncat (symlink, slp->text, slp->len);
				break;
			    case 2:
				sino->size += 1;
				strcat (symlink, ".");
				break;
			    case 4:
				sino->size += 2;
				strcat (symlink, "..");
				break;
			    case 8:
				rootflag = 1;
				sino->size += 1;
				strcat (symlink, "/");
				break;
			    default:
				printf("Symlink component flag not implemented\n");
			}
			slen -= slp->len + 2;
			oldslp = slp;
			slp = (struct SL_component *) (((char *) slp) + slp->len + 2);

			if (slen < 2) {
			    if(((rr->u.SL.flags & 1) != 0)
				    && ((oldslp->flags & 1) == 0) ) sino->size += 1;
			    break;
			}

			/*
			 * If this component record isn't continued, then append a '/'.
			 */
			if (!rootflag && (oldslp->flags & 1) == 0) {
			    strcat (symlink, "/");
			    sino->size += 1;
			}

		    }
		}
		symlink_len = sino->size;
		break;
	    case SIG('C','E'):
		CHECK_CE;
		break;
	    case SIG('P','X'):
		sino->mode = isonum_733(rr->u.PX.mode);
		sino->uid  = isonum_733(rr->u.PX.uid);
		sino->gid  = isonum_733(rr->u.PX.gid);
		break;
	    case SIG('T','F'):
		cnt = 0;
		if(rr->u.TF.flags & TF_CREATE)
		    cnt++;
		if(rr->u.TF.flags & TF_MODIFY)
		    sino->mtime = iso_date(rr->u.TF.times[cnt++].time, 0);
		break;
	}
	if (chr >= end && cont_extent) {
	    char *sect = alloca (2048);
	    if (io_channel_read_blk (fs->io, cont_extent, 1, sect))
		return;
	    parse_rr (fs, &sect [cont_offset], &sect [cont_offset + cont_size - 3],
		      name, symlink, sino);
	}
    }
out:
    return;
}

static int isofs_lookup (isofs_filsys fs, struct isofs_inode *dir,
		       const char *name, int len, struct isofs_inode *result)
{
    char buffer [2048];
    char namebuf [512];
    char symlink [512];
    int block, size, i;
    struct iso_directory_record *idr;
    unsigned char *rr;
    struct silo_inode sino;

    size = dir->size;
    block = dir->extent;

    while (size > 0) {
	if (io_channel_read_blk (fs->io, block, 1, buffer)) {
	    printf ("Could not read directory\n");
	    return -1;
	}

	size -= 2048;
	block++;

	for (i = 0;;) {
	    idr = (struct iso_directory_record *) (buffer + i);
	    if (!idr->length[0])
		break;

	    i += (unsigned char)idr->length[0];
	    strncpy(namebuf, idr->name, (unsigned char)idr->name_len[0]);
	    namebuf[(unsigned char)idr->name_len[0]] = 0;

	    rr = (unsigned char *)(idr + 1);
	    rr += ((unsigned char)idr->name_len[0]) - sizeof(idr->name);

	    if (!(idr->name_len[0] & 1))
		rr++;

	    *symlink = 0;
	    memset(&sino, 0, sizeof(struct silo_inode));
	    parse_rr (fs, rr, &buffer[i-3], namebuf, symlink, &sino);

	    if (idr->name_len[0] == 1 && !idr->name[0])
		strcpy(namebuf, ".");
	    else if (idr->name_len[0] == 1 && idr->name[0] == 1)
		strcpy(namebuf, "..");

	    if (result == NULL) {
		/* We aren't returning a result inode, so we must be
		 * iterating...  */
		register_silo_inode(sino.mtime, sino.size?:isonum_733(idr->size),
				    sino.mode, sino.uid, sino.gid, namebuf,
				    *symlink ? symlink : NULL);
	    } else if ((!len && namebuf[0] == '.' && !namebuf[1]) || 
	        (strlen(namebuf) == len && !memcmp(namebuf, name, len))) {
		if (*symlink) {
		    int error;
		    if (link_count > 5) {
			printf ("Symlink loop, stopping.\n");
			return -1; /* Loop */
		    }
		    link_count++;
		    error = open_namei (fs, symlink, result, dir);
		    link_count--;
		    return error;
		}
		result->extent = isonum_733 (idr->extent);
		result->size = isonum_733 (idr->size);
		return 0;
	    }

	    if (i >= 2048 - sizeof(struct iso_directory_record) + sizeof(idr->name))
		break;
	}
    }
    if (result == NULL)
	return 0;
    else
	return -1;
}

static int dir_namei(isofs_filsys fs, const char *pathname, int *namelen, 
		     const char **name, struct isofs_inode *base,
		     struct isofs_inode *res_inode)
{
    char c;
    const char *thisname;
    int len;
    struct isofs_inode inode;

    if ((c = *pathname) == '/') {
	base = &root_inode;
	pathname++;
    }
    while (1) {
	thisname = pathname;
	for(len=0;(c = *(pathname++))&&(c != '/');len++);
	if (!c) break;
	if (isofs_lookup (fs, base, thisname, len, &inode)) return -1;
	base = &inode;
    }
    *name = thisname;
    *namelen = len;
    *res_inode = *base;
    return 0;
}

static int open_namei(isofs_filsys fs, const char *pathname, 
		      struct isofs_inode *res_inode,
		      struct isofs_inode *base)
{
    const char *basename;
    int namelen;
    struct isofs_inode dir, inode;

    if (dir_namei(fs, pathname, &namelen, &basename, base, &dir)) return -1;
    if (!namelen) {			/* special case: '/usr/' etc */
	*res_inode=dir;
	return 0;
    }
    if (isofs_lookup (fs, &dir, basename, namelen, &inode)) return -1;
    *res_inode = inode;
    return 0;
}

struct fs_ops iso_fs_ops;

static int isofs_namei (const char *filename)
{
    int ret;
    link_count = 0;

    ret = open_namei (fs, filename, &inode, &root_inode);
    iso_fs_ops.have_inode = (ret) ? 0 : 1;

    return ret;
}

static void isofs_close(void)
{
    free (fs->io);
    free (fs);
}

static int isofs_block_iterate(void)
{
    int i;
    blk_t nr;
    int size;

    nr = inode.extent;
    size = (inode.size + 2047) / 2048;
    for (i = 0; i < size; i++, nr++) {
        switch (dump_block (&nr, i)) {
            case BLOCK_ABORT:
            case BLOCK_ERROR:
            	return 0;
        }
    }
    return dump_finish();
}

static void print_error_isofs (int error_val) {
    printf("Unknown isofs error");
}

struct fs_ops iso_fs_ops = {
    name:		"ISO-9660 CDROM",
    open:		open_isofs,
    ls:			ls_isofs,
    dump:		isofs_block_iterate,
    close:		isofs_close,
    ino_size:		ino_size_isofs,
    print_error:        print_error_isofs,
    namei_follow:       isofs_namei,
    have_inode:		0,
};
