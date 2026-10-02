/* A very small C library for the EPOC Doom port (see libc.c). */
#ifndef EPOC_STDIO_H
#define EPOC_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct EPOC_FILE FILE;
extern FILE *epoc_stdout, *epoc_stderr, *epoc_stdin;
#define stdout epoc_stdout
#define stderr epoc_stderr
#define stdin  epoc_stdin
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define BUFSIZ 512
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
size_t fread(void *p, size_t sz, size_t n, FILE *f);
size_t fwrite(const void *p, size_t sz, size_t n, FILE *f);
int fseek(FILE *f, long off, int whence);
long ftell(FILE *f);
int fflush(FILE *f);
int feof(FILE *f);
int ferror(FILE *f);
int fgetc(FILE *f);
int getc(FILE *f);
char *fgets(char *s, int n, FILE *f);
int fputc(int c, FILE *f);
int fputs(const char *s, FILE *f);
int putchar(int c);
int puts(const char *s);
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int vprintf(const char *fmt, va_list ap);
int sprintf(char *s, const char *fmt, ...);
int snprintf(char *s, size_t n, const char *fmt, ...);
int vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
int vsprintf(char *s, const char *fmt, va_list ap);
int sscanf(const char *s, const char *fmt, ...);
int remove(const char *path);
int rename(const char *a, const char *b);
void setbuf(FILE *f, char *b);
#endif
