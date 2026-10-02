#ifndef EPOC_STDLIB_H
#define EPOC_STDLIB_H
#include <stddef.h>
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void exit(int code) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int atoi(const char *s);
long atol(const char *s);
double atof(const char *s);
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
int abs(int x);
long labs(long x);
int rand(void);
void srand(unsigned s);
char *getenv(const char *n);
int system(const char *c);
void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *));
int atexit(void (*f)(void));
#endif
