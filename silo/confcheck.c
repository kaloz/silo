/* siloconfcheck - validate silo.conf syntax

   Copyright (C) 2000 Ben Collins <bcollins@debian.org>

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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <ctype.h>

/* XXX: cfg.c needs these defined. Make sure they stay in sync with the
 * main set.  */
enum arch { sun4, sun4c, sun4m, sun4d, sun4e, sun4u, sun4p, sununknown };
enum arch architecture;

/* Tells cfg.c not to #include anything of it's own, we are in control.  */
#define SILOCONFCHECK 1

#include "../second/cfg.c"

int confcheck(char* conf) {
	struct stat st;
	int fd;
	char *buf;

	if (conf == NULL) return 1;

	if (stat(conf, &st) || ((fd = open(conf, O_RDONLY)) == -1) ||
	    ((buf = (char *)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED)) {
		perror(conf);
		return 0;
	}

	if (cfg_parse(conf, buf, st.st_size)) {
		fprintf(stderr, "Error parsing silo config file %s\n", conf);
		return 0;
	}

	munmap(buf, st.st_size);

	printf("%s appears to be valid\n", conf);

	return 1;
}
