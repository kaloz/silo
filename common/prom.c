/* $Id: prom.c,v 1.2 2001/05/25 22:51:28 bencollins Exp $
 * init.c:  Initialize internal variables used by the PROM
 *          library functions.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <promlib.h>

struct linux_romvec *romvec;
enum prom_major_version prom_vers;
unsigned int prom_rev, prom_prev;
void (*prom_cif_handler)(long long *);
void (*prom_cif_h32)(long *);
void *prom_cif_stack;

/* The root node of the prom device tree. */
int prom_root_node;

int prom_stdin, prom_stdout;
int prom_chosen;

/* Pointer to the device tree operations structure. */
struct linux_nodeops *prom_nodeops;

/* You must call prom_init() before you attempt to use any of the
 * routines in the prom library.  It returns 0 on success, 1 on
 * failure.  It gets passed the pointer to the PROM vector.
 */

static long long p1275_args[23];

int p1275_cmd (char *service, unsigned fmt, ...)
{
	va_list list;
	int i;
	int args = fmt & 0xf;

	fmt >>= 8;

	/* This means we have a P1275 PROM on a 32-bit machine. There
	 * better not be any 64-bit arguments. */
	if (prom_cif_handler == 0) {
		long *p1275_args32 = (long *)p1275_args;

		if (fmt)
			prom_halt();

		p1275_args32[0] = (long)service;
		p1275_args32[1] = args;
		p1275_args32[2] = 1;
		va_start (list, fmt);
		for (i = 0; i < args; i++)
			p1275_args32[i + 3] = (long) va_arg (list, char *);
		va_end (list);
		(*prom_cif_h32)(p1275_args32);
		return p1275_args32[3 + args];
	}

	p1275_args[0] = (unsigned long long)(unsigned long)service;
	p1275_args[1] = args;
	p1275_args[2] = 1;
	va_start (list, fmt);
	for (i = 0; i < args; i++, fmt >>= 1) {
		if (fmt & 0x1)
			p1275_args[i + 3] = va_arg (list, unsigned long long);
		else
			p1275_args[i + 3] = (unsigned long long)(unsigned long) va_arg (list, char *);
	}
	va_end (list);
	__asm__ __volatile__ ("\t"
		"mov	%1, %%g1\n\t"
		"mov	%2, %%g2\n\t"
		"save	%0, -0xc0, %%sp\n\t"
		"rdpr	%%pstate, %%l1\n\t"
		"andn	%%l1, 8, %%l1\n\t"
		"wrpr	%%l1, 0, %%pstate\n\t"
		"call	%%g1\n\t"
		" mov	%%g2, %%o0\n\t"
		"wrpr	%%l1, 8, %%pstate\n\t"
		"restore\n"
	: : "r" (prom_cif_stack), "r" (prom_cif_handler), "r" (p1275_args) :
	"o0", "o1", "o2", "o3", "o4", "o5", "o7", "g1", "g2", "g3", "g4",
	"g5", "g6", "g7");
	return (int) p1275_args [3 + args];
}

void prom_init(struct linux_romvec *rp, void *cifh, void *cifsp)
{
	if (cifh || ((int)rp & 1) != 0) {
		prom_cif_handler = cifh;
		prom_cif_h32 = (void *)((unsigned long)rp & ~1);
		prom_cif_stack = cifsp;
		prom_vers = PROM_P1275;
		prom_chosen = p1275_cmd ("finddevice", 1, "/chosen");
		if (prom_chosen == -1) prom_halt ();
		prom_stdin = prom_getint (prom_chosen, "stdin");
		prom_stdout = prom_getint (prom_chosen, "stdout");
	} else {
		romvec = rp;

		switch(romvec->pv_romvers) {
		case 0:
			prom_vers = PROM_V0;
			break;
		case 2:
			prom_vers = PROM_V2;
			break;
		case 3:
			prom_vers = PROM_V3;
			break;
		}

		prom_rev = romvec->pv_plugin_revision;
		prom_prev = romvec->pv_printrev;
		prom_nodeops = romvec->pv_nodeops;
		if (prom_vers != PROM_V0) {
			prom_stdin = *romvec->pv_v2bootargs.fd_stdin;
			prom_stdout = *romvec->pv_v2bootargs.fd_stdout;
		}
		if((((unsigned long) prom_nodeops) == 0) || 
		   (((unsigned long) prom_nodeops) == -1))
			prom_halt();
	}

	prom_root_node = prom_getsibling(0);
	if((prom_root_node == 0) || (prom_root_node == -1))
		prom_halt();

	/* Initialization successful. */
	return;
}

void
prom_chain(unsigned long addr, int size, unsigned long entry, char *args, int len)
{
	p1275_cmd ("chain", 5, addr, size, entry, args, len);
}

void
prom_reboot(char *bcommand)
{
	p1275_cmd ("boot", 1, bcommand);
}

/* Drop into the prom, with the chance to continue with the 'go'
 * prom command.
 */
void
prom_cmdline(void)
{
	if (prom_vers != PROM_P1275)
		(*(romvec->pv_abort))();
	else
		p1275_cmd ("enter", 0);
}

/* Drop into the prom, but completely terminate the program.
 * No chance of continuing.
 */
void
prom_halt(void)
{
	if (prom_vers != PROM_P1275)
		(*(romvec->pv_halt))();
	else
		p1275_cmd ("exit", 0);
	/* Not reached */
}

static int mmu_ihandle_cache = 0;

static int prom_get_mmu_ihandle(void)
{
	int node, ret;

	if (mmu_ihandle_cache != 0)
		return mmu_ihandle_cache;

	node = prom_finddevice("/chosen");
	ret = prom_getint(node, "mmu");
	if (ret == -1 || ret == 0)
		mmu_ihandle_cache = -1;
	else
		mmu_ihandle_cache = ret;

	return ret;
}

int prom_map(int mode, unsigned long long size,
	     unsigned long long vaddr,
	     unsigned long long paddr)
{
	int ret = p1275_cmd("call-method",
			    P1275_ARG_64B(3) | P1275_ARG_64B(4) |
			    P1275_ARG_64B(6) | 7,
			    "map",
			    prom_get_mmu_ihandle(),
			    mode,
			    size,
			    vaddr,
			    0,
			    paddr);

        return ret;
}

void prom_unmap(unsigned long long size, unsigned long long vaddr)
{
	p1275_cmd("call-method",
		  P1275_ARG_64B(0) | P1275_ARG_64B(2) |
		  P1275_ARG_64B(3) | 4,
		  "unmap",
		  prom_get_mmu_ihandle(),
		  size,
		  vaddr);
}
