/* prom - prom handling routines via /dev/openprom
   
   Copyright (C) 1996 Jakub Jelinek
   
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#ifdef __linux__
#  include <asm/openpromio.h>
#elif defined (__sun__)
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <sys/openpromio.h>
#  include <limits.h>
#else
#  error "Unknown system"
#endif
#include <fcntl.h>

static struct openpromio *oi;
static char buffer[2048 + sizeof (oi->oprom_size)];
static int prom_root_node = 0;
static int fd;
static int current_node;
static int previous_node;
static int action;
static int parent_node;

#define SETSIZE oi->oprom_size = 2048;

int prom_init (void)
{
    if ((fd = open ("/dev/openprom", O_RDONLY)) < 0 &&
	(fd = open ("/dev/misc/openprom", O_RDONLY)) < 0)
        return -1;
    oi = (struct openpromio *)buffer;
    SETSIZE
    *(int *)(oi->oprom_array) = 0;
    if (ioctl (fd, OPROMNEXT, (void *)oi) >= 0) {
    	prom_root_node = *(int *)(oi->oprom_array);
    	current_node = prom_root_node;
    	previous_node = 0;
    	parent_node = 0;
    	action = 0;
    	return 0;
    }
    return -1;
}

int prom_set_root_node (void)
{
    SETSIZE
    *(int *)(oi->oprom_array) = 0;
    if (ioctl (fd, OPROMNEXT, (void *)oi) >= 0) {
    	current_node = prom_root_node;
    	previous_node = 0;
    	parent_node = 0;
    	action = 0;
        return 0;
    }
    return -1;
}

int prom_search_siblings (char *name)
{
    int node = current_node;
    
    for (;;) {
	SETSIZE
	strcpy (oi->oprom_array, "name");
	if (ioctl (fd, OPROMGETPROP, (void *)oi) < 0)
	    return -1;
	if (!oi->oprom_size) return -1;
	if (!strcmp (oi->oprom_array, name))
	    return node;
    	SETSIZE
	*(int *)(oi->oprom_array) = node;
	if (ioctl (fd, OPROMNEXT, (void *)oi) < 0 || !(*(int *)(oi->oprom_array)))
	    return -1;
	if (!(*(int *)(oi->oprom_array)) || !oi->oprom_size) return -1;
	node = *(int *)(oi->oprom_array);
	previous_node = current_node;
	current_node = node;
	action = 0;
    }
}

int prom_next_sibling (void)
{
    SETSIZE
    *(int *)(oi->oprom_array) = current_node;
    if (ioctl (fd, OPROMNEXT, (void *)oi) < 0 || !(*(int *)(oi->oprom_array)))
	return -1;
    if (!(*(int *)(oi->oprom_array)) || !oi->oprom_size) return -1;
    previous_node = current_node;
    current_node = *(int *)(oi->oprom_array);
    action = 0;
    return 0;
}

int prom_getchild (void)
{
    SETSIZE
    *(int *)(oi->oprom_array) = current_node;
    if (ioctl (fd, OPROMCHILD, (void *)oi) >= 0) {
        if (!(*(int *)(oi->oprom_array)) || !oi->oprom_size) return -1;
        parent_node = current_node;
        current_node = *(int *)(oi->oprom_array);
        action = 1;
        previous_node = parent_node;
        return 0;
    }
    return -1;
}

char *prom_getstring (char *name)
{
    SETSIZE
    strcpy (oi->oprom_array, name);
    if (ioctl (fd, OPROMGETPROP, (void *)oi) >= 0) {
        if (!oi->oprom_size) return 0;
    	return oi->oprom_array;
    }
    return 0;
}

char *prom_getopt (char *name)
{
    SETSIZE
    strcpy (oi->oprom_array, name);
    if (ioctl (fd, OPROMGETOPT, (void *)oi) >= 0) {
    	if (!oi->oprom_size) return 0;
    	return oi->oprom_array;
    }
    return 0;
}

#ifdef __sun__
int prom_getversion()
{
    int i;
    char arch[10], *p;

    SETSIZE
    if (ioctl (fd, OPROMGETVERSION, (void *)oi) >= 0) {
    	p = strchr (oi->oprom_array, '.');
    	if (p && p > oi->oprom_array && p[-1] >= '0' && p[-1] <= '9' && 
    	    (p == oi->oprom_array + 1 || p[-2] < '0' || p[-2] > '9')) {
    	    switch (p[-1]) {
    	    	case '0':
    	    	case '1': return 0;
    	    	case '2':
    	    	case '3': return 2;
    	    }
    	}
    	printf ("Please report version string '%s' to jj@sunsite.mff.cuni.cz\n", oi->oprom_array);
    }
    /* Let's try brute force :)) */
    
    if (prom_set_root_node () < 0) return -1;
    *arch = 0;
    if ((p = prom_getstring ("compatability")) != 0)
        strcpy (arch, p);
    else if ((p = prom_getstring ("compatible")) != 0)
        strcpy (arch, p);
    if (*arch && (!strcmp (arch, "sun4m") || !strcmp (arch, "sun4d") || !strcmp (arch, "sun4e")))
    	return 2;
    else if (!*arch) {
        p = prom_getopt ("boot-from");
        if (p && (!strncmp (p, "sd(", 3) || 
    	    	  !strncmp (p, "le(", 3) || 
    	    	  !strncmp (p, "ie(", 3) ||
    	    	  !strncmp (p, "fd(", 3) ||
    	    	  !strncmp (p, "xd(", 3)))
    	    return 0;
    } else {
        if (prom_getchild () < 0) return -1;
    	if (prom_search_siblings ("aliases") != -1) {
    	    p = prom_getstring ("disk");
    	    if (!p) p = prom_getstring ("net");
    	    if (!p) p = prom_getstring ("disk0");
    	    if (p && *p == '/') {
    	    	if (!strncmp (p + 1, "iommu@", 6) || !strncmp (p + 1, "iommu/", 6) ||
    	    	    !strncmp (p + 1, "sbus@", 5) || !strncmp (p + 1, "sbus/", 5) ||
    	    	    !strncmp (p + 1, "espdma@", 7) || !strncmp (p + 1, "espdma/", 7) ||
    	    	    !strncmp (p + 1, "esp@", 4) || !strncmp (p + 1, "esp/", 4) ||
    	    	    !strncmp (p + 1, "io-unit@", 8) || !strncmp (p + 1, "io-unit/", 8))
    	    	    return 2;
    	    }
    	}
    }
    return -1;
}
#endif
