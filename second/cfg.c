/* Handling and parsing of silo.conf
   
   Copyright (C) 1995 Werner Almesberger
   		 1996,1998 Jakub Jelinek
   
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

/* This file gets included by the confcheck (usespace), try to be nice */
#ifndef SILOCONFCHECK

#include "silo.h"
#include "stringops.h"
#include "setjmp.h"
#ifndef NULL
#define NULL (void *)0
#endif
#include "promlib.h"

#endif /* !SILOCONFCHECK */

typedef enum {
    cft_strg, cft_flag, cft_end
} CONFIG_TYPE;

typedef struct {
    CONFIG_TYPE type;
    char *name;
    void *data;
} CONFIG;

#define MAX_TOKEN 200
#define MAX_VAR_NAME MAX_TOKEN
#ifndef EOF
#define EOF -1
#endif

CONFIG cf_options[] =
{
    {cft_strg, "device", NULL},
    {cft_strg, "partition", NULL},
    {cft_strg, "default", NULL},
    {cft_strg, "timeout", NULL},
    {cft_strg, "message", NULL},
    {cft_strg, "root", NULL},
    {cft_strg, "ramdisk", NULL},
    {cft_flag, "read-only", NULL},
    {cft_flag, "read-write", NULL},
    {cft_strg, "append", NULL},
    {cft_strg, "initrd", NULL},
    {cft_flag, "initrd-prompt", NULL},
    {cft_strg, "initrd-size", NULL},
    {cft_flag, "pause-after", NULL},
    {cft_strg, "pause-message", NULL},
    {cft_flag, "fill-reboot-cmd", NULL},
    {cft_strg, "password", NULL},
    {cft_flag, "restricted", NULL},
    {cft_strg, "proll", NULL},
    {cft_end, NULL, NULL}};

CONFIG cf_image[] =
{
    {cft_strg, "image", NULL},
    {cft_strg, "other", NULL},
    {cft_strg, "label", NULL},
    {cft_strg, "alias", NULL},
    {cft_strg, "device", NULL},
    {cft_strg, "partition", NULL},
    {cft_strg, "root", NULL},
    {cft_strg, "ramdisk", NULL},
    {cft_flag, "read-only", NULL},
    {cft_flag, "read-write", NULL},
    {cft_strg, "append", NULL},
    {cft_strg, "literal", NULL},
    {cft_strg, "initrd", NULL},
    {cft_flag, "initrd-prompt", NULL},
    {cft_strg, "initrd-size", NULL},
    {cft_flag, "pause-after", NULL},
    {cft_strg, "pause-message", NULL},
    {cft_flag, "solaris", NULL},
    {cft_flag, "fill-reboot-cmd", NULL},
    {cft_strg, "bootblock", NULL},
    {cft_flag, "single-key", NULL},
    {cft_strg, "proll", NULL},
    {cft_end, NULL, NULL}};

static char flag_set;
static char *last_token = NULL, *last_item = NULL, *last_value = NULL;
static int line_num;
static int back = 0;		/* can go back by one char */
static char *currp;
static char *endp;
static char *file_name;
static CONFIG *curr_table = cf_options;
static jmp_buf env;

static struct IMAGES {
    CONFIG table[sizeof (cf_image) / sizeof (cf_image[0])];
    struct IMAGES *next;
} *images = NULL;

void cfg_error (char *msg,...)
{
    va_list ap;

    va_start (ap, msg);
    printf ("Config file error: ");
    vprintf (msg, ap);
    va_end (ap);
    printf (" near line %d in file %s\n", line_num, file_name);
    longjmp (env, 1);
}

inline int my_getc ()
{
    if (currp == endp)
	return EOF;
    return *currp++;
}

#define next_raw next
static int next (void)
{
    int ch;

    if (!back)
	return my_getc ();
    ch = back;
    back = 0;
    return ch;
}

static void again (int ch)
{
    back = ch;
}

static char *cfg_get_token (void)
{
    char buf[MAX_TOKEN + 1];
    char *here;
    int ch, escaped;

    if (last_token) {
	here = last_token;
	last_token = NULL;
	return here;
    }
    while (1) {
	while (ch = next (), ch == ' ' || ch == '\t' || ch == '\n')
	    if (ch == '\n')
		line_num++;
	if (ch == EOF)
	    return NULL;
	if (ch != '#')
	    break;
	while (ch = next_raw (), ch != '\n')
	    if (ch == EOF)
		return NULL;
	line_num++;
    }
    if (ch == '=')
	return strdup ("=");
    if (ch == '"') {
	here = buf;
	while (here - buf < MAX_TOKEN) {
	    if ((ch = next ()) == EOF)
		cfg_error ("EOF in quoted string");
	    if (ch == '"') {
		*here = 0;
		return strdup (buf);
	    }
	    if (ch == '\\') {
		ch = next ();
		if (ch != '"' && ch != '\\' && ch != '\n')
		    cfg_error ("Bad use of \\ in quoted string");
		if (ch == '\n') {
		    while ((ch = next ()), ch == ' ' || ch == '\t');
		    if (!ch)
			continue;
		    again (ch);
		    ch = ' ';
		}
	    }
	    if (ch == '\n' || ch == '\t')
		cfg_error ("\\n and \\t are not allowed in quoted strings");
	    *here++ = ch;
	}
	cfg_error ("Quoted string is too long");
	return 0;		/* not reached */
    }
    here = buf;
    escaped = 0;
    while (here - buf < MAX_TOKEN) {
	if (escaped) {
	    if (ch == EOF)
		cfg_error ("\\ precedes EOF");
	    if (ch == '\n')
		line_num++;
	    else
		*here++ = ch == '\t' ? ' ' : ch;
	    escaped = 0;
	} else {
	    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '#' ||
		ch == '=' || ch == EOF) {
		again (ch);
		*here = 0;
		return strdup (buf);
	    }
	    if (!(escaped = (ch == '\\')))
		*here++ = ch;
	}
	ch = next ();
    }
    cfg_error ("Token is too long");
    return 0;			/* not reached */
}

static void cfg_return_token (char *token)
{
    last_token = token;
}

static int cfg_next (char **item, char **value)
{
    char *this;

    if (last_item) {
	*item = last_item;
	*value = last_value;
	last_item = NULL;
	return 1;
    }
    *value = NULL;
    if (!(*item = cfg_get_token ()))
	return 0;
    if (!strcmp (*item, "="))
	cfg_error ("Syntax error");
    if (!(this = cfg_get_token ()))
	return 1;
    if (strcmp (this, "=")) {
	cfg_return_token (this);
	return 1;
    }
    if (!(*value = cfg_get_token ()))
	cfg_error ("Value expected at EOF");
    if (!strcmp (*value, "="))
	cfg_error ("Syntax error after %s", *item);
    return 1;
}

static void cfg_return (char *item, char *value)
{
    last_item = item;
    last_value = value;
}

static int cfg_set (char *item, char *value)
{
    CONFIG *walk;

    if (!strncasecmp (item, "image", 5) || !strcasecmp (item, "other")) {
	struct IMAGES **p = &images;

	if (item[5] == '[' && item[strlen(item) - 1] == ']') {
		char *p, *q = item;
		
		item[5] = 0;
		for (p = item + 6; q; p = q) {
			q = strchr(p, ',');
			if (q)
				*q++ = 0;
			if (strncasecmp (p, "sun4", 4))
				return 0;
			switch (tolower(p[4])) {
			case 0:
			case ']': if (architecture == sun4) goto cfg_set_cont; break;
			case 'c': if (architecture == sun4c) goto cfg_set_cont; break;
			case 'd': if (architecture == sun4d) goto cfg_set_cont; break;
			case 'e': if (architecture == sun4e) goto cfg_set_cont; break;
			case 'm': if (architecture == sun4m) goto cfg_set_cont; break;
			case 'u': if (architecture == sun4u) goto cfg_set_cont; break;
			default: return 0;
			}
		}
		curr_table = (CONFIG *)malloc(sizeof (cf_image));
		memcpy (curr_table, cf_image, sizeof (cf_image));
		goto cfg_set_redo;
	}
	if (item[5])
		return 0;
 cfg_set_cont:
	while (*p)
	    p = &((*p)->next);
	*p = malloc (sizeof (struct IMAGES));
	(*p)->next = 0;
	curr_table = ((*p)->table);
	memcpy (curr_table, cf_image, sizeof (cf_image));
    }
 cfg_set_redo:
    for (walk = curr_table; walk->type != cft_end; walk++) {
	if (walk->name && !strcasecmp (walk->name, item)) {
	    if (value && walk->type != cft_strg)
		cfg_error ("'%s' doesn't have a value", walk->name);
	    if (!value && walk->type == cft_strg)
		cfg_error ("Value expected for '%s'", walk->name);
	    if (walk->data)
		cfg_error ("Duplicate entry '%s'", walk->name);
	    if (walk->type == cft_flag)
		walk->data = &flag_set;
	    else if (walk->type == cft_strg)
		walk->data = value;
	    break;
	}
    }
    if (walk->type != cft_end) {
    	if (!strcasecmp (item, "other")) {
    	    item = "image";
    	    goto cfg_set_redo;
    	}
	return 1;
    }
    cfg_return (item, value);
    return 0;
}

int cfg_parse (char *cfg_file, char *buff, int len)
{
    char *item, *value;

    file_name = cfg_file;
    currp = buff;
    endp = currp + len;
    if (setjmp (env))
	return -1;
    while (1) {
	if (!cfg_next (&item, &value))
	    return 0;
	if (!cfg_set (item, value))
	    return -1;
	free (item);
    }
}

static char *cfg_get_strg_i (CONFIG * table, char *item)
{
    CONFIG *walk;

    for (walk = table; walk->type != cft_end; walk++)
	if (walk->name && !strcasecmp (walk->name, item))
	    return walk->data;
    return 0;
}

char *cfg_get_strg (char *image, char *item)
{
    struct IMAGES *p;
    char *label, *alias;
    char *ret;

    if (!image)
	return cfg_get_strg_i (cf_options, item);
    for (p = images; p; p = p->next) {
	label = cfg_get_strg_i (p->table, "label");
	if (!label) {
	    label = cfg_get_strg_i (p->table, "image");
	    alias = strrchr (label, '/');
	    if (alias)
		label = alias + 1;
	}
	alias = cfg_get_strg_i (p->table, "alias");
	if (!strcmp (label, image) || (alias && !strcmp (alias, image))) {
	    ret = cfg_get_strg_i (p->table, item);
	    if (!ret)
		ret = cfg_get_strg_i (cf_options, item);
	    return ret;
	}
    }
    return 0;
}

int cfg_get_flag (char *image, char *item)
{
    return !!cfg_get_strg (image, item);
}

static int printl_count = 0;
static void printlabel (char *label)
{
    int len = strlen (label);

    if (!printl_count)
	printf ("\n");
    printf ("%s", label);
    while (len++ < 25)
	putchar (' ');
    printl_count++;
    if (printl_count == 3)
	printl_count = 0;
}

void cfg_print_images (void)
{
    struct IMAGES *p;
    char *label, *alias;

    printl_count = 0;
    for (p = images; p; p = p->next) {
	label = cfg_get_strg_i (p->table, "label");
	if (!label) {
	    label = cfg_get_strg_i (p->table, "image");
	    alias = strrchr (label, '/');
	    if (alias)
		label = alias + 1;
	}
	alias = cfg_get_strg_i (p->table, "alias");
	printlabel (label);
	if (alias)
	    printlabel (alias);
    }
    printf ("\n");
}

char *cfg_get_default (void)
{
    char *label;
    char *ret = cfg_get_strg_i (cf_options, "default");

    if (ret)
	return ret;
    if (!images)
	return 0;
    ret = cfg_get_strg_i (images->table, "label");
    if (!ret) {
	ret = cfg_get_strg_i (images->table, "image");
	label = strrchr (ret, '/');
	if (label)
	    ret = label + 1;
    }
    return ret;
}
