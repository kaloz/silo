/* Memory utilities

   Copyright (C) 1995, 1996 David S. Miller
   		 1996, 1998, 1999 Jakub Jelinek
   		 1996 Andrew Tridgell

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

struct linux_prom_registers prom_reg_memlist[64];
struct linux_mlist_v0 prom_phys_avail[64];

/* Internal Prom library routine to sort a linux_mlist_v0 memory
 * list.  Used below in initialization.
 */
static void prom_sortmemlist (struct linux_mlist_v0 *thislist)
{
    int swapi = 0;
    int i, mitr, tmpsize;
    char *tmpaddr;
    char *lowest;

    for (i = 0; thislist[i].theres_more != 0; i++) {
	lowest = thislist[i].start_adr;
	for (mitr = i + 1; thislist[mitr - 1].theres_more != 0; mitr++)
	    if (thislist[mitr].start_adr < lowest) {
		lowest = thislist[mitr].start_adr;
		swapi = mitr;
	    }
	if (lowest == thislist[i].start_adr)
	    continue;
	tmpaddr = thislist[swapi].start_adr;
	tmpsize = thislist[swapi].num_bytes;
	for (mitr = swapi; mitr > i; mitr--) {
	    thislist[mitr].start_adr = thislist[mitr - 1].start_adr;
	    thislist[mitr].num_bytes = thislist[mitr - 1].num_bytes;
	}
	thislist[i].start_adr = tmpaddr;
	thislist[i].num_bytes = tmpsize;
    }
}

/* Initialize the memory lists based upon the prom version. */
struct linux_mlist_v0 *prom_meminit (void)
{
    int node = 0;
    unsigned int iter, num_regs;
    struct linux_mlist_v0 *mptr;	/* ptr for traversal */
    static int meminited = 0;
    
    if (meminited) return prom_phys_avail;
    meminited = 1;

    switch (prom_vers) {
    case PROM_V0:
	/* Nice, kind of easier to do in this case. */
	/* First, the total physical descriptors. */
	/* Last, the available physical descriptors. */
	for (mptr = (*(romvec->pv_v0mem.v0_available)), iter = 0;
	     mptr; mptr = mptr->theres_more, iter++) {
	    prom_phys_avail[iter].start_adr = mptr->start_adr;
	    prom_phys_avail[iter].num_bytes = mptr->num_bytes;
	    prom_phys_avail[iter].theres_more = &prom_phys_avail[iter + 1];
	}
	prom_phys_avail[iter - 1].theres_more = 0;
	prom_sortmemlist (prom_phys_avail);
	break;
    case PROM_V2:
    case PROM_V3:
	/* Grrr, have to traverse the prom device tree ;( */
	node = prom_getchild (prom_root_node);
	node = prom_searchsiblings (node, "memory");
	num_regs = prom_getproperty (node, "available",
				     (char *) prom_reg_memlist,
				     sizeof (prom_reg_memlist));
	num_regs = (num_regs / sizeof (struct linux_prom_registers));
	for (iter = 0; iter < num_regs; iter++) {
	    prom_phys_avail[iter].start_adr =
		prom_reg_memlist[iter].phys_addr;
	    prom_phys_avail[iter].num_bytes =
		(unsigned long) prom_reg_memlist[iter].reg_size;
	    prom_phys_avail[iter].theres_more =
		&prom_phys_avail[iter + 1];
	}
	prom_phys_avail[iter - 1].theres_more = 0;
	prom_sortmemlist (prom_phys_avail);
    default:
	break;

    }
    return prom_phys_avail;
}

static int sun4c_hwflushes;
static int sun4c_linesize;
static void sun4c_init (void)
{
    static int inited = 0;
    int propval;
    
    if (!inited) {
        inited = 1;
        propval = prom_getintdefault (prom_root_node, "vac_hwflush", -1);
        sun4c_hwflushes = (propval == -1) ? prom_getintdefault (prom_root_node, "vac-hwflush", 0) : propval;
        sun4c_linesize = prom_getintdefault (prom_root_node, "vac-linesize", 16);
    }
}

void sun4c_map (unsigned long virtual, unsigned long page)
{
    unsigned long virt = virtual;

    sun4c_init ();
    if (sun4c_hwflushes) {
    	__asm__ __volatile__ ("\n\tsta %%g0, [%0] 0x06\n\t" : : "r" (virt));
    } else {
        unsigned long end = virt + 4096;
        
        for (; virt < end; virt += sun4c_linesize)
            __asm__ __volatile__ ("\n\tsta %%g0, [%0] 0x0d\n\t" : : "r" (virt));
    }
    virt = virtual;
    __asm__ __volatile__ ("\n\tsta %1, [%0] 0x04\n\t" : : "r" (virt), "r" (page));
}

int sun4c_mapio (unsigned long phys, unsigned long virtual, int rdonly)
{
    unsigned long page = ((phys >> 12) & 0xffff) | 0x94000000;
    
    if (!rdonly) page |= 0x40000000;
    sun4c_map (virtual & ~4095, page);
    return 0;
}

void sun4c_unmapio (unsigned long virtual)
{
    sun4c_map (virtual & ~4095, 0);
}

inline unsigned long sun4m_get_lev1 (void)
{
    unsigned long ret;
    
    __asm__ ("\n\t"
	"set 0x100, %0\n\t"
	"lda [%0] 4, %0\n\t"
	"sll %0, 4, %0\n\t"
	"lda [%0] 32, %0\n\t"
	"srl %0, 4, %0\n\t"
	"sll %0, 8, %0\n\t" : "=r" (ret));
    return ret;
}

inline unsigned long sun4m_probe (unsigned long l)
{
    unsigned long ret;
    
    __asm__ ("\n\t"
        "lda [%1] 3, %0" : "=r" (ret) : "r" (l | 0x400));
    return ret;
}

inline unsigned long sun4m_get_direct (unsigned long l)
{
    unsigned long ret;
    __asm__ ("\n\t"
	"lda [%1] 32, %0\n\t" : "=r" (ret) : "r" (l));
    return ret;
}

inline void sun4m_set_direct (unsigned long l, unsigned long set)
{
    __asm__ ("\n\t"
	"sta %0, [%1] 32\n\t" : : "r" (set), "r" (l));
}

#ifndef TLB_TAG_ACCESS
#define TLB_TAG_ACCESS 		0x30
#endif
#ifndef ASI_DMMU
#define ASI_DMMU 		0x58
#define ASI_DTLB_DATA_ACCESS	0x5d
#endif

unsigned long sun4u_initrd_pa;
static unsigned long long sun4u_memory_base;
unsigned long sun4m_initrd_pa;
unsigned long sun4m_initrd_va;
extern unsigned int linux_end;
extern unsigned long _start;

char *memory_find (int len)
{
    register struct linux_mlist_v0 *mlist;
    char *beg = 0, *start;
    int l = 0, num;
    unsigned long totalmem = 0;
    char *min = (char *)0x300000;

    if (linux_end && (linux_end & 0x3fffff) + len < ((long)&_start) - 16384)
	return (char *)(linux_end & 0x3fffff);
    if (architecture != sun4u) {
        prom_meminit ();
        for (mlist = prom_phys_avail; mlist; mlist = mlist->theres_more) {
            totalmem += mlist->num_bytes;
            if (totalmem >= 0x4000000)
            	break;
        }
        if (architecture != sun4c) {
            unsigned long ll;
            if (totalmem >= 0x4000000)
		min = (char *)0x3000000;
	    else if (totalmem >= 0x2000000)
		min = (char *)0x1000000;
	    ll = (sun4m_probe (0x4000) & 0xffffff00) << 4;
	    ll -= 0x4000;
	    min += ll;
	}
        mlist = prom_phys_avail;
        for (;;) {
            if (beg && mlist->start_adr != beg + l)
                beg = 0;
            start = mlist->start_adr;
            num = mlist->num_bytes;
            if (start <= min) {
                num += start - min;
                start = min;
            }
            if (num > 0) {
                if (num + (beg ? l : 0) >= len) {
                    if (!beg) beg = start;
                    if (architecture == sun4c)
                    	return beg;
                    else {
                        unsigned long lev1;
                        int i;
                    	sun4m_initrd_pa = (unsigned long)beg;
                    	lev1 = sun4m_get_lev1();
                    	for (i = 0x60; i < 0xa0; i++)
                    	    if (!(sun4m_get_direct(lev1 + 4*i) & 3))
                   		break;
                        if (i == 0xa0) return (char *)0;
                        sun4m_set_direct(lev1 + 4*i, ((sun4m_initrd_pa & 0xff000000) >> 4) | 0x9e);
                        sun4m_initrd_va = i << 24;
                        return (char *)sun4m_initrd_va + (sun4m_initrd_pa & 0xffffff);
                    }
                }
                if (beg) l += num;
                else {
                    beg = start;
                    l = num;
                }
            }
            if (!mlist->theres_more) goto not_found;
            mlist = mlist->theres_more;
        }
    } else {
        int n, node, i;
        struct p1275_mem { unsigned long long pa; unsigned long long len; } *p;
        unsigned long long phys_base, base, b;

	p = (struct p1275_mem *)malloc(2048);
        node = prom_finddevice("/memory");
        if (prom_getproperty(node, "reg", (char *)p, 2048) == -1) {
            free (p);
            printf("Could not get reg property\n");
            return (char *)0;
        }
        phys_base = p[0].pa;
        n = prom_getproplen(node, "available");
        if (!n || n == -1 || prom_getproperty(node, "available", (char *)p, 2048) == -1) {
            free (p);
            printf("Could not get available property\n");
            return (char *)0;
        }
        base = 0;
        b = phys_base + 0x100000000ULL - 0x400000ULL;
        n /= sizeof(*p);
        len += 8192;
        for (i = 0; i < n; i++) {
            if (p[i].pa + p[i].len <= b &&
                p[i].pa >= base &&
                p[i].len >= len)
                base = (p[i].pa + p[i].len - len + 7) & ~7;
        }
        free (p);
	if (base >= b || base < phys_base + 0x400000ULL) {
            printf("Could not find any available memory for initial ramdisk\n");
            return (char *)0;
        }
	sun4u_memory_base = base & ~0x3fffffULL;
	sun4u_initrd_pa = base - phys_base;
        __asm __volatile("\n\
            sethi %%hi(0xe0000000), %%g1\n\
            ldx [%3], %%g2\n\
            sllx %%g1, 32, %%g1\n\
            or %%g2, 0x77, %%g2\n\
            or %%g2, %%g1, %%g2\n\
            rdpr %%pil, %%g1\n\
            wrpr 15, %%pil\n\
            stxa %0, [%1] %2\n\
            stxa %%g2, [%4] %5\n\
            membar #Sync\n\
            flush %0\n\
            membar #Sync\n\
            wrpr %%g1, %%pil\n\
        " : : "r" (0x40000000), "r" (TLB_TAG_ACCESS), "i" (ASI_DMMU),
              "r" (&sun4u_memory_base), "r" (63 << 3), "i" (ASI_DTLB_DATA_ACCESS) : "g1", "g2");
        return (char *)0x40000000 + ((long)base & 0x3fffffUL);
    }
not_found:
    return (char *)0;
}

void memory_release(void)
{
    if (architecture == sun4u) {
        __asm __volatile("\n\
            rdpr %%pil, %%g1\n\
            wrpr 16, %%pil\n\
            stxa %%g0, [%0] %1\n\
            stxa %%g0, [%2] %3\n\
            membar #Sync\n\
            flush %4\n\
            membar #Sync\n\
            wrpr %%g1, %%pil\n\
        " : : "r" (TLB_TAG_ACCESS), "i" (ASI_DMMU),
              "r" (63 << 3), "i" (ASI_DTLB_DATA_ACCESS),
              "r" (memory_release) : "g1");
    } else if (sun4m_initrd_pa) {
	unsigned long lev1;
        lev1 = sun4m_get_lev1();
        sun4m_set_direct(lev1 + (sun4m_initrd_va >> 24) * 4, 0);
    }
}
