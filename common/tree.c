/* $Id: tree.c,v 1.2 2001/06/16 06:35:24 bencollins Exp $
 * tree.c: Basic device tree traversal/scanning for the Linux
 *         prom library.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <silo.h>
#include <stringops.h>

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

/* Search siblings at 'node_start' for a node with name
 * 'nodename'.  Return node if successful, zero if not.
 */
int prom_searchsiblings(int node_start, char *nodename)
{

	int thisnode, error;
	static char promlib_buf[128];

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
