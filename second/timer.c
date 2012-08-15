/* Timer/counter utilities
   
   Copyright (C) 1996 David S. Miller
                 1996,1997,1998 Jakub Jelinek
   
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
#include <asm/mostek.h>

struct sun4m_timer_regs {
	volatile unsigned int limit;
	volatile unsigned int count;
	volatile unsigned int noclear;
	volatile unsigned int foobar;
	volatile unsigned int cfg;
};

struct sun4c_timer_info {
	volatile unsigned int count10;
	volatile unsigned int limit10;
	volatile unsigned int count14;
	volatile unsigned int limit14;
};

static volatile struct sun4m_timer_regs *sun4m_timer;
static volatile struct sun4c_timer_info *sun4c_timer;
static unsigned char *addr_to_free = 0;
static int len_to_free;
static unsigned long long sun4u_tickcmpr;
static int sun4u_notimer = 0;
static struct mostek48t02 *mregs;
static long clock_frequency;

#define TICKER_VIRTUAL 		0xfc000000
#define SUN4C_TIMER_PHYSADDR	0xf3000000
#define PCIC_PHYSADDR		0x300c0000	/* c0000 Registers */
#define PCIC_SYS_LIMIT		0xb8
#define PCIC_SYS_COUNT		0xbc
#ifndef ASI_M_BYPASS
#define ASI_M_BYPASS 0x20
#endif

static unsigned int swab4(unsigned int n)
{
    return ((n>>24)&0xFF) | ((n>>8)&0xFF00) | ((n&0xFF)<<24) | ((n&0xFF00)<<8);
}

static inline int sun4d_init_timer ()
{
    int node = prom_getchild(prom_searchsiblings(prom_getchild(prom_searchsiblings(prom_getchild(prom_root_node), "cpu-unit")), "bootbus"));
    unsigned int addr[2];

    if (!node)
        return -1;    
    node = prom_searchsiblings(node,"eeprom");
    if (!node)
    	return -1;
    if(prom_getproperty(node, "address", (char *)addr, 2*sizeof(int)) == -1)
    	return -1;
    mregs = (struct mostek48t02 *)(addr[0] + 0x1800);
    return 0;
}

static inline int sun4m_init_timer ()
{
    int reg_count;
    struct linux_prom_registers cnt_regs[PROMREG_MAX];
    int obio_node, cnt_node;
    
    cnt_node = 0;
    if ((obio_node = prom_searchsiblings (prom_getchild (prom_root_node), "obio")) == 0 ||
	(obio_node = prom_getchild (obio_node)) == 0 ||
	(cnt_node = prom_searchsiblings (obio_node, "counter")) == 0) {
	return -1;
    }
    reg_count = prom_getproperty (cnt_node, "reg",
				  (void *) cnt_regs, sizeof (cnt_regs));
    reg_count = (reg_count / sizeof (struct linux_prom_registers));

    prom_apply_obio_ranges (cnt_regs, reg_count);
    
    sun4m_timer = (struct sun4m_timer_regs *)(*romvec->pv_v2devops.v2_dumb_mmap)((char *)TICKER_VIRTUAL, cnt_regs[reg_count-1].which_io, (unsigned)cnt_regs[reg_count-1].phys_addr, cnt_regs[reg_count-1].reg_size);
    if (!sun4m_timer) return -1;
    sun4m_timer->limit = 0x7FFFFFFF;
    addr_to_free = (unsigned char *)sun4m_timer;
    len_to_free = cnt_regs[reg_count-1].reg_size;
    return 0;
}

static inline int sun4c_init_timer ()
{
#if 0
    if (romvec->pv_romvers >= 2) {
        sun4c_timer = (struct sun4c_timer_info *)(*romvec->pv_v2devops.v2_dumb_mmap)((char *)TICKER_VIRTUAL, 0, SUN4C_TIMER_PHYSADDR, sizeof (struct sun4c_timer_info));
        if (!sun4c_timer) return -1;
        addr_to_free = (unsigned char *)sun4c_timer;
        len_to_free = sizeof (struct sun4c_timer_info);
    } else 
#endif
    {
        sun4c_timer = (struct sun4c_timer_info *)TICKER_VIRTUAL;
    	if (sun4c_mapio (SUN4C_TIMER_PHYSADDR, TICKER_VIRTUAL, 0) < 0)
    	    return -1;
        addr_to_free = (unsigned char *)0xffffffff;
    }
    sun4c_timer->limit10 = 0x7FFFFFFF;
    return 0;
}

static int sun4p_init_timer ()
{
    __asm__ __volatile__("sta %2, [%0] %1\n\t": :
        "r" (PCIC_PHYSADDR+PCIC_SYS_LIMIT),
        "i" (ASI_M_BYPASS), "r" (0xFFFFFF7F) : "memory");
    /* Could allocate something here... */
    addr_to_free = 0;
    return 0;
}

static inline int sun4u_init_timer ()
{
    char node_str[128];
    int node, foundcpu, notimer;
    
    addr_to_free = 0; 
    foundcpu = 0;
    notimer = 1;
    node = prom_getchild(prom_root_node);
    while ((node = prom_getsibling(node)) != 0) {
    	if (!foundcpu) {
    	    prom_getstring(node, "device_type", node_str, sizeof(node_str));
            if (!strcmp(node_str, "cpu")) {
                foundcpu = 1;
                clock_frequency = prom_getintdefault(node, "clock-frequency", 0) / 100;
            }
        }
        if (notimer) {
            prom_getstring(node, "name", node_str, sizeof(node_str));
            if (!strcmp(node_str, "counter-timer"))
                notimer = 0;
        }
    }
    if (!foundcpu || !clock_frequency)
        clock_frequency = prom_getint(prom_root_node, "clock-frequency") / 100;
    if (notimer && !sun4v_cpu) {
        sun4u_notimer = 1;
        __asm__ __volatile__ ("\t"
        	"rd	%%tick_cmpr, %%g1\n\t"
        	"stx	%%g1, [%0]\n\t"
        	"mov	1, %%g1\n\t"
        	"sllx    %%g1, 63, %%g1\n\t"
        	"wr      %%g1, 0, %%tick_cmpr"
        	: : "r" (&sun4u_tickcmpr) : "g1");
    }
    return 0;
}

static inline unsigned long mktime(unsigned int year, unsigned int mon,
        unsigned int day, unsigned int hour,
        unsigned int min, unsigned int sec)
{
        if (0 >= (int) (mon -= 2)) {    /* 1..12 -> 11,12,1..10 */
                mon += 12;      /* Puts Feb last since it has leap day */
                year -= 1;
        }
        return (((
            (unsigned long)(year/4 - year/100 + year/400 + 367*mon/12 + day) +
              year*365 - 719499
            )*24 + hour /* now have hours */
           )*60 + min /* now have minutes */
          )*60 + sec; /* finally seconds */
}

int init_timer ()
{
    int retval;
    
    switch (architecture) {
        case sun4c: retval = sun4c_init_timer (); break;
        case sun4m: retval = sun4m_init_timer (); break;
        case sun4d: retval = sun4d_init_timer (); break;
        case sun4p: retval = sun4p_init_timer (); break;
        case sun4u: retval = sun4u_init_timer (); break;
        default: retval = -1; break;
    }  
    if (retval < 0) {
        addr_to_free = 0;
        return -1;
    }
    return 0;
}

void close_timer ()
{
    if (sun4u_notimer) {
        __asm__ __volatile__("\t"
        	"ldx	[%0], %%g1\n\t"
        	"wr	%%g1, 0, %%tick_cmpr"
        	: : "r" (&sun4u_tickcmpr) : "g1");
    }
    if (addr_to_free) {
        if (addr_to_free == (unsigned char *)0xffffffff)
            sun4c_unmapio (TICKER_VIRTUAL);
        else
	    (*romvec->pv_v2devops.v2_dumb_munmap)((char *)addr_to_free, len_to_free);
	addr_to_free = 0;
    }
}

static long long ticks = 0;

void reset_ticks (void)
{
    ticks = 0;
    if (architecture == sun4u) {
        __asm__ __volatile__ ("\t"
        	"rd	%%tick, %%g1\n\t"
        	"stx	%%g1, [%0]\n\t"
        	: : "r" (&ticks) : "g1");
    }
}

static inline unsigned int sun4d_read(void)
{
	unsigned int year, mon, day, hour, min, sec;

	for (;;) {
	        sec = MSTK_REG_SEC(mregs);
        	min = MSTK_REG_MIN(mregs);
	        hour = MSTK_REG_HOUR(mregs);
        	day = MSTK_REG_DOM(mregs);
	        mon = MSTK_REG_MONTH(mregs);
        	year = MSTK_CVT_YEAR( MSTK_REG_YEAR(mregs) );
        	if (MSTK_REG_SEC(mregs) == sec)
        		break;
        }
        return mktime(year, mon, day, hour, min, sec);
}

static unsigned int sun4p_lda(unsigned int addr)
{
    register unsigned int w;
    __asm__ __volatile__("\n\tlda [%1] %2, %0\n\t" : "=r" (w) :
                                "r" (addr), "i" (ASI_M_BYPASS));
    return swab4(w);
}

/* 10ms ticks */
int get_ticks (void)
{
    unsigned int i;
    static unsigned int lasti = 0;
    
    switch (architecture) {
        case sun4c: i = sun4c_timer->count10; break;
        case sun4m: i = sun4m_timer->count; break;
        case sun4p:
		i = sun4p_lda(PCIC_PHYSADDR+PCIC_SYS_COUNT) & 0x7FFFFFFF;
		if (i >= lasti)
			ticks += i - lasti;
		else
			ticks += (0x7FFFFFFF - lasti) + i;
		lasti = i;
		/* 1 increment every 4 CPU clocks (@ 100MHz) */
		return (int) (ticks / 250000);
        case sun4d: /* I cannot get the normal sun4d timer working
		       during bootstrapping, so unfortunately I can give
		       just a 1000ms precision. */
		    if (!lasti) {
			lasti = (sun4d_read() * 100) + 100;
			return 0;
		    }
		    i = sun4d_read() * 100;
		    if (i <= lasti) return 0;
		    return i - lasti;
        case sun4u: 
        	__asm__ __volatile__ ("\t"
        		"ldx	[%2], %%g2\n\t"
        		"rd	%%tick, %%g1\n\t"
        		"sub	%%g1, %%g2, %%g1\n\t"
        		"udivx	%%g1, %1, %0\n\t"
		: "=r" (i) : "r" (clock_frequency), "r" (&ticks) : "g1", "g2");
        	return i;
        default: return 0;
    }
    i >>= 9;
    if (i >= lasti)
        ticks += i - lasti;
    else
        ticks += (1 << 22) + i - lasti - 1;
    lasti = i;
    return (int)(ticks / 20000);
}
