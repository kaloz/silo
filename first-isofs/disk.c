/* Disk functions
   
   Copyright (C) 1996 Pete A. Zaitcev
   		 1996,1997 Jakub Jelinek
   
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
#include <stringops.h>

static int fd;

int diskinit (void)
{
	char bootdevice[1024];
	char *s = bootdevice;

	if (prom_vers == PROM_V0) {
		struct linux_arguments_v0 *ap = *romvec->pv_v0bootargs;

		*s++ = ap->boot_dev[0];
		*s++ = ap->boot_dev[1];
		*s++ = '(';
		*s++ = (ap->boot_dev_ctrl & 07) + '0';
		*s++ = ',';
		// Hopefully it's never > 10
		*s++ = ap->boot_dev_unit;
		*s++ = ','; 
		*s++ = '0';
		*s++ = ')';
		*s = 0;

		fd = (*romvec->pv_v0devops.v0_devopen) (bootdevice);
	} else {
		if (prom_vers == PROM_P1275)
			prom_getproperty (prom_chosen, "bootpath", bootdevice, sizeof(bootdevice));
		else
			memcpy(bootdevice, *romvec->pv_v2bootargs.bootpath,
			       strlen(*romvec->pv_v2bootargs.bootpath));

		for (; *s && *s != ':'; s++)
			/* Do nothing */;

		if (!*s) {
			*s++ = ':'; *s++ = 'a'; *s = 0;
		} else if (s[1] >= 'a' && s[1] <= 'z' && !s[2])
			s[1] = 'a';

		if (prom_vers == PROM_P1275)
			fd = p1275_cmd ("open", 1, bootdevice);
		else
			fd = (*romvec->pv_v2devops.v2_dev_open) (bootdevice);
	}

	if (fd == 0 || fd == -1)
		return 1;

	return 0;
}

/* We assume that size is always a 512byte multiple and offset is always
 * on a 512byte boundary */
int prom_read(void *data, int size, unsigned long long offset)
{
	int ret;

	if (!size)
		return 0;

	if (prom_vers == PROM_V0) {
		ret = (*romvec->pv_v0devops.v0_rdblkdev)
				(fd, size >> 9, (unsigned)(offset >> 9), data);

	        if (ret != (size >> 9))
			return -1;

		return ret;
	} else {
		if (prom_vers == PROM_P1275) {
			if (p1275_cmd("seek", -3, (unsigned long long)
				      (unsigned long)fd, 0LL, offset) == -1)
				return -1;
		} else {
			if ((*romvec->pv_v2devops.v2_dev_seek)
				(fd, (unsigned)(offset >> 32), (unsigned)offset) == -1)
				return -1;
		}

		if (prom_vers == PROM_P1275) {
			ret = p1275_cmd ("read", 3, fd, data, size);
		} else {
			ret = (*romvec->pv_v2devops.v2_dev_read) (fd, data, size);
		}

		if (ret != size)
			ret = -1;
	}

	return ret;
}
