/* $Id: tree.c,v 1.1 2001/05/25 14:41:26 bencollins Exp $
 * tree.c: Basic device tree traversal/scanning for the Linux
 *         prom library.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <silo.h>
#include <stringops.h>

static char promlib_buf[128];

/* Return the child of node 'node' or zero if no this node has no
 * direct descendent.
 */
int prom_getchild(int node)
{
	int cnode;

	if (node == -1)
		return 0;

	if (prom_vers != PROM_P1275)
		cnode = prom_nodeops->no_child(node);
	else
		cnode = p1275_cmd ("child", 1, node);
		
	if (cnode == 0 || cnode == -1)
		return 0;

	return cnode;
}

/* Return the next sibling of node 'node' or zero if no more siblings
 * at this level of depth in the tree.
 */
int prom_getsibling(int node)
{
	int sibnode;

	if (node == -1)
		return 0;

	if (prom_vers != PROM_P1275)
		sibnode = prom_nodeops->no_nextnode(node);
	else
		sibnode = p1275_cmd ("peer", 1, node);
		
	if (sibnode == 0 || sibnode == -1)
		return 0;

	return sibnode;
}

/* Return the length in bytes of property 'prop' at node 'node'.
 * Return -1 on error.
 */
int prom_getproplen(int node, char *prop)
{
	int ret;

	if((!node) || (!prop))
		ret = -1;
	else {
		if (prom_vers != PROM_P1275)
			ret = prom_nodeops->no_proplen(node, prop);
		else
			ret = p1275_cmd ("getproplen", 2, node, prop);
	}
	return ret;
}

/* Acquire a property 'prop' at node 'node' and place it in
 * 'buffer' which has a size of 'bufsize'.  If the acquisition
 * was successful the length will be returned, else -1 is returned.
 */
int prom_getproperty(int node, char *prop, char *buffer, int bufsize)
{
	int plen, ret;

	plen = prom_getproplen(node, prop);
	if((plen > bufsize) || (plen == 0) || (plen == -1))
		ret = -1;
	else {
		/* Ok, things seem all right. */
		if (prom_vers != PROM_P1275)
			ret = prom_nodeops->no_getprop(node, prop, buffer);
		else
			ret = p1275_cmd ("getprop", 4, node, prop, buffer, bufsize);
	}
	return ret;
}

/* Acquire an integer property and return its value.  Returns -1
 * on failure.
 */
int prom_getint(int node, char *prop)
{
	static int intprop;

	if(prom_getproperty(node, prop, (char *) &intprop, sizeof(int)) != -1)
		return intprop;

	return -1;
}

/* Acquire an integer property, upon error return the passed default
 * integer.
 */
int prom_getintdefault(int node, char *property, int deflt)
{
	int retval;

	retval = prom_getint(node, property);
	if(retval == -1) return deflt;

	return retval;
}

/* Acquire a boolean property, 1=TRUE 0=FALSE. */
int prom_getbool(int node, char *prop)
{
	int retval;

	retval = prom_getproplen(node, prop);
	if(retval == -1) return 0;
	return 1;
}

/* Acquire a property whose value is a string, returns a null
 * string on error.  The char pointer is the user supplied string
 * buffer.
 */
void prom_getstring(int node, char *prop, char *user_buf, int ubuf_size)
{
	int len;

	len = prom_getproperty(node, prop, user_buf, ubuf_size);
	if(len != -1) return;
	user_buf[0] = 0;
	return;
}


/* Does the device at node 'node' have name 'name'?
 * YES = 1   NO = 0
 */
int prom_nodematch(int node, char *name)
{
	static char namebuf[128];
	prom_getproperty(node, "name", namebuf, sizeof(namebuf));
	if(strcmp(namebuf, name) == 0) return 1;
	return 0;
}

/* Search siblings at 'node_start' for a node with name
 * 'nodename'.  Return node if successful, zero if not.
 */
int prom_searchsiblings(int node_start, char *nodename)
{

	int thisnode, error;

	for(thisnode = node_start; thisnode;
	    thisnode=prom_getsibling(thisnode)) {
		error = prom_getproperty(thisnode, "name", promlib_buf,
					 sizeof(promlib_buf));
		/* Should this ever happen? */
		if(error == -1) continue;
		if(strcmp(nodename, promlib_buf)==0) return thisnode;
	}

	return 0;
}

/* Gets name in the form prom v2+ uses it (name@x,yyyyy or name (if no reg)) */
int prom_getname (int node, char *buffer, int len)
{
	int i;
	struct linux_prom_registers reg[PROMREG_MAX];
	
	i = prom_getproperty (node, "name", buffer, len);
	if (i <= 0) return -1;
	buffer [i] = 0;
	len -= i;
	i = prom_getproperty (node, "reg", (char *)reg, sizeof (reg));
	if (i <= 0) return 0;
	if (len < 11) return -1;
	buffer = strchr (buffer, 0);
	sprintf (buffer, "@%x,%x", reg[0].which_io, (unsigned)reg[0].phys_addr);
	return 0;
}

/* Return the first property type for node 'node'.
 */
char * prom_firstprop(int node, char *buffer)
{
	char *ret;

	*buffer = 0;
	if (node == -1) return buffer;
	if (prom_vers != PROM_P1275) {
		ret = prom_nodeops->no_nextprop(node, (char *) 0x0);
		strcpy (buffer, ret);
	} else {
		p1275_cmd ("nextprop", 3, node, (char *)0, buffer);
	}
	return buffer;
}

/* Return the property type string after property type 'oprop'
 * at node 'node' .  Returns NULL string if no more
 * property types for this node.
 */
char * prom_nextprop(int node, char *oprop, char *buffer)
{
	char *ret;
	char buf[32];

	if (node == -1) {
		*buffer = 0;
		return buffer;
	}
	if (oprop == buffer) {
		strcpy (buf, oprop);
		oprop = buf;
	}
	*buffer = 0;
	if(node == -1) return buffer;
	if (prom_vers != PROM_P1275) {
		ret = prom_nodeops->no_nextprop(node, oprop);
		strcpy (buffer, ret);
	} else {
		p1275_cmd ("nextprop", 3, node, oprop, buffer);
	}
	return buffer;
}

int prom_node_has_property(int node, char *prop)
{
	char buf [32];

	*buf = 0;
	do {
		prom_nextprop(node, buf, buf);
		if(!strcmp(buf, prop))
		   return 1;
	} while (*buf);
	return 0;
}

/* Set property 'pname' at node 'node' to value 'value' which has a length
 * of 'size' bytes.  Return the number of bytes the prom accepted.
 */
int prom_setprop(int node, char *pname, char *value, int size)
{
	int ret;

	if((pname == 0) || (value == 0)) return 0;
	if (prom_vers != PROM_P1275)
		ret = prom_nodeops->no_setprop(node, pname, value, size);
	else
		ret = p1275_cmd ("setprop", 4, node, pname, value, size);
	return ret;
}

int prom_inst2pkg(int inst)
{
	int node;

	if (prom_vers != PROM_P1275)
		node = (*romvec->pv_v2devops.v2_inst2pkg)(inst);
	else
		node = p1275_cmd ("instance-to-package", 1, inst);
	if (node == -1) return 0;
	return node;
}

int prom_finddevice(char *path)
{
	int node;

	switch (prom_vers) {
	case PROM_P1275:
		node = p1275_cmd ("finddevice", 1, path);
		break;
	default:
		return 0;
	}
	if (node == -1) return 0;
	return node;
}
