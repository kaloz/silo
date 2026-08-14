#ifndef SILO_H
#define SILO_H

#include <promlib.h>

#define CMD_LENG 512



void sun4m_kernel_release(void);
void sun4m_image_release(void);
int sun4m_map_kernel_elf_window(unsigned long min_va,
				unsigned long max_va,
				unsigned long *mapped_va,
				unsigned long *mapped_len);

extern unsigned long _start;

struct aout_hdr {
    int magic;
    int ltext;
    int ldata;
    int lbss;
    int lsym;
    int lentry;
    int x1;
    int x2;
};
typedef struct {
    unsigned char info[128];	/* Informative text string */
    unsigned char spare0[14];
    struct sun_info {
	unsigned char spare1;
	unsigned char id;
	unsigned char spare2;
	unsigned char flags;
    } infos[8];
    unsigned char spare1[246];	/* Boot information etc. */
    unsigned short rspeed;	/* Disk rotational speed */
    unsigned short pcylcount;	/* Physical cylinder count */
    unsigned short sparecyl;	/* extra sects per cylinder */
    unsigned char spare2[4];	/* More magic... */
    unsigned short ilfact;	/* Interleave factor */
    unsigned short ncyl;	/* Data cylinder count */
    unsigned short nacyl;	/* Alt. cylinder count */
    unsigned short ntrks;	/* Tracks per cylinder */
    unsigned short nsect;	/* Sectors per track */
    unsigned char spare3[4];	/* Even more magic... */
    struct sun_partition {
	unsigned int start_cylinder;
	unsigned int num_sectors;
    } partitions[8];
    unsigned short magic;	/* Magic number */
    unsigned short csum;	/* Label xor'd checksum */
} sun_partition;

#define SUN_LABEL_MAGIC          0xDABE

struct silo_inode {
    unsigned int inolen;
    unsigned int mtime;
    unsigned int size;
    unsigned int mode;
    unsigned int uid;
    unsigned int gid;
    unsigned char name[0];
};

/* Options for cmd for load_file */
#define LOADFILE_GZIP		0x01
#define LOADFILE_LS		0x02
#define LOADFILE_MATCH		0x04
#define LOADFILE_QUIET		0x08
#define LOADFILE_NO_ROTATE	0x10

#define LOADFILE_LS_MATCH	(LOADFILE_MATCH | LOADFILE_MATCH)

/* cmdline.c */
void silo_cmdinit(void);
void silo_cmdedit(void (*)(void), int);
extern char cbuff[];
extern char passwdbuff[];
/* ls.c */
#define LSOPT_L	1
#define LSOPT_T 2
#define LSOPT_R 4
extern int ls_opt;
int do_ls (unsigned char *, int *);
/* disk.c */
char *silo_disk_get_bootdevice(void);
int silo_diskinit(void);
int silo_disk_read(char *, int, unsigned long long);
int silo_disk_open(char *);
int silo_disk_setdisk(char *);
int silo_disk_partitionable(void);
void silo_disk_close(void);
/* printf.c */
int vprintf (char *, va_list);
int vsprintf (char *str, char *fmt, va_list adx);
int sprintf (char *s, char *format, ...);
int putchar (int);
/* malloc.c */
void *malloc (int);
void free (void *);
void mark (void **);
void release (void *);
/* file.c */
int silo_load_file(char *, int, char *, unsigned char *,
		   unsigned char *, int *, int,
		   void (*)(int, char **, char **));
/* misc.c */
void silo_fatal(const char *);
char *silo_get_bootargs(int);
void silo_show_bootargs(void);
void silo_set_bootargs(char *, char *);
void silo_set_prollargs(char *, unsigned int, int);
char *silo_v0_device(char *);
enum arch silo_get_architecture(void);
unsigned char *silo_find_linux_HdrS(char *, int);
void print_message(char *);
void get_idprom(void);
char *get_syspackage(void);
char *seed_part_into_device (char *device, int part);
/* cfg.c */
int cfg_parse (char *, char *, int);
char *cfg_get_strg (char *, char *);
int cfg_get_flag (char *, char *);
int cfg_print_images (char *, char *);
char *cfg_get_default (void);
/* strtol.c */
int strtol (const char *, char **, int);
#define atoi(a) strtol(a,0,10)
/* decompress.c */
int decompress (char *, char *, unsigned char (*)(void), void (*)(void));
/* main.c */
extern enum arch architecture;
extern int sun4v_cpu;
/* timer.c */
int init_timer ();
void close_timer ();
int get_ticks (void);
void reset_ticks (void);
/* memory.c */
char *memory_find (int);

extern unsigned long sun4m_kernel_phys_base;
void memory_release (void);
char *image_memory_find (unsigned int len);
void image_memory_release (void);
struct linux_mlist_v0 *prom_meminit (void);
int sun4c_mapio (unsigned long, unsigned long, int);
void sun4c_unmapio (unsigned long);
/* libc */
char *strdup (const char *);
char *strstr (const char *, const char *);
int strcmp (const char *, const char *);
char *strcat (char *, const char *);
#undef tolower
int tolower (int);
int strcasecmp (const char *, const char *);
#endif
