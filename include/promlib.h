#ifndef __PROMLIB_H
#define __PROMLIB_H

#include <asm/openprom.h>
#include <stdarg.h>

/*
 * sun4p is a fake architecture, representing IIep based system.
 * Krups has no architecture in the PROM tree.
 * CP-1200 has "sun4s" which is obviously a mistake [don't ask, huh].
 */
enum arch { sun4, sun4c, sun4m, sun4d, sun4e, sun4u, sun4p, sununknown };

extern struct linux_romvec *romvec;

void prom_init(struct linux_romvec *rp, void *cifh, void *cifsp);
void prom_halt(void);
void prom_cmdline(void);
int prom_getchild (int);
int prom_getsibling (int);
int prom_getproplen (int, char *);
int prom_getproperty (int, char *, char *, int);
int prom_getint (int, char *);
int prom_getintdefault (int, char *, int);
int prom_finddevice (char *);
void prom_getstring (int, char *, char *, int);
void prom_chain (unsigned long, int, unsigned long, char *, int);
void prom_reboot (char *command);
int prom_searchsiblings (int, char *);
int prom_setprop (int, char *, char *, int);
void prom_adjust_regs (struct linux_prom_registers *, int,
		       struct linux_prom_ranges *, int);
void prom_adjust_ranges (struct linux_prom_ranges *, int,
		         struct linux_prom_ranges *, int);

void prom_apply_obio_ranges (struct linux_prom_registers *, int);
void prom_ranges_init(void);
void prom_puts (char *, int);
void prom_putchar (char);
char prom_getchar (void);
int prom_nbputchar (char);
int prom_nbgetchar (void);
void prom_printf (char *, ...);
#define printf prom_printf
int p1275_cmd (char *, unsigned, ...);
#define P1275_ARG_64B(x) (1 << ((x) + 8))

/* Map/Unmap client program address ranges.  First the format of
 * the mapping mode argument.
 */
#define PROM_MAP_WRITE	0x0001 /* Writable */
#define PROM_MAP_READ	0x0002 /* Readable - sw */
#define PROM_MAP_EXEC	0x0004 /* Executable - sw */
#define PROM_MAP_LOCKED	0x0010 /* Locked, use i/dtlb load calls for this instead */
#define PROM_MAP_CACHED	0x0020 /* Cacheable in both L1 and L2 caches */
#define PROM_MAP_SE	0x0040 /* Side-Effects */
#define PROM_MAP_GLOB	0x0080 /* Global */
#define PROM_MAP_IE	0x0100 /* Invert-Endianness */
#define PROM_MAP_DEFAULT (PROM_MAP_WRITE | PROM_MAP_READ | PROM_MAP_EXEC | PROM_MAP_CACHED)

int prom_map(int mode, unsigned long long size,
	     unsigned long long vaddr,
	     unsigned long long paddr);
void prom_unmap(unsigned long long size, unsigned long long vaddr);


enum prom_major_version { PROM_V0, PROM_V2, PROM_V3, PROM_P1275 };
extern enum prom_major_version prom_vers;
extern unsigned int prom_rev, prom_prev;
extern int prom_root_node;
extern int prom_stdin, prom_stdout;
extern int prom_chosen;
extern struct linux_nodeops *prom_nodeops;
#endif
