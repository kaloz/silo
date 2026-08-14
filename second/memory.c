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

#define INITRD_VIRT_ADDR	0x40c00000
#define IMAGE_VIRT_ADDR		0x40000000

extern int initrd_can_do_64bit_phys;

static char *sun4u_memory_find (unsigned int len, int is_kernel);

struct linux_prom_registers prom_reg_memlist[64];
struct linux_mlist_v0 prom_phys_avail[64];


static unsigned long sun4m_image_pa;
static unsigned long sun4m_image_va;
static unsigned long sun4m_image_len;

static unsigned long sun4m_kernel_pa;
static unsigned long sun4m_kernel_va;
static unsigned long sun4m_kernel_len;

/* Physical base the kernel was loaded at, 0 if it was not high loaded. */
unsigned long sun4m_kernel_phys_base;

/* Internal Prom library routine to sort a linux_mlist_v0 memory
 * list.  Used below in initialization.
 */

#define SUN4M_L1_ET_MASK	0x3
/* Linux/sparc32 PAGE_OFFSET, the virtual address phys_base is
 * defined against.
 */
#define SUN4M_PAGE_OFFSET	0xf0000000UL
#define SUN4M_L1_SIZE   0x01000000
#define SUN4M_LOW_AVOID 0x01000000

#if SUN4M_L1_SIZE != 0x01000000
#error "SUN4M_L1_SIZE must be 16 MB for SRMMU L1 mappings"
#endif

static inline void sun4m_flush_tlb_all(void);
static inline unsigned long sun4m_get_lev1(void);
static inline unsigned long sun4m_get_direct(unsigned long l);
static inline void sun4m_set_direct(unsigned long l, unsigned long set);
static unsigned long sun4m_find_phys_window(unsigned long len);
static inline unsigned long sun4m_make_l1_pte(unsigned long pa);

static int sun4m_map_l1_window(unsigned long va,
			       unsigned long pa,
			       unsigned long len)
{
	unsigned long lev1;
	unsigned long off;
	unsigned long idx;
	unsigned long pte;

	if (va & (SUN4M_L1_SIZE - 1))
		return -1;
	if (pa & (SUN4M_L1_SIZE - 1))
		return -1;
	if (len & (SUN4M_L1_SIZE - 1))
		return -1;

	lev1 = sun4m_get_lev1();

	/* Never displace a mapping somebody else owns: PROM and SILO state
	 * live in this table too.
	 */
	for (off = 0; off < len; off += SUN4M_L1_SIZE) {
		idx = (va + off) >> 24;
		pte = sun4m_get_direct(lev1 + 4 * idx);
		if (pte & SUN4M_L1_ET_MASK)
			return -1;
	}

	for (off = 0; off < len; off += SUN4M_L1_SIZE) {
		sun4m_set_direct(lev1 + (((va + off) >> 24) * 4),
				  sun4m_make_l1_pte(pa + off));
	}
	sun4m_flush_tlb_all();
	return 0;
}


static int ranges_overlap(unsigned long a_start, unsigned long a_len,
			  unsigned long b_start, unsigned long b_len)
{
	unsigned long a_end = a_start + a_len;
	unsigned long b_end = b_start + b_len;

	return a_start < b_end && b_start < a_end;
}


/* Map one contiguous physical span covering [min_va, max_va), install every
 * L1 entry for it once, and record the single VA<->PA relationship.
 */
int sun4m_map_kernel_elf_window(unsigned long min_va,
                                unsigned long max_va,
                                unsigned long *mapped_va,
                                unsigned long *mapped_len)
{
        unsigned long map_va;
        unsigned long map_end;
        unsigned long map_len;
        unsigned long pa;

        if (max_va <= min_va)
                return -1;

        map_va = min_va & ~(SUN4M_L1_SIZE - 1);
        map_end = (max_va + SUN4M_L1_SIZE - 1) & ~(SUN4M_L1_SIZE - 1);
        if (map_end <= map_va)
                return -1;
        map_len = map_end - map_va;

        /* phys_base is what PAGE_OFFSET maps to, so a window that does not
         * cover it cannot express one.
         */
        if (map_va > SUN4M_PAGE_OFFSET || map_end <= SUN4M_PAGE_OFFSET)
                return -1;

        pa = sun4m_find_phys_window(map_len);
        if (!pa)
                return -1;

        if (sun4m_map_l1_window(map_va, pa, map_len) < 0)
                return -1;

        printf("SILO: sun4m kernel ELF map VA=0x%x PA=0x%x LEN=0x%x\n",
               map_va, pa, map_len);

        sun4m_kernel_pa = pa;
        sun4m_kernel_va = map_va;
        sun4m_kernel_len = map_len;

        /* What PAGE_OFFSET maps to, which is what Linux calls phys_base.
         * Equal to pa whenever the image starts at PAGE_OFFSET.
         */
        sun4m_kernel_phys_base = pa + (SUN4M_PAGE_OFFSET - map_va);

        if (mapped_va)
                *mapped_va = map_va;

        if (mapped_len)
                *mapped_len = map_len;

        return 0;
}

/* Drop the staging mapping and forget it. */
void sun4m_image_release(void)
{
        unsigned long lev1;
        unsigned long off;

        if (!sun4m_image_len)
                goto out;

        lev1 = sun4m_get_lev1();
        for (off = 0; off < sun4m_image_len; off += SUN4M_L1_SIZE)
                sun4m_set_direct(lev1 + (((sun4m_image_va + off) >> 24) * 4), 0);
        sun4m_flush_tlb_all();

out:
        sun4m_image_pa = 0;
        sun4m_image_va = 0;
        sun4m_image_len = 0;
}

/* Drop the kernel mapping and forget it, so a fallback or retry cannot
 * inherit stale state.
 */
void sun4m_kernel_release(void)
{
        unsigned long lev1;
        unsigned long off;

        if (!sun4m_kernel_len)
                goto out;

        lev1 = sun4m_get_lev1();
        for (off = 0; off < sun4m_kernel_len; off += SUN4M_L1_SIZE)
                sun4m_set_direct(lev1 + (((sun4m_kernel_va + off) >> 24) * 4), 0);
        sun4m_flush_tlb_all();

out:
        sun4m_kernel_pa = 0;
        sun4m_kernel_va = 0;
        sun4m_kernel_len = 0;
        sun4m_kernel_phys_base = 0;
}

static inline unsigned long sun4m_make_l1_pte(unsigned long pa)
{
	/*
	 * Same L1 PTE form as SILO's existing sun4m initrd mapping:
	 * 16 MB physical base encoded into an SRMMU level-1 PTE.
	 *
	 * Caller must pass a 16 MB aligned PA.
	 */
	return ((pa & 0xff000000) >> 4) | 0x9e;
}

static inline void sun4m_flush_tlb_all(void)
{
	/*
	 * SRMMU flush/probe ASI.  Type 4 is whole-MMU flush.
	 */
	unsigned long addr = 4 << 8;

	__asm__ __volatile__ ("sta %%g0, [%0] 3\n\t" : : "r" (addr));
}

static int sun4m_map_image_window(unsigned long va,
				  unsigned long pa,
				  unsigned long len)
{
	return sun4m_map_l1_window(va, pa, len);
}

static unsigned long sun4m_find_phys_window(unsigned long len)
{
	struct linux_mlist_v0 *mlist;
	unsigned long start, end;

	prom_meminit();

	for (mlist = prom_phys_avail; mlist; mlist = mlist->theres_more) {
		start = (unsigned long)mlist->start_adr;
		end = start + mlist->num_bytes;


		if (start < SUN4M_LOW_AVOID)
			start = SUN4M_LOW_AVOID;

		start = (start + SUN4M_L1_SIZE - 1) & ~(SUN4M_L1_SIZE - 1);

		while (end > start && end - start >= len) {
			if (sun4m_image_pa &&
			    ranges_overlap(start, len,
					   sun4m_image_pa,
					   sun4m_image_len)) {

				start = sun4m_image_pa + sun4m_image_len;
				start = (start + SUN4M_L1_SIZE - 1) &
					~(SUN4M_L1_SIZE - 1);
				continue;
			}

			if (sun4m_kernel_pa &&
			    ranges_overlap(start, len,
					   sun4m_kernel_pa,
					   sun4m_kernel_len)) {
				start = sun4m_kernel_pa + sun4m_kernel_len;
				start = (start + SUN4M_L1_SIZE - 1) &
					~(SUN4M_L1_SIZE - 1);
				continue;
			}

			printf("SILO: sun4m selected image PA 0x%x len 0x%x\n",
			       start, len);
			return start;
		}
	}

	printf("SILO: sun4m no phys window for len 0x%x\n", len);
	return 0;
}


static char *sun4m_image_memory_find(unsigned int len)
{
	unsigned long va = IMAGE_VIRT_ADDR;
	unsigned long pa;
	unsigned long map_len;


	/* Only reuse the cached staging window if it still fits. */
	if (sun4m_image_va) {
		if (sun4m_image_len >= (unsigned long)len + 0x4000)
			return (char *)(sun4m_image_va + 0x4000);
		sun4m_image_release();
	}

	/*
	 * main.c expects image_base - 0x4000 to be the mapped allocation
	 * base.  The actual file load starts at returned pointer.
	 */
	map_len = len + 0x4000;
	map_len = (map_len + SUN4M_L1_SIZE - 1) & ~(SUN4M_L1_SIZE - 1);


	pa = sun4m_find_phys_window(map_len);
	if (!pa) {
		printf("SILO: sun4m: no %d MB image window available\n",
		       map_len >> 20);
		return 0;
	}

	if (sun4m_map_image_window(va, pa, map_len) < 0)
		return 0;

	sun4m_image_pa = pa;
	sun4m_image_va = va;
	sun4m_image_len = map_len;

	return (char *)(va + 0x4000);
}

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

static inline unsigned long sun4m_get_lev1 (void)
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

static inline unsigned long sun4m_probe (unsigned long l)
{
    unsigned long ret;
    
    __asm__ ("\n\t"
        "lda [%1] 3, %0" : "=r" (ret) : "r" (l | 0x400));
    return ret;
}

static inline unsigned long sun4m_get_direct (unsigned long l)
{
    unsigned long ret;
    __asm__ ("\n\t"
	"lda [%1] 32, %0\n\t" : "=r" (ret) : "r" (l));
    return ret;
}

static inline void sun4m_set_direct (unsigned long l, unsigned long set)
{
    __asm__ ("\n\t"
	"sta %0, [%1] 32\n\t" : : "r" (set), "r" (l));
}

unsigned long long initrd_phys;

unsigned long sun4m_initrd_pa;
unsigned long sun4m_initrd_va;

static char *sun4m_map_initrd_window(char *beg)
{
	unsigned long lev1;
	int i;

	sun4m_initrd_pa = (unsigned long)beg;
	initrd_phys = (unsigned long long)(unsigned long)beg;

	lev1 = sun4m_get_lev1();

	for (i = 0x60; i < 0xa0; i++)
		if (!(sun4m_get_direct(lev1 + 4 * i) & 3))
			break;

	if (i == 0xa0)
		return (char *)0;

	sun4m_set_direct(lev1 + 4 * i,
			 ((sun4m_initrd_pa & 0xff000000) >> 4) | 0x9e);

	sun4m_initrd_va = i << 24;

	return (char *)sun4m_initrd_va + (sun4m_initrd_pa & 0xffffff);
}


static char *memory_find_try_region(char **begp, int *lp,
				    char *start, int num, int len)
{
	char *beg = *begp;
	int l = *lp;

	if (num <= 0)
		return (char *)0;

	if (beg && start != beg + l) {
		beg = 0;
		l = 0;
	}

	if (num + (beg ? l : 0) >= len) {
		if (!beg)
			beg = start;

		*begp = beg;
		*lp = l;

		if (architecture == sun4c)
			return beg;

		return sun4m_map_initrd_window(beg);
	}

	if (beg) {
		l += num;
	} else {
		beg = start;
		l = num;
	}

	*begp = beg;
	*lp = l;

	return (char *)0;
}


char *memory_find(int len)
{
	register struct linux_mlist_v0 *mlist;
	char *beg = 0, *start;
	int l = 0, num;
	unsigned long totalmem = 0;
	char *min = (char *)0x300000;

	if (architecture == sun4u)
		return sun4u_memory_find((len + 0x1fff) & ~0x1fff, 0);

	prom_meminit();


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

		ll = (sun4m_probe(0x4000) & 0xffffff00) << 4;
		ll -= 0x4000;
		min += ll;
	}

	mlist = prom_phys_avail;

	for (;;) {
		char *ret;

		start = mlist->start_adr;
		num = mlist->num_bytes;

		if (start <= min) {
			num += start - min;
			start = min;
		}

                /*
                 * On sun4m, SILO may already have reserved private physical
                 * memory for:
                 *
                 *   1. the high staging image buffer, sun4m_image_pa
                 *   2. the final linked-VA kernel ELF mapping, sun4m_kernel_pa
                 *
                 * prom_phys_avail does not know about either reservation.
                 * Do not let the initrd allocator reuse those pages.
                 */
                if (architecture == sun4m && num > 0) {
                        unsigned long seg_start = (unsigned long)start;
                        unsigned long seg_end = seg_start + num;

                        for (;;) {
                                unsigned long res_start = 0;
                                unsigned long res_len = 0;
                                unsigned long res_end;

                                /*
                                 * Prefer skipping the earliest overlapping
                                 * reserved range.  This lets us split a large
                                 * PROM span into:
                                 *
                                 *   usable-before, reserved, usable-after
                                 *
                                 * and then loop again if usable-after also
                                 * overlaps another reserved range.
                                 */
                                if (sun4m_image_pa &&
                                    ranges_overlap(seg_start,
                                                         seg_end - seg_start,
                                                         sun4m_image_pa,
                                                         sun4m_image_len)) {
                                        res_start = sun4m_image_pa;
                                        res_len = sun4m_image_len;
                                }

                                if (sun4m_kernel_pa &&
                                    ranges_overlap(seg_start,
                                                         seg_end - seg_start,
                                                         sun4m_kernel_pa,
                                                         sun4m_kernel_len)) {
                                        if (!res_start ||
                                            sun4m_kernel_pa < res_start) {
                                                res_start = sun4m_kernel_pa;
                                                res_len = sun4m_kernel_len;
                                        }
                                }

                                if (!res_start)
                                        break;

                                res_end = res_start + res_len;


                                /*
                                 * First try the usable part before the
                                 * reserved range.
                                 */
                                if (seg_start < res_start) {
                                        ret = memory_find_try_region(&beg, &l,
                                                                     (char *)seg_start,
                                                                     res_start - seg_start,
                                                                     len);
                                        if (ret)
                                                return ret;
                                }

                                /*
                                 * The reserved range is a hard gap.  Anything
                                 * after it cannot be accumulated with anything
                                 * before it.
                                 */
                                beg = 0;
                                l = 0;

                                /*
                                 * Continue with the part after the reserved
                                 * range.  There may be another reserved range
                                 * later in the same PROM span.
                                 */
                                if (seg_end <= res_end) {
                                        seg_start = seg_end;
                                        break;
                                }

                                seg_start = res_end;
                        }

                        if (seg_end > seg_start) {
                                ret = memory_find_try_region(&beg, &l,
                                                             (char *)seg_start,
                                                             seg_end - seg_start,
                                                             len);
                                if (ret)
                                        return ret;
                        }

                        goto next_mlist;
                }
		ret = memory_find_try_region(&beg, &l, start, num, len);
		if (ret)
			return ret;

next_mlist:
		if (!mlist->theres_more)
			goto not_found;

		mlist = mlist->theres_more;
	}

not_found:
	return (char *)0;
}


static unsigned long long sun4u_image_virt, sun4u_image_len, sun4u_image_phys;
static unsigned long long sun4u_initrd_virt, sun4u_initrd_len;
unsigned long long sun4u_initrd_phys;

static char *sun4u_memory_find (unsigned int len, int is_kernel)
{
	int n, node, i;
	struct p1275_mem {
		unsigned long long phys;
		unsigned long long size;
	} *p = (struct p1275_mem *)0;
	unsigned int virt = (is_kernel ? IMAGE_VIRT_ADDR : INITRD_VIRT_ADDR);
	unsigned long long phys = 0, phys_base;

	p = (struct p1275_mem *)malloc(2048);

	node = prom_finddevice("/memory");

	n = prom_getproplen(node, "available");

	if (!n || n == -1 || prom_getproperty(node, "available", (char *)p, 2048) == -1) {
		free (p);
		printf("Could not get available property\n");
		return (char *)0;
	}

	phys = 0;
        n /= sizeof(*p);

	phys_base = ~(unsigned long long)0;
	for (i = 0; i < n; i++) {
		if (p[i].phys < phys_base)
			phys_base = p[i].phys;
	}

	for (i = 0; i < n; i++) {
		/* Do not mess with first 4 Megs of memory */
		if (p[i].phys == phys_base) {
			if (p[i].size <= 0x400000)
				continue;
			p[i].phys += 0x400000;
			p[i].size -= 0x400000;
		}

		/* Make sure initrd doesn't overwrite kernel */
		if (!is_kernel && p[i].phys == sun4u_image_phys) {
			if (p[i].size <= sun4u_image_len)
				continue;
			p[i].phys += sun4u_image_len;
			p[i].size -= sun4u_image_len;
		}

		/* Make sure initrd phys isn't greater than 32-bits. We
		 * can only pass unsigned int to the kernel for this
		 * location. */
		if (!is_kernel && !initrd_can_do_64bit_phys && p[i].phys >= 0x0000000100000000ULL)
			continue;

		if (p[i].size >= len) {
			phys = p[i].phys;
			break;
		}
	}

	free (p);

	if (!phys) {
		printf("Could not find any available memory\n");
		return (char *)0;
	}

	if (prom_map(PROM_MAP_DEFAULT, (unsigned long long)len, virt, phys) == -1) {
		printf("Could not map memory\n");
		return (char *)0;
	}

	if (is_kernel) {
		sun4u_image_len = len;
		sun4u_image_virt = virt;
		sun4u_image_phys = phys;
		phys += 0x4000ULL;
		virt += 0x4000;
	} else {
		sun4u_initrd_len = len;
		sun4u_initrd_virt = virt;
		initrd_phys = phys;
		/* Not sure what the old kernel crap is for, but it
		 * expects the passed initrd physical to be relative to
		 * the phys memory base. We'll keep compatible with older
		 * kernels to avoid any problems. */
		sun4u_initrd_phys = phys - phys_base;
	}

	return (char *)virt;
}

static void sun4u_memory_release(int is_kernel)
{
	unsigned long long virt, len;

	if (is_kernel) {
		virt = sun4u_image_virt;
		len = sun4u_image_len;
	} else {
		virt = sun4u_initrd_virt;
		len = sun4u_initrd_len;
	}

	if (!len)
		return;


	prom_unmap(len, virt);

	if (is_kernel)
		sun4u_image_len = 0;
	else
		sun4u_initrd_len = 0;
}


char *image_memory_find(unsigned int len)
{
	if (architecture == sun4u)
		return sun4u_memory_find(len, 1);

	if (architecture == sun4m)
		return sun4m_image_memory_find(len);

	return (char *)0;
}

void image_memory_release(void)
{
	if (architecture == sun4m) {
		sun4m_image_release();
		return;
	}

	if (architecture != sun4u)
		return;

	sun4u_memory_release(1);
}

void memory_release(void)
{
    if (architecture == sun4u) {
	    sun4u_memory_release(0);
    } else if (sun4m_initrd_pa) {
	unsigned long lev1;
        lev1 = sun4m_get_lev1();
        sun4m_set_direct(lev1 + (sun4m_initrd_va >> 24) * 4, 0);
    }
}
