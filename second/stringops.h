#ifndef __STRINGOPS_H
#define __STRINGOPS_H

/* common */
#ifndef __SIZE_TYPE__
#define __SIZE_TYPE__ long unsigned int
#endif
#ifndef _LINUX_TYPES_H
typedef __SIZE_TYPE__ size_t;
#endif

/* stringops1.c */
char *strcpy(char *, const char *);
int strcmp(const char *, const char *);
void *memset(void *, int, size_t);
void __bzero(void *, size_t);
void *memcpy(void *, const void *, size_t);

/* stringops2.c */
char *strncpy(char *, const char *, int);
char *strcat(char *, const char *);
char *strncat(char *, const char *, int);
int strncmp(const char *, const char *, int);
char *strchr(const char *, int);
char *strrchr(const char *, int);
int strlen(const char *);
char *strdup(const char *);
int strcasecmp(const char *, const char *);
int strncasecmp(const char *, const char *, size_t);
char *strstr(const char *, const char *);
unsigned long time(void);
void *realloc(void *, int);
int memcmp(const void *, const void *, size_t);

#endif /* __STRINGOPS_H */
