/* CD-SILO : SILO CDROM (ISO-9660) boot block
   
   Copyright (C) 1996 Jakub Jelinek
   		 1998 Jan Vondrak
		 2003 Ben Collins
   
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
#include <linux/iso_fs.h>

#include <silo.h>
#include <rock.h>
#include <stringops.h>

#ifndef NULL
#define NULL (void *)0
#endif


#define SECOND_BLK	"/boot/second.b"
#define SILO_CONF	"/boot/silo.conf"
#define LOAD_ADDR	0x10000

struct isofs_inode {
	unsigned int extent;
	unsigned int size;
};

struct silo_info {
	char id;
	char conf_part;
	char part;
	char pad;
	char conf_file[256];
};


static struct isofs_inode root_ino;
static int link_count;
static char silo_conf[] = SILO_CONF;

static int open_namei(const char *pathname, struct isofs_inode *res_inode,
		      struct isofs_inode *base);

int prom_read(void *data, int size, unsigned long long offset);

#define cd_read_blk(block, data) \
	prom_read(data, ISOFS_BLOCK_SIZE, (unsigned long long)block * ISOFS_BLOCK_SIZE)
#define cd_read_blks(block, count, data) \
	prom_read(data, count * ISOFS_BLOCK_SIZE, (unsigned long long)block * ISOFS_BLOCK_SIZE)

static int isonum_733 (char * p)
{
	return ((p[0] & 0xff) | ((p[1] & 0xff) << 8)
		| ((p[2] & 0xff) << 16) | ((p[3] & 0xff) << 24));
}


static int isofs_read_super(void)
{
	struct iso_primary_descriptor iso;

	if (cd_read_blk(16, &iso) < 0)
		return -1;

	if (memcmp(iso.id, ISO_STANDARD_ID, sizeof (iso.id)))
		return -1;

	root_ino.extent = isonum_733 (((struct iso_directory_record *)
				(iso.root_directory_record))->extent);
	root_ino.size =   isonum_733 (((struct iso_directory_record *)
				(iso.root_directory_record))->size);

	return 0;
}


static void parse_rr (unsigned char *chr, unsigned char *end,
                      char *name, char *symlink)
{
	int cont_extent = 0, cont_offset = 0, cont_size = 0;
	struct rock_ridge *rr;
	int sig;
	int truncate = 0;
	int rootflag;

	*name = 0;

	while (chr < end) {
		rr = (struct rock_ridge *)chr;
		if (rr->len == 0)
			goto out;

		sig = (chr[0] << 8) + chr[1];
		chr += rr->len;

		switch (sig) {
			case SIG('R','R'):
				if ((rr->u.RR.flags[0] &
				    (RR_PX | RR_TF | RR_SL | RR_CL | RR_NM | RR_PX | RR_TF)) == 0)
					goto out;
				break;
			case SIG('N','M'):
				if (truncate || rr->u.NM.flags & ~1)
					break;

				if ((strlen(name) + rr->len - 5) >= 254) {
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

					while (slen > 1) {
						rootflag = 0;
						switch (slp->flags & ~1) {
							case 0:
								strncat(symlink, slp->text, slp->len);
								break;
							case 2:
								strcat(symlink, ".");
								break;
							case 4:
								strcat(symlink, "..");
								break;
							case 8:
								rootflag = 1;
								strcat(symlink, "/");
								break;
							default:
								break;
						}

						slen -= slp->len + 2;
						oldslp = slp;
						slp = (struct SL_component *) (((char *) slp) +
								slp->len + 2);

						if (slen < 2)
							break;

						if (!rootflag && (oldslp->flags & 1) == 0)
							strcat (symlink, "/");
					}
				}
				break;
			case SIG('C','E'):
				CHECK_CE;
				break;
			case SIG('P','X'):
			case SIG('T','F'):
				break;
		}

		if (chr >= end && cont_extent) {
			char sect_buf[ISOFS_BLOCK_SIZE];

			if (cd_read_blk(cont_extent, sect_buf) < 0)
				return;
			parse_rr(&sect_buf[cont_offset], &sect_buf[cont_offset +
				 cont_size - 3], name, symlink);
		}
	}

out:
	return;
}


static int isofs_lookup (struct isofs_inode *dir, const char *name,
			 int len, struct isofs_inode *result)
{
	char buffer [ISOFS_BLOCK_SIZE];
	char symlink [512];
	char namebuf [512];
	int block, size;
	struct iso_directory_record *idr;
	unsigned char *rr;

	size = dir->size;
	block = dir->extent;

	while (size > 0) {
		int i;

		if (cd_read_blk(block, buffer) < 0)
			return -1;

		size -= ISOFS_BLOCK_SIZE;
		block++;

		for (i = 0 ;; ) {
			idr = (struct iso_directory_record *) (buffer + i);

			if (!idr->length[0])
				break;

			i += (unsigned char)idr->length[0];
			memcpy(namebuf, idr->name, (unsigned char)idr->name_len[0]);
			namebuf[(unsigned char)idr->name_len[0]] = 0;

			rr = (unsigned char *)(idr + 1);
			rr += ((unsigned char)idr->name_len[0]) - sizeof(idr->name);

			if (!(idr->name_len[0] & 1))
				rr++;

			*symlink = 0;
			parse_rr(rr, &buffer[i-3], namebuf, symlink);

			if (idr->name_len[0] == 1 && !idr->name[0]) {
				namebuf[0] = '.';
				namebuf[1] = '\0';
			} else if (idr->name_len[0] == 1 && idr->name[0] == 1) {
				namebuf[0] = namebuf[1] = '.';
				namebuf[2] = '\0';
			}

			if (strlen(namebuf) == len && !memcmp(name, namebuf, len)) {
				if (*symlink) {
					int error;
					
					if (link_count > 5)
						return -1; /* Looping */
					
					link_count++;
					error = open_namei(symlink, result, dir);
					link_count--;
					
					return error;
				}

				result->extent = isonum_733 (idr->extent);
				result->size = isonum_733 (idr->size);
				return 0;
			}

			if (i >= ISOFS_BLOCK_SIZE - sizeof(struct iso_directory_record) +
			    sizeof(idr->name))
				break;
		}
	}

	return -1;
}


static int dir_namei(const char *pathname, int *namelen, const char **name,
		     struct isofs_inode *base, struct isofs_inode *res_inode)
{
	char c;
	const char *thisname;
	int len;
	struct isofs_inode this_inode;

	if ((c = *pathname) == '/') {
		base = &root_ino;
		pathname++;
	}

	while (1) {
		thisname = pathname;

		for (len = 0; (c = *(pathname++)) && (c != '/'); len++)
			/* Do nothing */;

		if (!c)
			break;

		if (isofs_lookup (base, thisname, len, &this_inode))
			return -1;

		base = &this_inode;
	}

	*name = thisname;
	*namelen = len;
	*res_inode = *base;

	return 0;
}


static int open_namei(const char *pathname,
		      struct isofs_inode *res_inode,
		      struct isofs_inode *base)
{
	struct isofs_inode dir;
	const char *basename;
	int namelen;

	if (dir_namei(pathname, &namelen, &basename, base, &dir))
		return -1;

	if (isofs_lookup(&dir, basename, namelen, res_inode))
		return -1;

	return 0;
}


char *cd_main (struct linux_romvec *promvec, void *cifh, void *cifs)
{
	struct isofs_inode inode;
	unsigned char *dest = (unsigned char *)LOAD_ADDR;
	struct silo_info *sinfo;

	prom_init(promvec, cifh, cifs);

	prom_putchar('S');

	if (diskinit())
		prom_halt();

	if (isofs_read_super())
		prom_halt();

	link_count = 0;

	if (open_namei(SECOND_BLK, &inode, &root_ino))
		prom_halt();

	prom_putchar('I');

	if (cd_read_blks(inode.extent, (inode.size + (ISOFS_BLOCK_SIZE - 1)) /
	    ISOFS_BLOCK_SIZE, dest) < 0)
		prom_halt();

	dest += 0x800;

	sinfo = (struct silo_info *)&dest[0x08];

	if (sinfo->id != 'L')
		prom_halt();

	memset(sinfo, 0, sizeof(*sinfo));
	sinfo->id = 'L';
	sinfo->conf_part = 1;
	memcpy(sinfo->conf_file, silo_conf, sizeof(silo_conf));

	prom_putchar(sinfo->id);

	return dest;
}


/* Utility functions */
int memcmp(const void *cs, const void *ct, size_t count)
{
	const unsigned char *su1, *su2;
	signed char res = 0;

	for (su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
		if ((res = *su1 - *su2) != 0)
			break;
	return res;
}

void *memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = (char *) dest, *s = (char *) src;
	while (count--)
		*tmp++ = *s++;
	return dest;
}

int strlen(const char *s)
{
	const char *sc;
	for (sc = s; *sc != '\0'; ++sc)
		/* Do nothing */;
	return sc - s;
}

char *strcat(char *dest, const char *src)
{
	char *tmp = dest;
	while (*dest) dest++;
	while ((*dest++ = *src++) != '\0');
	return tmp;
}

char *strncat(char *dest, const char *src, size_t n)
{
	char *tmp = dest;
	while (*dest) dest++;
	while (n && (*dest++ = *src++) != '\0') n--;
	if (!n) *dest = 0;
	return tmp;
}

int strcmp(const char *cs, const char *ct)
{
	register signed char __res;
	while (1)
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
	return __res;
}

void *memset(void *s,int c,size_t count)
{
	char *xs = (char *) s;
	while (count--)
		*xs++ = c;
	return s;
}
