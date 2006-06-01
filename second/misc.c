/* Miscelaneous functions

   Copyright (C) 1996 David Miller
   1996 Pete A. Zaitcev
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
#include <asm/idprom.h>
#include <asm/machines.h>
#include <stringops.h>

void silo_fatal(const char *msg)
{
    printf("\nFatal error: %s\n", msg);
}

char *silo_v0_device(char *imagename)
{
    if (((imagename[0] == 's' && strchr ("dt", imagename[1])) ||
        (imagename[0] == 'x' && strchr ("dy", imagename[1])) ||
        (imagename[0] == 'f' && imagename[1] == 'd') ||
        (imagename[0] == 'l' && imagename[1] == 'e') ||
        (imagename[0] == 'i' && imagename[1] == 'e')) && 
        imagename[2] == '(') {
        return strchr (imagename, ')');
    } else
        return 0;
}

char *seed_part_into_device (char *device, int part)
{
	static char buffer[256];
	
	strcpy (buffer, device);
	if (prom_vers != PROM_V0) {
	    	char *p = strchr (buffer, ':');
	    	
	    	if (!p) {
	    	    p = strchr (buffer, 0);
	    	    *p++ = ':';
	    	} else
	    	    p++;
	    	*p++ = 'a' + part - 1;
	    	*p = 0;
	} else {
	        int i = strlen (device);
	        char *p;
	        
	        if (i >= 4 && buffer[2] == '(' && buffer[i - 1] == ')') {
	        	if (i == 4) {
	        	    strcpy (buffer + 3, "0,0,");
	        	    buffer [7] = '0' + part - 1;
	        	    strcpy (buffer + 8, ")");
	        	} else {
	        	    p = strchr (buffer + 3, ',');
	        	    if (!p) {
	        	    	strcpy (buffer + i - 1, ",0,");
	        	    	buffer [i + 2] = '0' + part - 1;
	        	    	strcpy (buffer + i + 3, ")");
	        	    } else {
	        	    	p = strchr (p + 1, ',');
	        	    	if (!p) {
	        	    	    buffer [i - 1] = ',';
	        	    	    buffer [i] = '0' + part - 1;
	        	    	    strcpy (buffer + i + 1, ")");
	        	    	} else {
	        	    	    *p = '0' - part - 1;
	        	    	    strcpy (p + 1, ")");
	        	    	}
	        	    }
	        	}
	        }
	}
	return buffer;
}

static char barg_buf[1024];
char barg_out[1024];

void silo_set_bootargs(char *params, char *device)
{
    char *q, *r;
    char **p;
    int iter, i;

    /*
     * We expect a kernel to extract a command line
     * from our dead body promptly, if the arguments are longer than 90 or so. 
     */
    if (params) {
        q = params;
        r = barg_out;
        /* Remove unnecessary spaces */
        do {
            while (*q == ' ') q++;
            if (!*q) break;
            if (r != barg_out) *r++ = ' ';
            while (*q && *q != ' ') *r++ = *q++;
        } while (*q);
        *r = 0;
    } else
        *barg_out = 0;
    switch (prom_vers) {
    case PROM_V0:
        if (strlen (barg_out) > 100 - 12) {
	    p = (*(romvec->pv_v0bootargs))->argv;
	    p [0] = "silo()";
	    q = barg_out;
	    for (iter = 1; iter < 7; iter++) {
	        while (*q == ' ') q++;
	        if (!*q) {
	            p [iter] = 0;
	            continue;
	        }
	        r = q;
	        while (*r && *r != ' ') r++;
	        if (!p[iter] || strlen (p[iter]) != r - q || strncmp (q, p[iter], r - q)) {
	    	    if (*r) *r++ = 0;
	            p [iter] = q;
	        }
	        q = r;
	    }
	    while (*q == ' ') q++;
	    if (*q)
	        p [7] = q;
	    else
	        p [7] = 0;
	    return;
	} else {
	    q = (*(romvec->pv_v0bootargs))->args;
	    p = (*(romvec->pv_v0bootargs))->argv;
	    p[0] = q;
	    if (!device) {
	    	strcpy (q, "silo()");
	    	q += 7;
	    	strcpy (q, barg_out);
	    } else {
	    	strcpy (q, device);
	    	q = strchr (q, 0);
	    	strcpy (q, barg_out);
	    	r = strchr (q, ' ');
	    	if (!r)
	    	    r = strchr (q, 0);
	    	else {
	    	    *r = 0;
	    	    r++;
	    	}
	    	(*(romvec->pv_v0bootargs))->kernel_file_name = q;
	    	(*(romvec->pv_v0bootargs))->boot_dev[0] = device[0];
	    	(*(romvec->pv_v0bootargs))->boot_dev[1] = device[1];
	    	q = device + 3;
	    	i = 0;	
	    	while (*q != ',' && *q != ')') {
	    	    i = i * 10 + *q - '0';
	    	    q++;
	    	}
	    	if (*q == ',') q++;
	    	(*(romvec->pv_v0bootargs))->boot_dev_ctrl = i;
	    	i = 0;	
	    	while (*q != ',' && *q != ')') {
	    	    i = i * 10 + *q - '0';
	    	    q++;
	    	}
	    	if (*q == ',') q++;
	    	(*(romvec->pv_v0bootargs))->boot_dev_unit = i;
	    	i = 0;	
	    	while (*q != ',' && *q != ')') {
	    	    i = i * 10 + *q - '0';
	    	    q++;
	    	}
	    	(*(romvec->pv_v0bootargs))->dev_partition = i;
	    	q = r;
	    }
	    for (i = 1; i < 7; i++) {
	    	if (*q) {
	    	    r = strchr (q, ' ');
	    	    if (r) {
	    	        *r = 0;
	    	        r++;
	    	    } else
	    	    	r = strchr (q, 0);
	    	    p[i] = q;
	    	    q = r;
	    	} else
	    	    p[i] = 0;
	    }
	    if (*q)
	        p[7] = q;
	    else
	        p[7] = 0;
	    return;
	}
    case PROM_V2:
    case PROM_V3:
    	if (strlen (barg_out) < 128)
    	    strcpy (*romvec->pv_v2bootargs.bootargs, barg_out);
    	else
            *romvec->pv_v2bootargs.bootargs = barg_out;
        if (device)
            strcpy (*romvec->pv_v2bootargs.bootpath, device);
        break;
    case PROM_P1275:
	break;
    }
}

char *silo_get_bootargs(int full)
{
    int iter;
    char *cp, *q;
    char **p;

    switch (prom_vers) {
    case PROM_V0:
        p = (*(romvec->pv_v0bootargs))->argv;
	cp = barg_buf;
	*cp = 0;
	if (p [0]) {
	    for (iter = 0; iter < 8; iter++) {
	    	q = p [iter];
	        if (q) {
	            if (!iter && !full) {
	                q = silo_v0_device(q);
	                
	                if (q && !q[1])
	                    continue;
	                else if (q)
	                    q++;
	                else
	                    q = p [iter];
	            }
	            strcpy (cp, q);
	            cp += strlen (cp);
	            *cp++ = ' ';
	        } else
	            break;
	    }
	    if (cp != barg_buf)
	        cp[-1] = 0;
	}
	break;
    case PROM_V2:
    case PROM_V3:
    	if (!full)
    	    q = barg_buf;
    	else {
            strcpy (barg_buf, *romvec->pv_v2bootargs.bootpath);
            q = strchr (barg_buf, 0);
        }
        if (*romvec->pv_v2bootargs.bootargs) {
            if (full)
                *q++ = ' ';
	    strcpy (q, *romvec->pv_v2bootargs.bootargs);
	} else if (!full)
	    *q = 0;
	break;
    case PROM_P1275:
    	if (!full)
    	    q = barg_buf;
    	else {
    	    iter = prom_getproperty (prom_chosen, "bootpath", barg_buf, 510);
    	    if (iter != -1)
    	        if (iter && !barg_buf [iter - 1])
    	            q = barg_buf + iter - 1;
    	        else
    	            q = barg_buf + iter;
    	    else
    	        q = barg_buf;
    	}
    	iter = prom_getproperty (prom_chosen, "bootargs", full ? q + 1 : q, 512);
    	if (iter == -1 || !iter)
    	    *q = 0;
    	else {
    	    if (full)
    	        *q = ' ';
    	    if (q [iter + 1])
    	    	q [iter + 1] = 0;
    	}
    	break;
    }
    return barg_buf;
}

void silo_show_bootargs(void)
{
    printf("Kernel args: %s\n", silo_get_bootargs(0));
}

unsigned char *silo_find_linux_HdrS(char *base, int len)
{
    /* Ugly magic to find HdrS, we dig into first jmp gokernel */
    char *p = base + ((*(unsigned short *) (base+2)) << 2) - 512;
    char *q;

    q = base+8;
    if (*q == 'H' && q[1] == 'd' && q[2] == 'r' && q[3] == 'S')
    	return q;
    if (p >= base + len || p <= base)
	return 0;
    for (q = p + 512; p < q; p += 4) {
	if (*p == 'H' && p[1] == 'd' && p[2] == 'r' && p[3] == 'S')
	    return p;
    }
    return 0;
}

static struct idp_struct idprm;

/* Here is the master table of Sun machines which use some implementation
 * of the Sparc CPU and have a meaningful IDPROM machtype value that we
 * know about.  See asm/machines.h for empirical constants.
 */
struct SMM {
    char *name;
    char *package;
    enum arch architecture;
    unsigned char id_machtype;
} Machines[NUM_SUN_MACHINES] = {
/* First, Sun4's */
{ "Sun 4/100 Series", "sun4", sun4, (SM_SUN4 | SM_4_110) },
{ "Sun 4/200 Series", "sun4", sun4, (SM_SUN4 | SM_4_260) },
{ "Sun 4/300 Series", "sun4", sun4, (SM_SUN4 | SM_4_330) },
{ "Sun 4/400 Series", "sun4", sun4, (SM_SUN4 | SM_4_470) },
/* Now, Sun4c's */
{ "SparcStation 1", 	"SUNW,Sun_4_60", sun4c, (SM_SUN4C | SM_4C_SS1) },
{ "SparcStation IPC", 	"SUNW,Sun_4_40", sun4c, (SM_SUN4C | SM_4C_IPC) },
{ "SparcStation 1+", 	"SUNW,Sun_4_65", sun4c, (SM_SUN4C | SM_4C_SS1PLUS) },
{ "SparcStation SLC", 	"SUNW,Sun_4_20", sun4c, (SM_SUN4C | SM_4C_SLC) },
{ "SparcStation 2", 	"SUNW,Sun_4_75", sun4c, (SM_SUN4C | SM_4C_SS2) },
{ "SparcStation ELC", 	"SUNW,Sun_4_25", sun4c, (SM_SUN4C | SM_4C_ELC) },
{ "SparcStation IPX", 	"SUNW,Sun_4_50", sun4c, (SM_SUN4C | SM_4C_IPX) },
/* Finally, early Sun4m's */
{ "SparcSystem 600", 	"SUNW,Sun_4_600", sun4m, (SM_SUN4M | SM_4M_SS60) },
{ "SparcStation 10/20", 	"sun4m", sun4m, (SM_SUN4M | SM_4M_SS50) },
{ "SparcStation 4/5", 	"sun4m", sun4m, (SM_SUN4M | SM_4M_SS40) },
/* One entry for the OBP arch's which are sun4d, sun4e, sun4u and newer sun4m's */
{ "OBP based system", 	"sun4m", sun4m, (SM_SUN4M_OBP | 0x0) } };

char *get_systype(void)
{
    static char system_name[128];
    int i;

    for(i = 0; i < NUM_SUN_MACHINES; i++)
	if(Machines[i].id_machtype == idprm.id_machtype) {
	    if(idprm.id_machtype!=(SM_SUN4M_OBP | 0x0))
		return Machines[i].name;
	    else {
		prom_getproperty(prom_root_node, "banner-name",
			         system_name, sizeof(system_name));
		return system_name;
	    }
	}
    return "Unknown Sparc";
}

char *get_syspackage(void)
{
    static char system_name[128];
    int i;

    *system_name = 0;
    prom_getproperty(prom_root_node, "name",
		     system_name, sizeof(system_name));
    if (*system_name)
	return system_name;
    for(i = 0; i < NUM_SUN_MACHINES; i++)
	if(Machines[i].id_machtype == idprm.id_machtype) {
	    return Machines[i].package;
	}
    return "sun4c";
}

void
get_idprom(void)
{
    prom_getproperty (prom_root_node, "idprom", (char *)&idprm, sizeof (idprm));
}

void print_message (char *msg)
{
    char *p = msg, *q = 0;
    int curly;
    int i;

    while (*p) {
        while (*p && *p != '$') p++;

	if (!*p)
		break;

	*p = 0;
	printf ("%s", msg);
	msg = p;
	*p++ = '$';
	if ((curly = (*p == '{')))
		p++;

	if (!strncmp (p, "ARCH", 4)) {
		switch (silo_get_architecture()) {
			case sun4: q = "SUN4"; break;
			case sun4c: q = "SUN4C"; break;
			case sun4d: q = "SUN4D"; break;
			case sun4m: q = "SUN4M"; break;
			case sun4e: q = "SUN4E"; break;
			case sun4p: q = "SUN4P"; break;
			case sun4u: q = "SUN4U"; break;
			default: break;
		}
		printf ("%s", q);
		p += 4;
	} else if (!strncmp (p, "PROM", 4)) {
		switch (prom_vers) {
			case PROM_V0: printf ("0"); break;
			case PROM_V2: printf ("2"); break;
			case PROM_V3: printf ("3"); break;
			case PROM_P1275: printf ("IEEE"); break;
		}
		p += 4;
	} else if (!strncmp (p, "ETHER", 5)) {
		for (i = 0; i < 6; i++)
			printf ("%x%x%s", ((unsigned char)idprm.id_eaddr[i]) >> 4, idprm.id_eaddr[i] & 0xf, i == 5 ? "" : ":");
		p += 5;
	} else if (!strncmp (p, "SYSTYPE", 7)) {
		printf ("%s", get_systype ());
		p += 7;
	} else
		continue;

	if (curly && *p == '}')
		p++;

	msg = p;
    }

    if (*msg)
	    printf ("%s", msg);
	
}

void silo_set_prollargs(char *params, unsigned int kbase, int ksize)
{
    struct silo_to_proll {
        char magic[4];          /* SiPR */
        int kern_base;          /* Always 256K so far. But who knows... */
        int kern_size;
        int xxx;
        char kern_args[256];
    } *p;
    int *v;

    /*
     * Old, bad way:
     * p = ((struct silo_to_proll *) 0x40000) - sizeof (struct silo_to_proll);
     *
     * New way allows to warn if PROLL is not cooperative.
     */
    v = (int *) 0x4000;
    if (v[2] != 0x5377) {
        printf ("PROLL does not accept arguments\n");
        return;
    }
    /* PROLL's base address is floating often. Mask the linking address off. */
    p = (struct silo_to_proll *)(v[3] & (0x40000-1));
    if (p->magic[0] != 'L' || p->magic[1] != 'R') {
	printf ("Bad magic in PROLL parameter");
	return;
    }
    p->magic[0] = 'S';
    p->magic[1] = 'i';
    p->magic[2] = 'L';
    p->magic[3] = 'o';
    p->kern_base = kbase;	/* Hopefuly >= 0x40000 */
    p->kern_size = ksize;
    p->xxx = 0;
    strcpy(p->kern_args, params);
}

enum arch silo_get_architecture(void)
{
    char *buffer = "sun4c    ";
    int i;

    if (prom_vers == PROM_P1275) {
	i = prom_getchild(prom_root_node);
        if ((i = prom_searchsiblings(i, "MicroSPARC-IIep")) != 0) {
            return sun4p;
        }
        return sun4u;
    }
    i = prom_getproperty (prom_root_node, "compatability", buffer, 8);

    if (!i || i == -1)
	i = prom_getproperty (prom_root_node, "compatible", buffer, 8);

    switch (buffer[4]) {
    case 'c':
	return sun4c;
    case 'm':
	return sun4m;
    case 'd':
	return sun4d;
    case 'e':
	return sun4e;
    case 'u':
    case 'v':
	return sun4u;
    default:
    	for(i = 0; i < NUM_SUN_MACHINES; i++)
	    if(Machines[i].id_machtype == idprm.id_machtype)
	    	return Machines[i].architecture;
	return sununknown;
    }
}
