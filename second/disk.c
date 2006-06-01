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

static int net = 0;
static int floppy = 0;
static unsigned int flash = 0;
static int fd;
static unsigned long long seekp;
static char bootdevice[4096];
static char currentdevice[4096];

char *silo_disk_get_bootdevice(void)
{
	return bootdevice;
}

int silo_disk_open(char *device)
{
    strcpy (currentdevice, device);
    net = 0;
    floppy = 0;
    seekp = 0xffffffffffffffffULL;
    switch (prom_vers) {
    case PROM_V0:
    	{
        	char buffer[20], *p;
        
	        if (strlen (device) < 20) {
        	    /* v0 prom likes to paint in devopen parameter sometimes... */
	            strcpy (buffer, device);
        	    p = buffer;
	        } else p = device;
		fd = (*romvec->pv_v0devops.v0_devopen) (p);
		if (device[0] == 'f' && device[1] == 'd')
		    floppy = 1;
		else if ((device[0] == 'l' || device[0] == 'i') && device[1] == 'e')
		    net = 1;
	}
	break;
    case PROM_V2:
    case PROM_V3:
    	{
    		int node;
	    	char buffer[20];
    	
		fd = (*romvec->pv_v2devops.v2_dev_open) (device);
		if ((unsigned)(fd + 1) > 1) {
		    node = (romvec->pv_v2devops.v2_inst2pkg) (fd);
		    prom_getstring (node, "device_type", buffer, 20);
	    	    if (!strcmp (buffer, "network"))
	                net = 1;
	        }
	}
	break;
    case PROM_P1275:
        {
        	int node;
        	char buffer [20];

        	fd = p1275_cmd ("open", 1, device);
        	if ((unsigned)(fd + 1) > 1) {
		    node = p1275_cmd ("instance-to-package", 1, fd);
                    /*
                     * Don't use our argument due to devalias.
                     * Alas, flash has no device_type property.
                     */
                    prom_getstring (node, "name", buffer, 20);
                    if (!strcmp (buffer, "flash-memory")) {
                        int reg[3];
                        reg[0] = 0;  reg[1] = 0;  reg[2] = 0;
                        prom_getproperty (node, "reg", (char*)&reg[0], sizeof (reg));
                        flash = reg[1];
                    } else {
		        prom_getstring (node, "device_type", buffer, 20);
	    	        if (!strcmp (buffer, "network"))
	                    net = 1;
                    }
	        }
        }
        break;
    }
    if (fd == 0 || fd == -1) {
	printf ("\nFatal error: Couldn't open device %s\n", device);
	return -1;
    }
    return 0;
}

extern unsigned char boot_part, boot_parts[32];
extern unsigned char raid_dsk_number;

int get_boot_part(void)
{
    int ret = boot_part;
    if (raid_dsk_number > 32)
	printf("Internal error. RAID disk number should be at most 32.\n");
    else if (raid_dsk_number)
	ret = boot_parts[raid_dsk_number - 1];
    return ret;
}

int silo_diskinit(void)
{
    fd = 0;
    if (prom_vers == PROM_V0) {
	struct linux_arguments_v0 *ap = *romvec->pv_v0bootargs;
	char *s = bootdevice;

	*s++ = ap->boot_dev[0];
	*s++ = ap->boot_dev[1];
	*s++ = '(';
	*s++ = (ap->boot_dev_ctrl & 07) + '0';
	*s++ = ',';
	if ((*s = ap->boot_dev_unit / 10 + '0') != '0')
	    s++;
	*s++ = ap->boot_dev_unit % 10 + '0';
	*s++ = ','; 
	*s++ = get_boot_part() + '0';
	*s++ = ')';
	*s = 0;
    } else {
        char *p;
        if (prom_vers == PROM_P1275)
            prom_getproperty (prom_chosen, "bootpath", bootdevice, sizeof(bootdevice));
        else
	    strcpy (bootdevice, *romvec->pv_v2bootargs.bootpath);
	p = strchr (bootdevice, ':');
	if (!p) {
	    p = strchr (bootdevice, 0); *p++ = ':'; *p++ = get_boot_part() + 'a'; *p = 0;
	} else if (p[1] >= 'a' && p[1] <= 'z' && !p[2])
	    p[1] = get_boot_part() + 'a';
    }
    return silo_disk_open(bootdevice);
}

static void silo_disk_reopen(void)
{
    char c;
    
    c = *currentdevice;
    silo_disk_close();
    *currentdevice = c;
    silo_disk_open(currentdevice);
}

static unsigned int flash_ld(unsigned int offset)
{
    unsigned int retval;

    offset += flash;
    __asm__ __volatile__("lda [%2] %1, %0\n\t" :
        "=r" (retval) : "i" (0x20), "r" (offset));
    return retval;
}

int silo_disk_read(char *buff, int size, unsigned long long offset)
{
    if (!size)
	return 0;
    if (prom_vers == PROM_V0) {
    	if (net)
    	    return (*romvec->pv_v0devops.v0_rdnetdev) (fd, size, buff);
	else {
	    char buffer[512];
	    int i = 0, j, k, rc = 0, ret = 0;

	    if (offset & 0x1ff) {
	        if (size > 512 - (offset & 0x1ff))
		    i = 512 - (offset & 0x1ff);
	        else
		    i = size;
		for (j = 0; j < 5; j++) {
	        	rc = (*romvec->pv_v0devops.v0_rdblkdev) (fd, 1, (unsigned)(offset >> 9), buffer);
	        	if (rc) break;
	        	silo_disk_reopen();
	        }
	        if (rc != 1)
		    return -1;
	        memcpy (buff, buffer + (offset & 0x1ff), i);
	        buff += i;
	        size -= i;
	        offset = ((offset + 512) & ~0x1ffULL);
	        ret = i;
	    }
	    if (size >> 9) {
		for (j = 0; j < 5; j++) {
	        	rc = (*romvec->pv_v0devops.v0_rdblkdev) (fd, size >> 9, (unsigned)(offset >> 9), buff);
	        	if (rc) break;
	        	silo_disk_reopen();
	        }
	        if (rc != (size >> 9)) {
		    /* Lets try if the floppy is not happy because the read size is too large for it */
		    for (k = 0; k < (size >> 9); k++) {
			for (j = 0; j < 5; j++) {
			    rc = (*romvec->pv_v0devops.v0_rdblkdev) (fd, 1, (unsigned)(offset >> 9) + k, buff + (k << 9));
			    if (rc) break;
			    silo_disk_reopen();
			}
			if (rc != 1)
			    return -1;
		    }
		}
	        i = (size & (~0x1ff));
	        ret += i;
	        buff += i;
	        offset += i;
	    }
	    size &= 0x1ff;
	    if (size) {
		for (j = 0; j < 5; j++) {
	        	rc = (*romvec->pv_v0devops.v0_rdblkdev) (fd, 1, (unsigned)(offset >> 9), buffer);
	        	if (rc) break;
	        	silo_disk_reopen();
	        }
	        if (rc != 1)
		    return -1;
	        memcpy (buff, buffer, size);
	        ret += size;
	    }
	    return ret;
	}
    } else {
	int rc = 0;

        if (flash) {
            unsigned int word;
            int xlen;
            int count;

            if (offset >= 0x1000000) {	/* Not very precise but will work... */
                printf ("Reading beyond 16MB of flash, bad filesystem.\n");
	    	return -1;
            }

            /*
             * Right thing is to map stuff in, then do a copy.
             * We, however, are not sure that PROM does not leak mappings.
             * So, let's kludge data in with ASI_BYPASS.
             * Note that flash must be accessed with 32 bits loads.
             */
            offset += 1024;	/* XXX Offset of flash filesystem */
            count = 0;

            xlen = 4 - (offset & 3);
            if (xlen != 4) {
                word = flash_ld (offset & ~3);
                memcpy (buff, ((char *)&word) + (4 - xlen), xlen);
                buff += xlen;
                offset += xlen;
                count += xlen;
            }

            while (count + 4 <= size) {
                word = flash_ld (offset);
                memcpy (buff, (char *)&word, 4);
                buff += 4;
                offset += 4;
                count += 4;
            }

            xlen = size - count;
            if (xlen != 0) {
                word = flash_ld (offset);
                memcpy (buff, (char *)&word, xlen);
                buff += xlen;
                offset += xlen;
                count += xlen;
            }

            return count;
        }

	if (!net) {
	    if (prom_vers != PROM_P1275) {
		    if (((romvec->pv_printrev >> 16) < 2 || 
		         ((romvec->pv_printrev >> 16) == 2 && (romvec->pv_printrev && 0xffff) < 6)) 
		        && offset >= 0x40000000) {
		    	printf ("Buggy old PROMs don't allow reading past 1GB from start of the disk. Send complaints to SMCC\n");
	    		return -1;
	    	    }
	    }
	    if (seekp != offset) {
	    	if (prom_vers == PROM_P1275) {
		        if ((rc = p1275_cmd ("seek", P1275_ARG_64B(2) | 3, fd, 0, offset)) == -1)
			    return -1;
	    	} else {
		        if ((*romvec->pv_v2devops.v2_dev_seek) (fd, (unsigned)(offset >> 32), (unsigned)offset) == -1)
			    return -1;
		}
	        seekp = offset;
	    }
	}
	if (prom_vers == PROM_P1275) {
		rc = p1275_cmd ("read", 3, fd, buff, size);
	} else {
		int i;
		for (i = 0; i < 2; i++) {
		    rc = (*romvec->pv_v2devops.v2_dev_read) (fd, buff, size);
		    if (rc == size) break;
		    silo_disk_reopen();
		    if ((*romvec->pv_v2devops.v2_dev_seek) (fd, (unsigned)(offset >> 32), (unsigned)offset) == -1)
			return -1;
		}
		if (rc != size && size > 32768) {
		    int j, s = size;
		    silo_disk_reopen();
		    while (size) {
			if (size < 32768) j = size; else j = 32768;
			if (silo_disk_read(buff, j, offset) != j)
			    return -1;
			size -= j;
			offset += j;
			buff += j;
		    }
		    return s;
		}
	}
	if (!net) {
	    seekp += size;
	    if (rc == size)
	        return size;
	} else
	    return rc;
    }
    return -1;
}

void silo_disk_close(void)
{
    if (*currentdevice) {
    	switch (prom_vers) {
    	case PROM_V0:
	    (*romvec->pv_v0devops.v0_devclose) (fd); break;
	case PROM_V2:
	case PROM_V3:
	    (*romvec->pv_v2devops.v2_dev_close) (fd); break;
	case PROM_P1275:
	    p1275_cmd ("close", 1, fd); break;
	}
    }
    *currentdevice = 0;
}

int silo_disk_setdisk(char *device)
{
    if (!strcmp(currentdevice, device))
	return 0;

    silo_disk_close();
    return silo_disk_open(device);
}

/*
 * XXX Good thing would be to have an argument, perhaps some device name.
 * XXX Other option is to make silo_disk_open() to return partitionable flag.
 * XXX Retrofit floppy ((flash == 0) && (floppy == 0) && (net == 0));
 */
int silo_disk_partitionable(void)
{
    return (flash == 0);
}
