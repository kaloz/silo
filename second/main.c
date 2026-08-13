/* Second stage boot loader
   
   Coyright (C) 1996 Pete A. Zaitcev
   		 1996 Maurizio Plaza
   		 1996 David S. Miller
   		 1996 Miguel de Icaza
   		 1996,1997,1998,1999 Jakub Jelinek
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

/* TODO: This file is a good candidate for rewrite from scratch.  */

#include <silo.h>
#include <elf.h>
#include <stringops.h>

#ifndef NULL
#define NULL (void *)0
#endif


struct HdrS_struct {
	char magic[4];
	unsigned int linux_ver;
	unsigned short ver;
	unsigned short root_flags;
	unsigned short root_dev;
	unsigned short ram_flags;
	unsigned int ramdisk_image;
	unsigned int ramdisk_size;
	/* 0x201 */
	unsigned int reboot_cmd_ptr_high;
	unsigned int reboot_cmd_ptr_low;
	/* 0x202 */
	unsigned int bootstr_info_ptr_high;
	unsigned int bootstr_info_ptr_low;
	/* 0x301 */
	unsigned long long ramdisk_image64;
};

/* This has to be first initialized variable in main.c */

extern char silo_conf[256];
extern unsigned char silo_conf_part, silo_conf_parts[32], raid_dsk_number;

int useconf = 0;
enum {
    CMD_BOOT,
    CMD_CAT,
    CMD_LS
} load_cmd;
enum arch architecture;
int sun4v_cpu;
static int timer_status = 0;
static char *initrd_start;
static int initrd_size;
static int initrd_defpart;
static char *initrd_defdevice;
static char *initrd_string;
static int initrd_prompt;
static int pause_after;
static char *pause_message = "Press ENTER to continue.";
static int show_arguments = 0;
static char given_bootargs [512];
static char my_bootargs [512];
static int given_bootargs_by_user = 0;
static char sol_params [512];
static int fill_reboot_cmd = 0;
static char other_device [512];
static int reboot = 0;
static int floppyswap = 0;
int tab_ambiguous = 0;
int other_part = -1;
int solaris = 0;
int flash = 0;
int other = 0;
char *password = 0;
int initrd_can_do_64bit_phys = 0;

static void parse_name (char *, int, char **, int *, char **);

static char *next_tok (char *t) {
    while (*t++); /* That was easy */
    return t;
}

/* Check for possible file and target completions. Return non-zero if
 * the caller needs to re-output the command line.  */
static int tab_complete(void) {
    int image_len, defpart, part, ret = 0;
    char *device;
    char *p = cfg_get_strg (0, "partition");
    char *kname, *r;

    if ((r = strrchr(cbuff, ' ')) == NULL)
	r = strdup(cbuff);
    else
	r = strdup(r + 1);

    if (p && *p >= '1' && *p <= '8' && !p[1])
	defpart = *p - '0';
    else {
	silo_fatal("\nDefault partition could not be found");
	free(r);
	return 1;
    }

    parse_name (r, defpart, &device, &part, &kname);
    if (!kname) {
	/* Maybe this isn't a disk file. Maybe it's a silo.conf defined
	 * alias/label.  */
	if (tab_ambiguous) {
	    ret = cfg_print_images (NULL, r);
	} else {
	    char *addr = (char *)0x4000;
	    int count = cfg_print_images (addr, r);
	    if (*addr) {
		int len = strlen(r);
		int len2 = strlen(cbuff);
		/* We have some completions... */
		if (count == 1) {
		    /* Just one, complete it... */
		    while (addr[len]) {
			cbuff[len2] = addr[len];
			cbuff[len2 + 1] = 0;
			prom_puts(cbuff + len2, 1);
			len++; len2++;
		    }
		    cbuff[len2] = ' ';
		    cbuff[len2 + 1] = 0;
		    prom_puts(cbuff + len2, 1);
		} else if (count > 1) { /* This should always be true, if we get here */
		    /* Complete the line as much as possible */
		    int common = 1, orig = len, i;
		    while (common && addr[len]) {
			char *cur = next_tok(addr);
			for (i = 1; i < count && common; i++, cur = next_tok(cur))
			    if (addr[len] != cur[len])
				common = 0;

			if (common) {
			    cbuff[len2] = addr[len];
			    cbuff[len2 + 1] = 0;
			    prom_puts(cbuff + len2, 1);
			    len++; len2++;
			}
		    }
		    if (orig == len)
			tab_ambiguous = 1;
		}
	    } else
		tab_ambiguous = 1;
	}
    } else {
	if (!device) device = cfg_get_strg (0, "device");

	if (silo_load_file(device, part, kname, (unsigned char *) 0x4000,
			   (unsigned char *) &_start, &image_len,
			   LOADFILE_LS_MATCH|LOADFILE_QUIET, 0))
	    if (do_ls((unsigned char *)0x4000, &tab_ambiguous))
		ret = 1;
    }

    free(r);
    return ret;
}

static void maintabfunc (void)
{
    if (cbuff[0] == 0) {
	/* Nothing on the command line, just list the possible images from
	 * the config file.  */
	if (cfg_print_images (NULL, NULL))
	    printf ("boot: %s", cbuff);
    } else {
	/* If tab_complete() returns non-zero, then it just listed
	 * possible completions, and we need to redo our command line.  */
	if (tab_complete())
	    printf ("boot: %s", cbuff);
    }
    return;
}

static void parse_name (char *imagename, int defpart, char **device,
			int *part, char **kname)
{
    static char parsebuf[1024];

    strcpy (parsebuf, imagename);
    imagename = parsebuf;
    *device = 0;
    *kname = 0;
    if (prom_vers == PROM_V0) {
        static char v0_buffer[20];
        *kname = silo_v0_device(imagename);
        if (*kname) {
            memcpy (v0_buffer, imagename, *kname - imagename + 1);
            v0_buffer [*kname - imagename + 1] = 0;
            (*kname)++;
            *device = v0_buffer;
        }  else
            *kname = imagename;
    } else {
    	*kname = strchr (imagename, ';');
    	if (!*kname)
	    *kname = imagename;
    	else {
	    **kname = 0;
	    (*kname)++;
	    *device = imagename;
        }
    }
    /* Range */
    if (**kname >= '0' && **kname <= '8' && (*kname)[1] == '[') {
	*part = **kname - '0';
	(*kname)++;
	return;
    }
    if (**kname == '[') {
	*part = 0;
	return;
    }
    /* Path */
    if (**kname >= '1' && **kname <= '8') {
    	if ((*kname)[1] == '/' || !(*kname)[1]) {
	    *part = **kname - '0';
	    if (!(*kname)[1]) {
	    	*kname = 0;
	    } else
	        (*kname)++;
	}
    } else if (**kname == '/') {
    	if (defpart != -2) {
	    if (defpart != -1)
	    	*part = defpart;
	    else
	    	*part = 1;
	}
    } else
	*kname = 0;
}

static void check_initrd (char *label, int defpart, char *defdevice)
{
    char *p;
    
    initrd_string = cfg_get_strg (label, "initrd");
    if (initrd_string) {
	initrd_defpart = defpart;
	initrd_defdevice = defdevice;
	initrd_size = 0;
	p = cfg_get_strg (label, "initrd-size");
	if (p) initrd_size = atoi (p);
	initrd_prompt = cfg_get_flag (label, "initrd-prompt");
    }
}

static int dig_into_params (char *params)
{
    char *p, *q;
    char *last_root = 0;
    int root_found = 0;

    p = params;    
    for (;;) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (!strncmp (p, "initrd", 6)) {
            root_found = 1;
    	    p += 6;
    	    if (*p == '=') {
    	        for (q = p++; *q && *q != ' '; q++);
    	        if (p != q) {
    	    	    initrd_string = malloc (q - p + 1);
    	    	    memcpy (initrd_string, p, q - p);
    	    	    initrd_string [q - p] = 0;
    	    	    memset (p - 7, ' ', q - p + 7);
    	        }
    	    } else if (!strncmp (p, "-prompt", 7)) {
    	        initrd_prompt = 1;
    	        memset (p - 6, ' ', 7 + 6);
    	    } else if (!strncmp (p, "-size=", 6)) {
    	        int i;
    	        p += 6;
    	        i = atoi (p);
    	        if (i > 0) {
    	            initrd_size = i;
    	            for (q = p; *q >= '0' && *q <= '9'; q++);
    	            memset (p - 12, ' ', q - p + 12);
    	        }
    	    }
        } else if (!strncmp (p, "pause-after", 11)) {
    	    pause_after = 1;
    	    memset (p, ' ', 11);
        } else if (!strncmp (p, "show-arguments", 14)) {
            show_arguments = 1;
            memset (p, ' ', 14);
        } else if (!strncmp (p, "root=", 5)) {
            root_found = 1;
            if (last_root) {
                while (*last_root != ' ') *last_root++ = ' ';
            }
            last_root = p;
        } else if (!strncmp (p, "nfsroot=", 8)) {
            root_found = 1;
        }
        while (*p && *p != ' ') p++;
    }
    return root_found;
}

static void check_password(char *str)
{
    int i;
    
    for (i = 0; i < 3; i++) {
	printf ("\n%sassword: ", str);
	passwdbuff[0] = 0;
	silo_cmdedit((void (*)(void)) 0, 1);
	printf ("\n");
	if (!strcmp (password, passwdbuff))
	    return;
	if (i < 2)
	    printf ("Wrong password. Please try again...");
    }
    printf ("Seems like you don't know the access password...\n");
    prom_halt();
}

static char *ls_options(char *parms)
{
    char *p = parms;
    ls_opt = 0;
    if (*parms == '-') {
	for (p = parms + 1;*p && *p != ' ';p++) {
	    switch (*p) {
	    case 'l': ls_opt |= LSOPT_L; break;
	    case 't': ls_opt |= LSOPT_T; break;
	    case 'r': ls_opt |= LSOPT_R; break;
	    default:
		printf ("Unhandled options to ls: `%s' - only l, t and r supported\n", parms);
	    	break;
	    }
	}
	while (*p == ' ') p++;
    }
    return p;
}

static int get_params (char **device, int *part, char **kname, char **proll,
		       char **params)
{
    int defpart = -1;
    char *defdevice = 0;
    char *p, *q;
    int c;
    char *imagename = 0, *label = 0;
    int timeout = -1, beg = 0, end;
    static char buffer[2048], buffer2[2048];
    static char prollb[256];	/* Hmm */
    static int first = 1;
    int from_prom = 0;
    int from_siloconf = 0;
    static int no_prom_args = 0;
    int tabbedout = 0;

#ifndef TFTP
    if (raid_dsk_number) {
	defpart = silo_conf_parts[raid_dsk_number - 1];
	if (defpart < 1 || defpart > 8)
	    defpart = -1;
    }
#endif
    load_cmd = CMD_BOOT;    
    other = 0;
    fill_reboot_cmd = 0;
    solaris = 0;
    initrd_string = 0;
    pause_after = 0;
    reboot = 0;
    *proll = 0;
    silo_cmdinit();
    *params = "";
    if (useconf) {
	defdevice = cfg_get_strg (0, "device");
	if (defpart == -1) {
	    /* For RAID1 avoid using this at all, it makes no sense
	       anyway */
	    p = cfg_get_strg (0, "partition");
	    if (p && *p >= '1' && *p <= '8' && !p[1])
		defpart = *p - '0';
	}
	if (first) {
	    first = 0;
	    p = cfg_get_strg (0, "timeout");
	    if (p && *p)
	        timeout = atoi (p);
	    if (no_prom_args) p = 0;
	    else p = silo_get_bootargs(0);
	    if (p) while (*p == ' ') p++;
	    if (p && *p) {
	        for (q = p; *q && *q != ' '; q++);
	        if (*q == ' ') {
	            *q++ = 0;
	            while (*q == ' ') q++;
	            if (*q) *params = q;
	        }
	        imagename = p;
	        from_prom = 1;
	        printf ("\n");
	    } else if (!timeout) {
	    	printf ("boot: ");
	        c = prom_nbgetchar ();
		if (c != -1 && c != '\n' && c != '\r') {
		    if (c == '\t') {
		        maintabfunc ();
		        tabbedout = 1;
		    } else if (c >= ' ') {
			tab_ambiguous = 0;
		        cbuff[0] = c;
		        cbuff[1] = 0;
		    }
	        } else {
		    imagename = cfg_get_default ();
		    printf ("\n");
		}
	    } else if (timeout != -1) {
		if (!timer_status) {
		    if (!init_timer ()) timer_status = 1;
		    else timer_status = -1;
		} else
		    timer_status++;
	        if (timer_status <= 0) {
	            printf ("\nYour timeout %d.%ds will never expire, since counter couldn't"
	            	    "\nbe initialized\nboot: ", timeout/10, timeout % 10);
	        } else {
                    printf ("boot: ");
	            reset_ticks ();
	            c = prom_nbgetchar ();
	            if (c == -1) {
	            	beg = get_ticks ();
		        end = beg + 10 * timeout;
	            	do {
		            c = prom_nbgetchar ();
		            if (c != -1)
		                break;
	                } while (get_ticks () <= end);
	            }
		    if (c != -1 && c != '\n' && c != '\r') {
		        if (c == '\t') {
		            maintabfunc ();
		            tabbedout = 1;
		        } else if (c >= ' ') {
			    tab_ambiguous = 0;
		            cbuff[0] = c;
		            cbuff[1] = 0;
			    if (cfg_get_flag (cbuff, "single-key"))
				imagename = cbuff;
		        }
	            } else {
		        imagename = cfg_get_default ();
		        printf ("\n");
		    }
		    if (timer_status >= 1) {
		        timer_status--;
		        close_timer ();
		    }
	        }
	    }
	}
	if (!imagename) {
	    if ((!*cbuff || (timeout > 0 && timer_status < 0)) && !tabbedout)
                printf ("boot: ");
	    silo_cmdedit(maintabfunc, 0);
	    if (*cbuff == ' ') {
		for (p = cbuff; *p == ' '; p++);
		q = cbuff;
		while ((*q++ = *p++) != 0);
	    }
	    strcpy (given_bootargs, cbuff);
	    given_bootargs_by_user = 1;
	    printf ("\n");
	    if (!*cbuff)
		imagename = cfg_get_default ();
	    else {
		p = strchr (cbuff, ' ');
		if (!p)
		    *params = "";
		else {
		    *p++ = 0;
		    while (*p && *p <= ' ')
			p++;
		    *params = p;
		}
		imagename = cbuff;
	    }
	}
	p = cfg_get_strg (imagename, "image");
	if (p && *p) {
	    label = imagename;
	    imagename = p;
	} else
	    label = 0;
	p = cfg_get_strg (0, "pause-message");
	if (p) pause_message = p;
	if (label) {
	    if (**params && password)
		check_password ("To specify image arguments you need to enter your p");
	    from_siloconf = 1;
	    defdevice = cfg_get_strg (label, "device");
#ifndef TFTP
	    if (!raid_dsk_number)
#endif
	    {
		p = cfg_get_strg (label, "partition");
		if (p && *p >= '1' && *p <= '8' && !p[1])
		    defpart = *p - '0';
	    }
	    *buffer = 0;
	    q = buffer;
	    if (cfg_get_flag (label, "fill-reboot-cmd"))
	        fill_reboot_cmd = 1;
	    if (cfg_get_strg (label, "other"))
	    	other = 1;
	    else if (cfg_get_flag (label, "solaris"))
	    	solaris = 1;
	    else if (cfg_get_flag (label, "flash"))
	    	flash = 1;
	    p = cfg_get_strg (label, "literal");
	    if (!p && other)
	    	p = cfg_get_strg (label, "append");
	    if (p) {
		strcpy (q, p);
		q = strchr (q, 0);
		if (**params) {
		    if (*p)
		        *q++ = ' ';
		    strcpy (q, *params);
		}
	    } else if (!solaris && !other) {
		p = cfg_get_strg (label, "root");
		if (p) {
		    strcpy (q, "root=");
		    strcpy (q + 5, p);
		    q = strchr (q, 0);
		    *q++ = ' ';
		}
		if (cfg_get_flag (label, "read-only")) {
		    strcpy (q, "ro ");
		    q += 3;
		}
		if (cfg_get_flag (label, "read-write")) {
		    strcpy (q, "rw ");
		    q += 3;
		}
		p = cfg_get_strg (label, "ramdisk");
		if (p) {
		    strcpy (q, "ramdisk=");
		    strcpy (q + 8, p);
		    q = strchr (q, 0);
		    *q++ = ' ';
		}
		p = cfg_get_strg (label, "append");
		if (p) {
		    strcpy (q, p);
		    q = strchr (q, 0);
		    *q++ = ' ';
		}
		check_initrd (label, defpart, defdevice);
		if (**params)
			strcpy (q, *params);
		p = cfg_get_strg (label, "proll");
		if (p && strlen (p) < sizeof(prollb)-1) {
		    strcpy (prollb, p);
		    *proll = prollb;
		}
	    }
	    if (other) {
	        char *oth_device = 0;
	        char *oth_kname = 0;
	        
	        other_part = -1;
    		parse_name (imagename, -1, &oth_device, &other_part, &oth_kname);
    		if (other_part == -1 || oth_kname) {
    		    printf ("Wrong syntax for other= parameter. The parameter should be\n");
    		    if (prom_vers == PROM_V0)
    		        printf ("either a single number 1-8 (ie. partition of the current disk)\n"
    		                "or sd(X,Y,Z)N (ie. some other disk plus partition number)\n"
    		                "e.g. sd(0,3,2)4\n");
    		    else
    		        printf ("either a single number 1-8 (ie. partition of the current disk)\n"
    		                "or /prom/path/name;N (ie. some other disk plus partition number)\n"
    		                "e.g. /iommu/sbus/espdma/esp/sd@3,0;3 \n");
    		    *kname = 0;
    		    return 0;
    		}
    		if (!oth_device) {
    		    if (defdevice)
    		        oth_device = defdevice;
    		    else
    		        oth_device = silo_disk_get_bootdevice();
    		}
    		strcpy (other_device, oth_device);
	    	p = cfg_get_strg (label, "bootblock");
	    	if (p)
	    	    imagename = p;
	    	else {
	    	    static char bufx[8];
	    	    
	    	    if (architecture == sun4u)
	    	    	reboot = 1;
	    	    strcpy (bufx, "x[1-16]");
	    	    *bufx = other_part + '0';
	    	    defdevice = other_device;
	    	    defpart = other_part;
	    	}
	    }
	    pause_after = cfg_get_flag (label, "pause-after");
	    p = cfg_get_strg (label, "pause-message");
	    if (p) pause_message = p;
	    *params = buffer;
	}
    } else {
        if (first) {
            first = 0;
	    if (no_prom_args) p = 0;
	    else p = silo_get_bootargs(0);
	    if (p) while (*p == ' ') p++;
	    if (p && *p) {
	        for (q = p; *q && *q != ' '; q++);
	        if (*q == ' ') {
	            *q++ = 0;
	            while (*q == ' ') q++;
	            if (*q) *params = q;
	        }
	        imagename = p;
	        from_prom = 1;
	        printf ("\n");
	    }
	}
	if (!imagename) {
            printf ("boot: ");
	    silo_cmdedit((void (*)(void)) 0, 0);
	    if (*cbuff == ' ') {
		for (p = cbuff; *p == ' '; p++);
		q = cbuff;
		while ((*q++ = *p++) != 0);
	    }
	    strcpy (given_bootargs, cbuff);
	    given_bootargs_by_user = 1;
	    printf ("\n");
	    p = strchr (cbuff, ' ');
	    if (!p)
	        *params = "";
	    else {
	        *p = 0;
	        p++;
	        while (*p && *p <= ' ')
		    p++;
	        *params = p;
	    }
	    imagename = cbuff;
	}
    }
    if (!strcmp (imagename, "halt"))
	return 1;
    if (!label && password)
    	check_password ("To boot a custom image you need to enter your p");
    if (!strcmp (imagename, "xxdebug"))
	return 2;
    if (!strcmp (imagename, "help")) {
        if (prom_vers == PROM_V0)
            printf ("You have to type image name as {XY(...)}partno/path or {XY(...)}partno[mm-nn],\n"
                    "where XY(...) is the optional v0 prom device name (if partition number is specified,\n"
                    "it should be Sun partition number (zero based) of any partition starting at cyl. 0)\n"
                    "({} means that it is optional) and partno is a Linux partition number from 1 to 8.\n"
                    "You can specify a path into filesystem (ext2fs, ufs) - has to start with / - or\n"
                    "[mm-nn] as range of phys. blocks (512B).\n");
	else
	    printf ("You have to type image name as [prom_path;]partno/path, where partno is a\n"
	    	    "number from 1 to 8. If partno is not specified, either default from silo.conf\n"
	    	    "(partition=X) or 1 will be used. Instead of /path you can type [mm-nn] to\n"
	    	    "specify a range of phys. blocks (512B)\n");
	printf ("If you use silo.conf and have some image= sections there, you can type\n"
                "its label or alias name instead of the above described image names.\n"
                "Pressing just enter will load default image with default arguments. Special\n"
                "image names `halt' and `help' can be used to fall back to PROM or display\n"
                "help. All three types of image names can be followed by additional arguments.\n"
                "Examples:\n");
        if (prom_vers == PROM_V0)
            printf ("  /boot/vmlinux.gz root=/dev/sda4\n"
                    "  2/boot/mykernel.gz root=/dev/sda2\n"
                    "  sd(0,6,2)5/boot/old.b\n"
                    "  sd(1,2,0)[1-16] root=/dev/sda4\n");
        else
            printf ("  /iommu/sbus/espdma/esp/sd@3,0;4/boot/vmlinux.gz root=/dev/sda4\n"
                    "  1/boot/old.b\n"
                    "  /sbus/espdma/esp/sd@0,0;3[1-16] root=/dev/sda4\n");
        printf ("  linux root=/dev/sda4\n"
                "  live\n");
    	*kname = 0;
        return 0;
    }
    strcpy (buffer2, imagename);
    *part = -2;
    if (!label && (!strcmp (imagename, "cat") || !strcmp (imagename, "ls"))) {
	if (*imagename == 'c')
	    load_cmd = CMD_CAT;
	else {
	    load_cmd = CMD_LS;
	    *params = ls_options (*params);
	}
	imagename = *params;
	p = strchr (imagename, ' ');
	if (!p)
	    *params = "";
	else {
	    *p++ = 0;
	    while (*p && *p <= ' ')
		p++;
	    *params = p;
	}
    } else if (label && (!strncmp (imagename, "cat ", 4) || !strncmp (imagename, "ls ", 3))) {
	if (*imagename == 'c') {
	    load_cmd = CMD_CAT;
	    imagename += 4;
	    while (*imagename == ' ') imagename++;
	} else {
	    load_cmd = CMD_LS;
	    imagename += 3;
	    while (*imagename == ' ') imagename++;
	    imagename = ls_options (imagename);
	}
    }
    parse_name (imagename, -2, device, part, kname);
    if (!*device)
	*device = defdevice;
    if (*kname && *part == -2) {
        if (defpart != -1)
            *part = defpart;
        else
            *part = 1;
    }
    if (!*kname) {
    	if (*part != -2) {
	    other_part = *part;
	    if (!*device)
	    	strcpy (other_device, silo_disk_get_bootdevice());
	    else
	    	strcpy (other_device, *device);
	    p = strstr (*params, "bootblock=");
	    if (p && (p == *params || p[-1] == ' ') && p[10] && p[10] != ' ') {
	        char tmp[512], *q;
	        
	        q = tmp; 
	        p += 10;
	        while (*p && *p != ' ')
	            *q++ = *p++;
	        *q = 0;
	        parse_name (tmp, defpart, device, part, kname);
	        if (*kname) {
	            p = strstr (*params, "bootblock=");
	            memset (p, ' ', q - tmp + 10);
	            other = 1;
	            return 0;
	        } else {
	            printf ("Syntax of your bootblock= parameter is wrong. Please see help\n");
	        }
	    } else {
	    	other = 1;
	    	*kname = strdup ("[1-16]");
	    	if (architecture == sun4u)
	    	    reboot = 1;
	    	return 0;
	    }
    	} else {
            if (defpart != -1)
                *part = defpart;
            else
                *part = 1;
    	}
        if (from_prom) {
            int options_node, len;
            int is_from_prom = 0;
            char *v = "";
            char buffer3[2048];
            
    	    if ((options_node = prom_searchsiblings (prom_getchild (prom_root_node), "options")) != 0) {
    	    	if (prom_vers != PROM_V0) {
		    prom_getstring (options_node, v = "boot-file", buffer3, 2048);
		    len = prom_getproplen (options_node, v);
		    if (len < 0) len = 0;
		    buffer3[len] = 0;
		    if (!strcmp (buffer3, my_bootargs))
		        is_from_prom = 1;
		    else {
		        prom_getstring (options_node, v = "diag-file", buffer3, 2048);
		        len = prom_getproplen (options_node, v);
		        if (len < 0) len = 0;
		        buffer3[len] = 0;
		        if (!strcmp (buffer3, my_bootargs))
		            is_from_prom = 1;
		    }
		} else {
		    prom_getstring (options_node, v = "boot-from", buffer3, 2048);
		    len = prom_getproplen (options_node, v);
		    if (len < 0) len = 0;
		    buffer3[len] = 0;
		    if (!strcmp (buffer3, my_bootargs))
		    	is_from_prom = 1;
		}
	    }
	    if (is_from_prom) {
	    	first = 1;
	    	no_prom_args = 1;
	    	*given_bootargs = 0;
	    	given_bootargs_by_user = 1;
	    	printf ("You have `%s' string in your %s variable.\n"
	    		"This string doesn't contain valid arguments to SILO.\n"
	    		"Consider doing setenv %s. Anyway, SILO will continue as\n"
	    		"if there were no arguments in %s.\n", buffer3, v, v, v);
	    	return 0;
	    }
        }
    	printf ("Your imagename `%s' and arguments `%s' have either wrong syntax,\n"
    	        "or describe a label which is not present in silo.conf\n"
    	        "Type `help' at the boot: prompt if you need it and then try again.\n", buffer2, *params ? *params : "");
    } else if (!solaris) {
    	p = strstr (*params, "solaris");
    	if (p && (p == *params || p[-1] == ' ') && (!p[7] || p[7] == ' ')) {
    	    memset (p, ' ', 7);
    	    solaris = 1;
    	} else if (!strcmp (*kname, "/kernel/unix"))
    	    solaris = 1;
    	if (!fill_reboot_cmd) {
    	    p = strstr (*params, "fill-reboot-cmd");
    	    if (p && (p == *params || p[-1] == ' ') && (!p[15] || p[15] == ' ')) {
    	    	memset (p, ' ', 15);
    	    	fill_reboot_cmd = 1;
    	    }
    	}
    }
    if (!flash) {
    	p = strstr (*params, "flash");
	if (p)
	    flash = 1;
    }
    return 0;
}

static void initrd_lenfunc (int len, char **filebuffer, char **filelimit)
{
    extern unsigned long long initrd_phys;

    initrd_start = memory_find ((len + 16383) & ~16383);
    if (!initrd_start) {
        silo_fatal("You do not have enough continuous available memory "
		   "for such initial ramdisk.");
        prom_halt ();
    }
    initrd_size = len;
    *filebuffer = initrd_start;
    *filelimit = initrd_start + ((len + 16383) & ~16383);
    printf("Loading initial ramdisk (%d bytes at 0x%Lx phys, 0x%x virt)...\n", len,
	   initrd_phys, initrd_start);
}

static int parse_executable (unsigned char *base, int image_len, unsigned int *poff,
			     int *plen, unsigned *pstart, char *image_name)
{
    int isfile = 0;
    union {
	char *b;
	struct aout_hdr *a;
	Elf32_Ehdr *e;
	Elf64_Ehdr *f;
    } hp;
    unsigned off = 0;
    int len = 0;
    unsigned st = (unsigned int) base;

    /* By this point the first sector is loaded (and the rest of */
    /* the kernel) so we check if it is an executable file, either */
    /* an a.out or an elf binary */

    hp.b = (char *)base;
    if (hp.a->magic == 0x01030107) {
    	if (solaris) {
    	    printf ("\nYour Solaris `ufsboot' is not an ELF image. Try again.\n");
	    return -1;
    	}
	off = sizeof (struct aout_hdr);
	if (image_len > hp.a->ltext + hp.a->ldata)
	    len = hp.a->ltext + hp.a->ldata;
	else
	    len = image_len;
	isfile = 1;
    } else if (hp.e->e_ident[EI_MAG0] == ELFMAG0 &&
		hp.e->e_ident[EI_MAG1] == ELFMAG1 &&
		hp.e->e_ident[EI_MAG2] == ELFMAG2 &&
		hp.e->e_ident[EI_MAG3] == ELFMAG3) {

        if (hp.e->e_ident[EI_DATA] != ELFDATA2MSB) {
	    silo_fatal("Image is not a MSB ELF");
	    prom_halt ();
	}
	if (hp.e->e_ident[EI_CLASS] == ELFCLASS32) {
	    Elf32_Phdr *p;
printf("SILO: ELF32 entry=0x%x phoff=0x%x phnum=0x%x\n",
       hp.e->e_entry, hp.e->e_phoff, hp.e->e_phnum);

{
	int di;
	Elf32_Phdr *dp = (Elf32_Phdr *)(hp.b + hp.e->e_phoff);

	for (di = 0; di < hp.e->e_phnum; di++, dp++) {
		printf("SILO: PHDR %d type=0x%x off=0x%x vaddr=0x%x "
		       "paddr=0x%x filesz=0x%x memsz=0x%x flags=0x%x\n",
		       di,
		       dp->p_type,
		       dp->p_offset,
		       dp->p_vaddr,
		       dp->p_paddr,
		       dp->p_filesz,
		       dp->p_memsz,
		       dp->p_flags);
	}
}
	    p = (Elf32_Phdr *) (hp.b + hp.e->e_phoff);
	    if (p->p_type != PT_LOAD) {
		silo_fatal("Cannot find a loadable segment in your "
			   "ELF image");
		prom_halt ();
	    }
	    if (flash) {
	        int i;

		if (architecture != sun4u) {
		    silo_fatal("Flash boots only supported on sun4u "
			       "and later");
		    prom_halt ();
		}
	        for (i = 0; i < hp.e->e_phnum; i++, p++) {
	            printf("SILO: Loading flash segment %d at vaddr[0x%x] len[0x%x]\n",
			   i, p->p_vaddr, p->p_filesz);
	            memcpy ((char *)p->p_vaddr,
			    (base + p->p_offset), p->p_filesz);
	            if (p->p_filesz < p->p_memsz)
	                memset ((char *)(p->p_vaddr + p->p_filesz), 0,
				p->p_memsz - p->p_filesz);
	        }
	        isfile = 1;
	        st = hp.e->e_entry;
		printf("SILO: Flash image entry is at vaddr[0x%x]\n", st);
	    } else if (solaris) {
	        int i;
	        unsigned long sa = (unsigned long)&_start;

	        for (i = 0; i < hp.e->e_phnum; i++, p++) {
	            if (p->p_vaddr < 0x4000 + image_len ||
				p->p_vaddr + p->p_memsz >= sa) {
	        	silo_fatal("Unable to handle your "
				   "Solaris `ufsboot' bootloader.");
	            	prom_halt ();
	            }
	            memcpy ((char *)p->p_vaddr,
				(char *)(0x4000 + p->p_offset), p->p_filesz);
	            if (p->p_filesz < p->p_memsz)
	                memset ((char *)(p->p_vaddr + p->p_filesz), 0,
				p->p_memsz - p->p_filesz);
	        }
	        isfile = 1;
	        st = hp.e->e_entry;
	    } else {
	            int i;
	            unsigned long n;

	            Elf32_Phdr *q = p + 1;
		            
	            for (i = 1; i < hp.e->e_phnum; i++, q++) {
	                if (q->p_type != PT_LOAD)
	                    break;
	                n = q->p_offset - p->p_offset;
			if (q->p_vaddr - p->p_vaddr == n &&
	                    q->p_paddr - p->p_paddr == n &&
			    p->p_memsz == p->p_filesz &&
	                    p->p_memsz <= n) {
				p->p_filesz = n + q->p_filesz;
				p->p_memsz = n + q->p_memsz;
	                } else {
	                    silo_fatal("Multiple loadable segments in "
				       "your ELF image");
	                    prom_halt();
	                }
	            }
	    	    off = p->p_offset + hp.e->e_entry - p->p_vaddr;
		    printf("SILO: ELF32 computed off=0x%x len=0x%x entry=0x%x base=0x%x\n",
       			off, len, hp.e->e_entry, base);
	    	    len = p->p_filesz;
	    	    if (len > image_len) len = image_len;
	    	    isfile = 1;
	    }
	} else if (hp.e->e_ident[EI_CLASS] == ELFCLASS64) {
	    Elf64_Phdr *p;
	    unsigned long long n;
	    int i;
	    Elf64_Phdr *q;

	    p = (Elf64_Phdr *) (hp.b + hp.f->e_phoff);
	    if (p->p_type != PT_LOAD) {
		silo_fatal("Cannot find a loadable segment in your "
			   "ELF image");
		prom_halt ();
	    }
	    if (flash) {
		int i;

		if (architecture != sun4u) {
		    silo_fatal("Flash boots only supported on sun4u "
			       "and later");
		    prom_halt ();
		}
		for (i = 0; i < hp.f->e_phnum; i++, p++) {
	            printf("SILO: Loading flash segment %d at vaddr[0x%x] len[0x%x]\n",
			   i, (unsigned int) p->p_vaddr, (unsigned int) p->p_filesz);
		    memcpy ((Elf64_Addr *)(long)(p->p_vaddr),
			    (Elf64_Addr *)(long)(base + p->p_offset),
			    p->p_filesz);
		    if (p->p_filesz < p->p_memsz)
			memset ((Elf64_Addr *)(long)(p->p_vaddr + p->p_filesz),
				0, p->p_memsz - p->p_filesz);
		}
		isfile = 1;
		st = hp.f->e_entry;
		printf("SILO: Flash image entry is at vaddr[0x%x]\n", st);
	    } else if (solaris) {
		int i;
		unsigned long sa = (unsigned long)&_start;

		for (i = 0; i < hp.f->e_phnum; i++, p++) {
		    if (p->p_vaddr < 0x4000 + image_len ||
			 p->p_vaddr + p->p_memsz >= sa) {

			 silo_fatal("Unable to handle your "
				    "Solaris `ufsboot' bootloader.");
			 prom_halt ();
 		    }
		    memcpy ((Elf64_Addr *)(long)(p->p_vaddr),
				(Elf64_Addr *)(long)(0x4000 + p->p_offset),
				p->p_filesz);
		    if (p->p_filesz < p->p_memsz)
			memset ((Elf64_Addr *)(long)(p->p_vaddr + p->p_filesz),
				0, p->p_memsz - p->p_filesz);
		}
		isfile = 1;
		st = hp.f->e_entry;
	    } else {
		    q = p + 1;
		    for (i = 1; i < hp.f->e_phnum; i++, q++) {
			if (q->p_type != PT_LOAD)
			    break;
			n = q->p_offset - p->p_offset;
			if (q->p_vaddr - p->p_vaddr == n &&
			    q->p_paddr - p->p_paddr == n &&
			    p->p_memsz == p->p_filesz &&
			    p->p_memsz <= n) {

			    p->p_filesz = n + q->p_filesz;
			    p->p_memsz = n + q->p_memsz;
			} else {
			    silo_fatal("Multiple loadable segments "
				       "in your ELF image");
			    prom_halt();
			}
		    }
	    }
	    off = p->p_offset + hp.f->e_entry - p->p_vaddr;
	    len = p->p_filesz;
	    if (len > image_len) len = image_len;
	    isfile = 1;
	}
    }

    if (isfile) {
	if (pstart != NULL)
	    *pstart = st;
	*poff = off;
	*plen = len;
    } else {
	printf ("\nUnknown image %s format\n", image_name);
    }
    return isfile;
}

/* Here we are launched */
int bootmain (void)
{
    unsigned off = 0;
    int len = 0, image_len;
    char *kname, *params, *device;
    char *proll = 0;
    unsigned char *image_base = (unsigned char *) 0x4000;
    char *kernel_params;
    int part;
    int isfile, fileok = 0;
    unsigned int ret_offset = 0;
    char *params_device = 0;
    int silo_conf_partition;
    unsigned long elf_map_va = 0;
    unsigned long elf_map_len = 0;



    prom_ranges_init ();
    get_idprom();
    architecture = silo_get_architecture();
    strcpy (given_bootargs, silo_get_bootargs(1));
    strcpy (my_bootargs, silo_get_bootargs(0));

#ifndef TFTP
    if (silo_diskinit() == -1)
	prom_halt ();
#endif
	
    if (architecture == sun4u) {
    	char buffer[512], *p;
    	int node = prom_finddevice("/chosen");
    	if (node) {
    		prom_getstring(node, "bootpath", buffer, 512);
    		if (strstr(buffer, "fdthree")) {
    			p = strchr (buffer, ':');
    			if (p) *p = 0;
    			node = prom_finddevice(buffer);
    			if (node) {
    				if (prom_getintdefault (node, "unit", 0) == 1)
    					floppyswap = 1;
    			}
    		}
    	}
    }

/* Here should be code #ifdef TFTP, that will handle loading of silo.conf via tftp and this code should be on the other side #ifndef TFTP */
    silo_conf_partition = silo_conf_part;
#ifndef TFTP
    if (raid_dsk_number)
	silo_conf_partition = silo_conf_parts[raid_dsk_number - 1];
#endif
    if (*silo_conf && silo_conf_partition >= 1 && silo_conf_partition <= 8) {
	int len;
	solaris = 0;
	fileok = silo_load_file(0, silo_conf_partition, silo_conf,
				(unsigned char *) 0x4000,
				(unsigned char *) &_start,
				&len, LOADFILE_GZIP | LOADFILE_NO_ROTATE, 0);
	if (!fileok || (unsigned) len >= 65535)
	    printf ("\nCouldn't load %s\n", silo_conf);
	else {
	    if (!cfg_parse (silo_conf, (char *) 0x4000, len)) {
		char *p, *q;
		int len = 0;
		int defpart = -1;
#ifndef TFTP
		if (raid_dsk_number)
		    defpart = silo_conf_partition;
#endif		
		p = cfg_get_strg (0, "message");
		if (p) {
		    q = cfg_get_strg (0, "partition");
		    if (q && *q >= '1' && *q <= '8' && !q[1])
			defpart = *q - '0';
		    parse_name (p, defpart, &device, &part, &kname);
		    if (kname) {
			if (!device)
			    device = cfg_get_strg (0, "device");
			solaris = 0;
			if (silo_load_file(device, part, kname,
					   (unsigned char *) 0x4000,
					   (unsigned char *) &_start, &len,
					   LOADFILE_GZIP, 0)) {
			    *(unsigned char *) (0x4000 + len) = 0;
			    printf ("\n");
			    print_message ((char *) 0x4000);
			}
		    }
		}
		useconf = 1;
		password = cfg_get_strg (0, "password");
		if (password && !cfg_get_flag (0, "restricted")) {
		    check_password ("P");
		    password = 0;
		}
	    } else {
	        printf ("Syntax error in %s\n"
			"Please check the file for obvious bugs, and if you think it is correct, get\n"
	                "latest version of SILO. If problems still survive, contact the author\n", silo_conf);
	    }
	}
    } else
	printf ("\n");
    if (!useconf)
	printf ("No config file loaded, you can boot just from this command line\n"
	"Type [prompath;]part/path_to_image [parameters] on the prompt\n"
	  "E.g. /iommu/sbus/espdma/esp/sd@3,0;4/vmlinux root=/dev/sda4\n"
		"or 2/vmlinux.live (to load vmlinux.live from 2nd partition of boot disk)\n");
try_again:
    isfile = 0;			/* RC = 0 invalid file or not an executable */
    while (!isfile) {
	switch (get_params (&device, &part, &kname, &proll, &params)) {
	case 1:
	    prom_halt ();
	case 2:
	    prom_cmdline ();
	    continue;
	}
	if (!kname)
	    continue;
	    
	if (other && reboot)
	    break;

	if (solaris) {
	    char *p = seed_part_into_device ((!device || !*device) ? silo_disk_get_bootdevice() : device, part);
	    strcpy (sol_params, p);
	    params_device = strchr (sol_params, 0) + 1;
	    strcpy (params_device, kname);
	    if (params && *params) {
	        strcat (params_device, " ");
	        strcat (params_device, params);
	    }
	}

	if (load_cmd == CMD_BOOT && proll != 0) {
	    if (other || solaris) {
		printf ("\nNeither \"other\" nor \"solaris\" are compatible with proll\n");
		continue;
	    }
	    if (!silo_load_file(device, part, proll, (unsigned char *) 0x4000,
				(unsigned char *) 0x40000, &image_len,
				LOADFILE_GZIP, 0)) {
	        printf ("\nProll not found.... try again\n");
		continue;
	    }
            if (!parse_executable ((unsigned char *)0x4000, image_len, &off,
				    &len, NULL, proll))
		continue;
            memcpy ((char *) 0x4000, ((char *) 0x4000) + off, len);

	    image_base = (unsigned char *) 0x40000;
	    if (!silo_load_file(device, part, kname, image_base,
				(unsigned char *) &_start, &image_len,
				LOADFILE_GZIP, 0)) {
	        printf ("\nImage not found.... try again\n");
		continue;
	    }
            if (!(isfile = parse_executable (image_base, image_len,
			&off, &len, NULL, kname)))
		continue;
	    ret_offset = 0x4000;

	} else {
	    unsigned char *image_end = (unsigned char *)&_start;

	    /* See if we can use some extra memory for the kernel */
	    if (!load_cmd) {
		unsigned int size;
		unsigned char *mem;

		/* As of 2.6.25-rc6, an "allyesconfig" kernel is around
		 * ~42MB in size.  So we try to carve out up to 64MB of
		 * memory for the kernel.
		 */

		printf("SILO: arch=%d _start=%x initial image_base=%x\n",
			architecture, &_start, image_base);

		for (size = 16 * 1024 * 1024;
		     size >= 4 * 1024 * 1024;
		     size -= 4 * 1024 * 1024) {
			mem = (unsigned char *)image_memory_find(size);
			printf("SILO: size bytes 0x%x\n", size);
			printf("SILO: size mb 0x%x\n", size >> 20);
			printf("SILO: mem 0x%x\n", mem);
			if (mem)
				break;
		}

		if (mem) {
		    image_base = mem;
		    image_end = image_base + size - 0x4000;
		    printf("Allocated %d Megs of memory at 0x%x for kernel\n",
			   size >> 20, image_base - 0x4000);
		}
	    }

	    if (!silo_load_file(device, part, kname, image_base, image_end,
				&image_len, ((load_cmd == CMD_LS) ?
					     LOADFILE_LS :
					     LOADFILE_GZIP), 0)) {
		printf ("\nImage not found.... try again\n");

		if (!load_cmd)
		    image_memory_release();

        	continue;
            }

	    if (load_cmd == CMD_CAT) {
		*(image_base + image_len) = 0;
		printf ("%s", image_base);
		continue;
	    }

	    if (load_cmd == CMD_LS) {
		do_ls (image_base, NULL);
        	continue;
	    }

	    isfile = parse_executable (image_base, image_len, &off, &len,
				       &ret_offset, kname);

	}
    }

    kernel_params = 0;
    if (solaris) {
    	params = params_device;
    	params_device = sol_params;
    } else if (!other && !flash) {
	struct HdrS_struct *hdrs;

    	params_device = 0;
	printf("SILO: parse result image_base=0x%x image_len=0x%x off=0x%x len=0x%x ret_offset=0x%x\n",
       		image_base, image_len, off, len, ret_offset);

	if (architecture == sun4m &&
    image_base != (unsigned char *)0x4000 &&
    image_base[0] == ELFMAG0 &&
    image_base[1] == ELFMAG1 &&
    image_base[2] == ELFMAG2 &&
    image_base[3] == ELFMAG3) {
	Elf32_Ehdr *eh = (Elf32_Ehdr *)image_base;
	/* HdrS is found in the flattened image, which begins at off. */
	struct HdrS_struct *hh = (struct HdrS_struct *)
		silo_find_linux_HdrS((char *)image_base + off, len);

	/* Only kernels that say they tolerate running from somewhere
	 * other than phys_base may be left in the high window. */
	if (!hh || hh->ver < 0x300) {
		printf("SILO: HdrS 0x%x does not support high load, "
		       "using the low path\n", hh ? hh->ver : 0);
		goto normal_linux_load;
	}

	if (eh->e_ident[EI_CLASS] == ELFCLASS32 &&
	    eh->e_ident[EI_DATA] == ELFDATA2MSB) {
		Elf32_Phdr *ph = (Elf32_Phdr *)(image_base + eh->e_phoff);
		int i;
		int loaded = 0;

		for (i = 0; i < eh->e_phnum; i++, ph++) {
			if (ph->p_type != PT_LOAD)
				continue;

			printf("SILO: sun4m ELF load off=0x%x vaddr=0x%x filesz=0x%x memsz=0x%x\n",
			       ph->p_offset, ph->p_vaddr,
			       ph->p_filesz, ph->p_memsz);


			if (sun4m_map_kernel_elf_window(ph->p_vaddr,
				ph->p_memsz,
				&elf_map_va,
				&elf_map_len) < 0) {
				printf("SILO: sun4m ELF map failed\n");
				goto normal_linux_load;
			}
			memcpy((char *)ph->p_vaddr,
			       image_base + ph->p_offset,
			       ph->p_filesz);

			if (ph->p_memsz > ph->p_filesz) {
				memset((char *)(ph->p_vaddr + ph->p_filesz),
				       0,
				       ph->p_memsz - ph->p_filesz);
			}

			loaded++;
		}

		if (loaded) {
			printf("SILO: sun4m ELF entry 0x%x\n", eh->e_entry);
			ret_offset = eh->e_entry;
			image_base = (unsigned char *)elf_map_va;
			len = elf_map_len;

			/* Point at HdrS inside the loaded image, so the initrd
			 * and boot arguments below reach the running kernel.
			 */
			hdrs = (struct HdrS_struct *)
				silo_find_linux_HdrS((char *)eh->e_entry, len);

			/*
			 * Skip the old compressed-image memcpy/HdrS relocation
			 * path for this experiment.
			 */
			goto linux_loaded_at_entry;
		}
	}
}

normal_linux_load:

/* existing HdrS logic follows */
	memcpy (image_base, image_base + off, len);

        hdrs = (struct HdrS_struct *)
		silo_find_linux_HdrS((char *)image_base, image_len);

	printf("SILO: HdrS version 0x%x\n", hdrs ? hdrs->ver : 0);

	if (hdrs && hdrs->ver < 0x300 && image_base != (unsigned char *)0x4000) {
	    /* Kernel doesn't support being loaded to other than
	     * phys_base, so let's try to copy it down there. */
	    if ((unsigned int)&_start - 0x4000 < len) {
		/* Fuck, can't do that */
		printf("Your kernel cannot fit into the memory destination. This\n"
		       "can be resolved by recompiling the kernel with more devices\n"
		       "built as modules, or upgrading your kernel to one that\n"
		       "supports being loaded to higher memory areas (currently\n"
		       "2.6.3+ or 2.4.26+).\n");
		goto try_again;
	    }
	    printf("Kernel doesn't support loading to high memory, relocating...");

	    /* Ok, it fits, so copy it down there */
	    memcpy ((char *)0x4000, image_base, len);
	    image_base = (unsigned char *)0x4000;

	    /* Readjust some things */
	    ret_offset = 0x4000;
	    hdrs = (struct HdrS_struct *)
		silo_find_linux_HdrS((char *)image_base, image_len);

	    printf("done.\n");
	}
linux_loaded_at_entry:

	printf("SILO: final entry 0x%x image_base 0x%x len 0x%x\n",
               ret_offset, image_base, len);
        if (hdrs) {
	    unsigned int linux_ver = hdrs->linux_ver;

	    printf("Loaded kernel version %d.%d.%d\n", (linux_ver >> 16) & 0xff,
		   (linux_ver >> 8) & 0xff, linux_ver & 0xff);

            if (fill_reboot_cmd && hdrs->ver >= 0x201) { /* ie. uses reboot_command */
		char *q = (char *)hdrs->reboot_cmd_ptr_high;
                char *r;

                /* On Ultra there is xword there, this hack makes
                 * it work...
                 */
                if (q == (char *)0xfffff800 || !q)
                       q = (char *)hdrs->reboot_cmd_ptr_low;
                q = (char *)(((unsigned long)q)& 0x003fffff);
                if (q >= (char *)0x4000 && q <= (char *)0x300000) {
                    if (given_bootargs_by_user) {
                        if (strlen (silo_disk_get_bootdevice()) <= 254) {
			    strcpy (q, silo_disk_get_bootdevice());
			    r = strchr (q, 0);
			    if (strlen (given_bootargs) < 255 - (r - q)) {
			        *r++ = ' ';
			        strcpy (r, given_bootargs);
			    }
		        }
                    } else if (strlen (given_bootargs) <= 255)
                        strcpy (q, given_bootargs);
                }
            }
            if (!dig_into_params (params) && !hdrs->root_dev) {
                char *s1, *s2;

                s1 = cfg_get_strg(0, "root");
                if (s1) {
                    s2 = malloc(strlen(params) + 8 + strlen(s1));
                    strcpy(s2, params);
                    strcat(s2, " root=");
                    strcat(s2, s1);
                    params = s2;
                }
            }

            if (hdrs->ver >= 0x202) {
		if (architecture == sun4u)
		    kernel_params = (char *)((hdrs->bootstr_info_ptr_low - 0x400000) + 
				(image_base - 0x4000));
		else
		    kernel_params = (char *)(hdrs->bootstr_info_ptr_low & 0x3fffff);
	    }

            /* Some UltraAX machines have /dev/fd1 floppies only. */
            if (floppyswap) {
            	char *s1;
            	
            	for (s1 = params; (s1 = strstr(s1, "root=/dev/fd0")) != NULL; s1 += 13)
            		s1[12] = '1';
            }

            if (initrd_string) {
            	char *q, *r, *initrd_device, *initrd_kname, *initrd_limit, *initrd_cur, c;
		char *string;
            	int initrd_partno, len, statusok = 0;

		if (hdrs->ver >= 0x301)
			initrd_can_do_64bit_phys = 1;
            	
            	q = strchr (initrd_string, '|');
            	if (q && !initrd_size) {
            	    silo_fatal("When more than one initial ramdisk piece "
			       "is specified, you have to give\n"
			       "a non-zero initrd-size option which is no "
			       "shorter than sum of all pieces\n"
			       "lengths. Try again...\n");
            	    prom_halt ();
            	}
            	if (q) {
    		    initrd_start = memory_find ((initrd_size + 16383) & ~16383);
    		    if (!initrd_start) {
        		silo_fatal("You do not have enough continuous "
				   "available memory for such initial "
				   "ramdisk.");
        		prom_halt ();
    		    }
    		    string = strdup (initrd_string);
    		    q = strchr (string, '|');
    		    r = string;
    		    initrd_cur = initrd_start;
    		    initrd_limit = initrd_start + ((initrd_size + 16383) & ~16383);
    		    printf ("Loading parts of initial ramdisk...\n");
    		    for (;;) {
    		    	c = *q;
    		    	*q = 0;
			parse_name (r, initrd_defpart, &initrd_device, &initrd_partno, &initrd_kname);
			if (!initrd_kname) break;
			if (!initrd_device) initrd_device = initrd_defdevice;
			if (!silo_load_file(initrd_device, initrd_partno,
					    initrd_kname, (unsigned char *)initrd_cur,
					    (unsigned char *)initrd_limit, &len, 0, 0))
			  break;
			initrd_cur += len;
			if (!c) {
			    statusok = 1;
			    break;
			}
			r = q + 1;
			q = strchr (r, '|');
			if (!q) q = strchr (r, 0);
			if (initrd_prompt) {
			    silo_disk_close();
			    printf ("Insert next media and press ENTER");
			    prom_getchar ();
			    printf ("\n");
			}
    		    }
    		    free (string);
		    printf("Loaded initial ramdisk (%d bytes at 0x%x)...\n", (unsigned int)initrd_cur -
			   (unsigned int)initrd_start, initrd_start);
    		    if (statusok) {
	        	extern unsigned long long sun4u_initrd_phys;
	        	extern unsigned long sun4m_initrd_pa;

			if (architecture == sun4u) {
			    if (initrd_can_do_64bit_phys) {
				hdrs->ramdisk_image64 = 
					sun4u_initrd_phys + 0x400000ULL;
			    } else {
				hdrs->ramdisk_image =
					(unsigned int)sun4u_initrd_phys + 0x400000;
			    }
			} else if (sun4m_initrd_pa) {
	            	    hdrs->ramdisk_image = ((unsigned int)
	            	    	(sun4m_initrd_pa - sun4m_kernel_phys_base));
			} else
	            	    hdrs->ramdisk_image = ((unsigned int)initrd_start | 0xf0000000);

	            	hdrs->ramdisk_size = initrd_size;
    		    } else
    		        printf ("Error: initial ramdisk loading failed. No initrd will be used.\n");
            	} else {
		    parse_name (initrd_string, initrd_defpart, &initrd_device, &initrd_partno, &initrd_kname);
		    if (initrd_kname) {
		        if (!initrd_device) initrd_device = initrd_defdevice;
	        	if (silo_load_file(initrd_device, initrd_partno,
					   initrd_kname, (unsigned char *) 0x300000,
					   (unsigned char *) LARGE_RELOC,
					   0, 0, initrd_lenfunc)) {
	        	    extern unsigned long long sun4u_initrd_phys;
	        	    extern unsigned long sun4m_initrd_pa;

			    if (architecture == sun4u) {
				if (initrd_can_do_64bit_phys) {
				    hdrs->ramdisk_image64 =
					sun4u_initrd_phys + 0x400000ULL;
				} else {
				    hdrs->ramdisk_image =
					(unsigned int)sun4u_initrd_phys + 0x400000;
				}
			    } else if (sun4m_initrd_pa) {
				hdrs->ramdisk_image = ((unsigned int)
					(sun4m_initrd_pa - sun4m_kernel_phys_base));
			    } else
	            	        hdrs->ramdisk_image = ((unsigned int)initrd_start | 0xf0000000);

	            	    hdrs->ramdisk_size = initrd_size;
	        	}
		    } else
			printf ("Error: initial ramdisk loading failed. No initrd will be used.\n");
		}
	    }
        }
    } else {
        char *p;
        
        if (off != 0 && !reboot)
            memcpy (image_base, image_base + off, len);
        p = seed_part_into_device (other_device, other_part);
        strcpy (other_device, p);
        params_device = other_device;
    }

    silo_disk_close();
    if (timer_status >= 1)
        close_timer ();
    if (proll) {
	silo_set_prollargs(params, (unsigned int)image_base, len);
	if (show_arguments) {
	    printf ("Arguments: \"%s\"\n");
    	    pause_after = 1;
	}
    } else {
	silo_set_bootargs(params, params_device);

	if (kernel_params) {
    	    extern char barg_out[];
    	    int len = *(unsigned int *)kernel_params;

    	    strncpy (kernel_params + 8, barg_out, len);
    	    kernel_params [8 + len - 1] = 0;
    	    *(unsigned int *)(kernel_params + 4) = 1;
	}

	if (show_arguments) {
    	    silo_show_bootargs();
    	    pause_after = 1;
	}
    }

    if (pause_after) {
        printf ("%s", pause_message);
        prom_getchar ();
        printf ("\n");
    }

    memory_release();

    if (other && reboot) {
        strcpy (sol_params, params_device);
        strcat (sol_params, " ");
        strcat (sol_params, params);
    	prom_reboot(sol_params);
    }
	printf("SILO: final return ret_offset=0x%x image_base=0x%x len=0x%x\n",
       		ret_offset, image_base, len);
    return ret_offset;
}
