/* ls command handling
   
   Copyright (C) 1999 Jakub Jelinek
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#  include <ctype.h>
#  include <sys/types.h>
#  include "silo.h"
typedef int FILE;
#  include <linux/ext2_fs.h>

#include "stringops.h"
#include "ext2fs/ext2fs.h"

int ls_opt = 0;

static int do_ls_cmp (struct silo_inode *a, struct silo_inode *b)
{
    int ret;
    ret = strcmp (a->name, b->name);
    if (ret) {
	if (!strcmp (a->name, ".")) return -1;
	if (!strcmp (b->name, ".")) return 1;
	if (!strcmp (a->name, "..")) return -1;
	if (!strcmp (b->name, "..")) return 1;
    }
    if (ls_opt & LSOPT_T) {
	if (a->mtime < b->mtime)
	    ret = 1;
	else if (a->mtime > b->mtime)
	    ret = -1;
    }
    if (ls_opt & LSOPT_R)
	ret = -ret;
    return ret;
}

static void sortit (struct silo_inode **b, int n, struct silo_inode **t)
{
    struct silo_inode **tmp;
    struct silo_inode **b1, **b2;
    int n1, n2;

    if (n <= 1)
	return;

    n1 = n / 2;
    n2 = n - n1;
    b1 = b;
    b2 = b + n1;

    sortit (b1, n1, t);
    sortit (b2, n2, t);

    tmp = t;

    while (n1 > 0 && n2 > 0) {
	if (do_ls_cmp (*b1, *b2) <= 0) {
	    *tmp = *b1;
	    b1++;
	    --n1;
	} else {
	    *tmp = *b2;
	    b2++;
	    --n2;
	}
	tmp++;
    }
    if (n1 > 0)
	memcpy (tmp, b1, n1 * sizeof (*b));
    memcpy (b, t, (n - n2) * sizeof (*b));
}

static void ls_rwx (unsigned int bits, char *chars)
{
    chars[0] = (bits & LINUX_S_IRUSR) ? 'r' : '-';
    chars[1] = (bits & LINUX_S_IWUSR) ? 'w' : '-';
    chars[2] = (bits & LINUX_S_IXUSR) ? 'x' : '-';
}

static void ls_modestring (unsigned int mode, char *chars)
{
    if (LINUX_S_ISBLK (mode)) chars[0] = 'b';
    else if (LINUX_S_ISCHR (mode)) chars[0] = 'c';
    else if (LINUX_S_ISDIR (mode)) chars[0] = 'd';
    else if (LINUX_S_ISREG (mode)) chars[0] = '-';
    else if (LINUX_S_ISFIFO (mode)) chars[0] = 'p';
    else if (LINUX_S_ISLNK (mode)) chars[0] = 'l';
    else if (LINUX_S_ISSOCK (mode)) chars[0] = 's';
    ls_rwx ((mode & 0700) << 0, chars+1);
    ls_rwx ((mode & 0070) << 3, chars+4);
    ls_rwx ((mode & 0007) << 6, chars+7);
    if (mode & LINUX_S_ISUID) {
	if (chars[3] != 'x')
	    chars[3] = 'S';
	else
	    chars[3] = 's';
    }
    if (mode & LINUX_S_ISGID) {
	if (chars[6] != 'x')
	    chars[6] = 'S';
	else
	    chars[6] = 's';
    }
    if (mode & LINUX_S_ISVTX) {
	if (chars[9] != 'x')
	    chars[9] = 'T';
	else
	    chars[9] = 't';
    }
}

void print_number (unsigned int num, int pad, char padc)
{
    int len;
    unsigned int i;
    for (len = 1, i = num; i >= 10; len++) i /= 10;
    if (pad > 0) { /* Pad left */
	while (len < pad) { putchar (padc); len++; }
	printf ("%d", num);
    } else {
	printf ("%d", num);
	while (len < -pad) { putchar (padc); len++; }
    }
}

void do_ls (unsigned char *buf)
{
    int i, n, j;
    struct silo_inode *sino;
    struct silo_inode **array, **p;

    n = 0;    
    for (sino = (struct silo_inode *) buf; sino->inolen;
	 sino = (struct silo_inode *) (((char *)sino) + sino->inolen))
	n++;
    if (!n) return;
    array = p = (struct silo_inode **)sino;
    for (sino = (struct silo_inode *) buf, i = 0; i < n;
	 sino = (struct silo_inode *) (((char *)sino) + sino->inolen), i++)
	*p++ = sino;
    sortit (array, n, array + n);
    if (ls_opt & LSOPT_L) {
	char mode[11];
	char *q;
	unsigned int mtime, day, hour, min, month, year;
	static char *months[] = {
	    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	static unsigned short m_yday[] = {
	    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 };
	for (i = 0; i < n; i++) {
	    ls_modestring (array[i]->mode, mode);
	    mode[10] = 0;
	    printf ("%s ", mode);
	    print_number (array[i]->uid, -8, ' ');
	    printf (" ");
	    print_number (array[i]->gid, -8, ' ');
	    printf (" ");
	    print_number (array[i]->size, 8, ' ');
	    printf (" ");
	    mtime = array[i]->mtime;
	    /* Following code taken from glibc */
	    day = mtime / (60 * 60 * 24);
	    hour = mtime % (60 * 60 * 24);
	    hour /= 60;
	    min = hour % 60;
	    hour /= 60;
	    year = 1970;
#define ISLEAP(year) ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#define DIV(a, b) ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y) (DIV (y, 4) - DIV (y, 100) + DIV (y, 400))
 	    while (day < 0 || day >= (ISLEAP (year) ? 366 : 365)) {
		/* Guess a corrected year, assuming 365 days per year.  */
		long int yg = year + day / 365 - (day % 365 < 0);
		/* Adjust DAYS and Y to match the guessed year.  */
		day -= ((yg - year) * 365 + LEAPS_THRU_END_OF (yg - 1) - LEAPS_THRU_END_OF (year - 1));
		year = yg;
	    }
	    if (!ISLEAP(year) && day >= 59) day++;
	    for (month = 11; day < m_yday[month]; month--);
	    day -= m_yday[month] - 1;
	    printf ("%s ", months[month]);
	    print_number (day, 2, ' ');
	    putchar (' ');
	    print_number (hour, 2, '0');
	    putchar (':');
	    print_number (min, 2, '0');
	    printf (" %d %s", year, array[i]->name);
	    if (LINUX_S_ISLNK (array[i]->mode)) {
		q = strchr (array[i]->name, 0) + 1;
		if (*q) printf (" -> %s", q);
	    }
	    printf ("\n");
	}
    } else {
	for (i = 0; i < n; i++) {
	    printf ("%s", array[i]->name);
	    j = 19 - strlen(array[i]->name);
	    if ((i & 3) == 3 || i == n - 1)
		printf ("\n");
	    else
		do printf (" "); while (j-- > 0);
	}
    }
}
