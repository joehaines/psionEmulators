// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The part of a C library that Doom leans on, for an EPOC image that links
// against nothing. Small on purpose: the printf is the one Doom's own
// messages need, the files are read-only, and malloc is a first-fit free
// list over a static arena.
//
// An ARM710a (the Series 5's CPU) is an ARMv3: no divide, no long multiply.
// The division helpers at the bottom are shift-and-subtract; the build
// fails (tools/e32/armv3check.mts) if the compiler lets a long multiply
// through.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include "epoc.h"

int errno;

// ── memory and strings ──────────────────────────────────────────────

void *memset(void *d, int c, size_t n)
{
    unsigned char *p = d;
    unsigned v = (unsigned char)c;
    while (n && ((unsigned)p & 3)) { *p++ = (unsigned char)v; n--; }
    if (n >= 4) {
        unsigned w = v | (v << 8) | (v << 16) | (v << 24);
        unsigned *q = (unsigned *)p;
        while (n >= 16) { q[0] = w; q[1] = w; q[2] = w; q[3] = w; q += 4; n -= 16; }
        while (n >= 4) { *q++ = w; n -= 4; }
        p = (unsigned char *)q;
    }
    while (n--) *p++ = (unsigned char)v;
    return d;
}

void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    if ((((unsigned)p ^ (unsigned)q) & 3) == 0) {
        while (n && ((unsigned)p & 3)) { *p++ = *q++; n--; }
        {
            unsigned *pw = (unsigned *)p;
            const unsigned *qw = (const unsigned *)q;
            while (n >= 16) {
                unsigned a = qw[0], b = qw[1], c = qw[2], e = qw[3];
                pw[0] = a; pw[1] = b; pw[2] = c; pw[3] = e;
                pw += 4; qw += 4; n -= 16;
            }
            while (n >= 4) { *pw++ = *qw++; n -= 4; }
            p = (unsigned char *)pw; q = (const unsigned char *)qw;
        }
    }
    while (n--) *p++ = *q++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    if (p == q || n == 0) return d;
    if (p < q || p >= q + n) return memcpy(d, s, n);
    p += n; q += n;
    while (n--) *--p = *--q;
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = a, *q = b;
    while (n--) {
        if (*p != *q) return *p < *q ? -1 : 1;
        p++; q++;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    while (n--) { if (*p == (unsigned char)c) return (void *)p; p++; }
    return 0;
}

size_t strlen(const char *s) { const char *p = s; while (*p) p++; return (size_t)(p - s); }

char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }

char *strncpy(char *d, const char *s, size_t n)
{
    char *r = d;
    while (n && *s) { *d++ = *s++; n--; }
    while (n--) *d++ = 0;
    return r;
}

char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }

char *strncat(char *d, const char *s, size_t n)
{
    char *r = d;
    d += strlen(d);
    while (n-- && *s) *d++ = *s++;
    *d = 0;
    return r;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) { a++; b++; n--; }
    return n ? (unsigned char)*a - (unsigned char)*b : 0;
}

int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int isspace(int c) { return c == ' ' || (c >= 9 && c <= 13); }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isprint(int c) { return c >= 32 && c < 127; }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int ispunct(int c) { return isprint(c) && !isalnum(c) && c != ' '; }

int strcasecmp(const char *a, const char *b)
{
    while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

int strncasecmp(const char *a, const char *b, size_t n)
{
    while (n && *a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; n--; }
    return n ? tolower((unsigned char)*a) - tolower((unsigned char)*b) : 0;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return 0;
    }
}

char *strrchr(const char *s, int c)
{
    const char *r = 0;
    for (;; s++) {
        if (*s == (char)c) r = s;
        if (!*s) return (char *)r;
    }
}

char *strstr(const char *h, const char *n)
{
    size_t l = strlen(n);
    if (!l) return (char *)h;
    for (; *h; h++) if (*h == *n && !strncmp(h, n, l)) return (char *)h;
    return 0;
}

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

char *strerror(int e) { (void)e; return "error"; }

// ── numbers ─────────────────────────────────────────────────────────

int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }

unsigned long strtoul(const char *s, char **end, int base)
{
    unsigned long v = 0;
    int any = 0;
    while (isspace((unsigned char)*s)) s++;
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; base = 16; }
    else if (base == 0) base = s[0] == '0' ? 8 : 10;
    for (;; s++) {
        int d;
        if (isdigit((unsigned char)*s)) d = *s - '0';
        else if (isalpha((unsigned char)*s)) d = tolower((unsigned char)*s) - 'a' + 10;
        else break;
        if (d >= base) break;
        v = v * (unsigned long)base + (unsigned long)d;
        any = 1;
    }
    if (end) *end = (char *)(any ? s : s);
    return v;
}

long strtol(const char *s, char **end, int base)
{
    int neg = 0;
    long v;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    v = (long)strtoul(s, end, base);
    return neg ? -v : v;
}

int atoi(const char *s) { return (int)strtol(s, 0, 10); }
long atol(const char *s) { return strtol(s, 0, 10); }
double atof(const char *s) { (void)s; return 0; }       // no floating point in this port

static unsigned g_rand = 1;
int rand(void) { g_rand = g_rand * 1103515245u + 12345u; return (int)((g_rand >> 16) & 0x7fff); }
void srand(unsigned s) { g_rand = s; }

char *getenv(const char *n) { (void)n; return 0; }
int system(const char *c) { (void)c; return -1; }
int mkdir(const char *p, int m) { (void)p; (void)m; return -1; }
int remove(const char *p) { (void)p; return -1; }
int rename(const char *a, const char *b) { (void)a; (void)b; return -1; }

// Shell sort: not recursive, no allocation.
void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *))
{
    unsigned char *a = base, tmp[64];
    size_t gap, i, j;
    if (sz > sizeof tmp) return;
    for (gap = n / 2; gap > 0; gap /= 2)
        for (i = gap; i < n; i++) {
            memcpy(tmp, a + i * sz, sz);
            for (j = i; j >= gap && cmp(a + (j - gap) * sz, tmp) > 0; j -= gap)
                memcpy(a + j * sz, a + (j - gap) * sz, sz);
            memcpy(a + j * sz, tmp, sz);
        }
}

// ── malloc ──────────────────────────────────────────────────────────

// First fit over one arena; blocks carry an 8-byte header, are 8-aligned,
// and are merged with their neighbours on free.
typedef struct Blk { unsigned size; unsigned used; } Blk;     // size includes the header
static unsigned char g_heap[EPOC_HEAP_BYTES] __attribute__((aligned(8)));
static int g_heap_ready;

static void heap_init(void)
{
    Blk *b = (Blk *)g_heap;
    b->size = EPOC_HEAP_BYTES; b->used = 0;
    g_heap_ready = 1;
}

void *malloc(size_t n)
{
    Blk *b, *end = (Blk *)(g_heap + EPOC_HEAP_BYTES);
    if (!g_heap_ready) heap_init();
    n = (n + 7u + sizeof(Blk)) & ~7u;
    for (b = (Blk *)g_heap; b < end; b = (Blk *)((unsigned char *)b + b->size)) {
        if (b->used || b->size < n) continue;
        if (b->size >= n + 16) {
            Blk *r = (Blk *)((unsigned char *)b + n);
            r->size = b->size - n; r->used = 0;
            b->size = n;
        }
        b->used = 1;
        return b + 1;
    }
    return 0;
}

void free(void *p)
{
    Blk *b, *end = (Blk *)(g_heap + EPOC_HEAP_BYTES);
    if (!p) return;
    ((Blk *)p - 1)->used = 0;
    // merge forward runs of free blocks (a full sweep is cheap: there are few blocks)
    for (b = (Blk *)g_heap; b < end; ) {
        Blk *nx = (Blk *)((unsigned char *)b + b->size);
        if (!b->used && nx < end && !nx->used) b->size += nx->size;
        else b = nx;
    }
}

void *calloc(size_t n, size_t m)
{
    void *p = malloc(n * m);
    if (p) memset(p, 0, n * m);
    return p;
}

void *realloc(void *p, size_t n)
{
    void *q;
    size_t old;
    if (!p) return malloc(n);
    old = ((Blk *)p - 1)->size - sizeof(Blk);
    if (n <= old) return p;
    q = malloc(n);
    if (q) { memcpy(q, p, old); free(p); }
    return q;
}

// ── formatted output ────────────────────────────────────────────────

typedef struct { char *buf; size_t cap, n; } Out;

static void put(Out *o, char c)
{
    if (o->n + 1 < o->cap) o->buf[o->n] = c;
    o->n++;
}

int vsnprintf(char *s, size_t cap, const char *f, va_list ap)
{
    Out o;
    o.buf = s; o.cap = cap; o.n = 0;
    for (; *f; f++) {
        int left = 0, zero = 0, width = 0, prec = -1, lng = 0, plus = 0;
        char c;
        if (*f != '%') { put(&o, *f); continue; }
        f++;
        for (;; f++) {
            if (*f == '-') left = 1;
            else if (*f == '0') zero = 1;
            else if (*f == '+') plus = 1;
            else if (*f == ' ' || *f == '#') {}
            else break;
        }
        if (*f == '*') { width = va_arg(ap, int); if (width < 0) { left = 1; width = -width; } f++; }
        else while (isdigit((unsigned char)*f)) width = width * 10 + (*f++ - '0');
        if (*f == '.') {
            f++; prec = 0;
            if (*f == '*') { prec = va_arg(ap, int); f++; }
            else while (isdigit((unsigned char)*f)) prec = prec * 10 + (*f++ - '0');
        }
        while (*f == 'l' || *f == 'h' || *f == 'z') { if (*f == 'l') lng++; f++; }
        c = *f;
        if (!c) break;
        if (c == '%') { put(&o, '%'); continue; }
        if (c == 'c') {
            int pad = width - 1;
            if (!left) while (pad-- > 0) put(&o, ' ');
            put(&o, (char)va_arg(ap, int));
            if (left) while (pad-- > 0) put(&o, ' ');
        } else if (c == 's') {
            const char *str = va_arg(ap, const char *);
            int len = 0, pad;
            if (!str) str = "(null)";
            while (str[len] && (prec < 0 || len < prec)) len++;
            pad = width - len;
            if (!left) while (pad-- > 0) put(&o, ' ');
            while (len--) put(&o, *str++);
            if (left) while (pad-- > 0) put(&o, ' ');
        } else if (c == 'd' || c == 'i' || c == 'u' || c == 'x' || c == 'X' || c == 'o' || c == 'p') {
            char tmp[34];
            int n = 0, neg = 0, pad, base = 10, total, i;
            unsigned v, r;
            const char *digs = "0123456789abcdef";
            if (c == 'X') digs = "0123456789ABCDEF";
            if (c == 'x' || c == 'X' || c == 'p') base = 16;
            if (c == 'o') base = 8;
            if (c == 'p') { v = (unsigned)va_arg(ap, void *); }
            else if (c == 'd' || c == 'i') {
                int sv = va_arg(ap, int);
                if (sv < 0) { neg = 1; v = (unsigned)-sv; } else v = (unsigned)sv;
            } else v = va_arg(ap, unsigned);
            if (v == 0 && prec != 0) tmp[n++] = '0';
            while (v) { v = udivmod(v, (unsigned)base, &r); tmp[n++] = digs[r]; }
            while (n < prec) tmp[n++] = '0';
            total = n + neg + (plus && !neg && (c == 'd' || c == 'i'));
            pad = width - total;
            if (!left && !zero) while (pad-- > 0) put(&o, ' ');
            if (neg) put(&o, '-'); else if (plus && (c == 'd' || c == 'i')) put(&o, '+');
            if (!left && zero && prec < 0) while (pad-- > 0) put(&o, '0');
            for (i = n - 1; i >= 0; i--) put(&o, tmp[i]);
            if (left) while (pad-- > 0) put(&o, ' ');
        } else {
            // %f and friends: this port has no floating point; show nothing.
            (void)va_arg(ap, int);
        }
    }
    if (cap) o.buf[o.n < cap ? o.n : cap - 1] = 0;
    return (int)o.n;
}

int vsprintf(char *s, const char *f, va_list ap) { return vsnprintf(s, 1u << 30, f, ap); }

int snprintf(char *s, size_t n, const char *f, ...)
{
    va_list ap; int r;
    va_start(ap, f); r = vsnprintf(s, n, f, ap); va_end(ap);
    return r;
}

int sprintf(char *s, const char *f, ...)
{
    va_list ap; int r;
    va_start(ap, f); r = vsnprintf(s, 1u << 30, f, ap); va_end(ap);
    return r;
}

// ── a very small sscanf: %d %i %x %o %s %c, literals, white space ───

int sscanf(const char *s, const char *f, ...)
{
    va_list ap;
    int n = 0;
    va_start(ap, f);
    for (; *f; f++) {
        if (isspace((unsigned char)*f)) { while (isspace((unsigned char)*s)) s++; continue; }
        if (*f != '%') { if (*s != *f) break; s++; continue; }
        f++;
        if (*f == 'd' || *f == 'i' || *f == 'x' || *f == 'o' || *f == 'u') {
            char *end;
            int base = *f == 'd' || *f == 'u' ? 10 : *f == 'x' ? 16 : *f == 'o' ? 8 : 0;
            long v;
            while (isspace((unsigned char)*s)) s++;
            v = strtol(s, &end, base);
            if (end == s) break;
            *va_arg(ap, int *) = (int)v;
            s = end; n++;
        } else if (*f == 's') {
            char *d = va_arg(ap, char *);
            while (isspace((unsigned char)*s)) s++;
            if (!*s) break;
            while (*s && !isspace((unsigned char)*s)) *d++ = *s++;
            *d = 0; n++;
        } else if (*f == 'c') {
            if (!*s) break;
            *va_arg(ap, char *) = *s++; n++;
        } else break;
    }
    va_end(ap);
    return n;
}

// ── stdio ───────────────────────────────────────────────────────────

struct EPOC_FILE {
    int kind;                   // 0 free, 1 file server, 2 in memory, 3 console
    EpocFile ef;
    long pos, size;
    int err, eof;
    const char *mem;
};
static struct EPOC_FILE g_files[10];
static struct EPOC_FILE g_con_out = { 3 };
FILE *epoc_stdout = &g_con_out, *epoc_stderr = &g_con_out, *epoc_stdin = 0;

// A few files Doom looks for are made up here rather than read from the
// machine, so the game starts the way this port wants it to.
static const char g_default_cfg[] =
    "screenblocks 7\n"
    "detaillevel 1\n"
    "mouse_sensitivity 0\n"
    "show_endoom 0\n"
    "sfx_volume 0\n"
    "music_volume 0\n";

FILE *fopen(const char *path, const char *mode)
{
    int i, n = (int)strlen(path);
    struct EPOC_FILE *f = 0;
    if (mode[0] != 'r') return 0;               // read-only
    for (i = 0; i < 10; i++) if (!g_files[i].kind) { f = &g_files[i]; break; }
    if (!f) return 0;
    f->pos = 0; f->err = f->eof = 0;
    if (n >= 4 && !strcasecmp(path + n - 4, ".cfg")) {
        f->kind = 2; f->mem = g_default_cfg; f->size = (long)sizeof g_default_cfg - 1;
        return f;
    }
    if (epoc_file_open(&f->ef, path, &f->size) != 0) return 0;
    f->kind = 1;
    return f;
}

int fclose(FILE *f)
{
    if (!f || f->kind == 3) return 0;
    if (f->kind == 1) epoc_file_close(&f->ef);
    f->kind = 0;
    return 0;
}

size_t fread(void *p, size_t sz, size_t n, FILE *f)
{
    long want = (long)(sz * n), got = 0;
    if (!f || !sz || f->kind == 3 || f->pos >= f->size) { if (f) f->eof = 1; return 0; }
    if (f->pos + want > f->size) want = f->size - f->pos;
    if (f->kind == 2) { memcpy(p, f->mem + f->pos, (size_t)want); got = want; }
    else {
        got = epoc_file_read(&f->ef, f->pos, p, want);
        if (got < 0) { f->err = 1; return 0; }
    }
    f->pos += got;
    if (got < (long)(sz * n)) f->eof = 1;
    return (size_t)got / sz;
}

size_t fwrite(const void *p, size_t sz, size_t n, FILE *f)
{
    if (f && f->kind == 3) { epoc_con_write(p, (int)(sz * n)); return n; }
    return 0;
}

int fseek(FILE *f, long off, int whence)
{
    long p = whence == SEEK_SET ? off : whence == SEEK_CUR ? f->pos + off : f->size + off;
    if (p < 0) return -1;
    f->pos = p; f->eof = 0;
    return 0;
}
long ftell(FILE *f) { return f->pos; }
int fflush(FILE *f) { (void)f; return 0; }
int feof(FILE *f) { return f->eof; }
int ferror(FILE *f) { return f->err; }
void setbuf(FILE *f, char *b) { (void)f; (void)b; }

int fgetc(FILE *f)
{
    unsigned char c;
    return fread(&c, 1, 1, f) == 1 ? c : EOF;
}
int getc(FILE *f) { return fgetc(f); }

char *fgets(char *s, int n, FILE *f)
{
    int i = 0, c;
    while (i < n - 1 && (c = fgetc(f)) != EOF) { s[i++] = (char)c; if (c == '\n') break; }
    if (!i) return 0;
    s[i] = 0;
    return s;
}

int fputc(int c, FILE *f) { char ch = (char)c; return fwrite(&ch, 1, 1, f) ? c : EOF; }
int fputs(const char *s, FILE *f) { return (int)fwrite(s, 1, strlen(s), f); }
int putchar(int c) { return fputc(c, stdout); }
int puts(const char *s) { fputs(s, stdout); fputc('\n', stdout); return 0; }

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    char buf[512];
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    if (n >= (int)sizeof buf) n = (int)sizeof buf - 1;
    fwrite(buf, 1, (size_t)n, f);
    return n;
}
int vprintf(const char *fmt, va_list ap) { return vfprintf(stdout, fmt, ap); }
int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = vfprintf(f, fmt, ap); va_end(ap);
    return r;
}
int printf(const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = vfprintf(stdout, fmt, ap); va_end(ap);
    return r;
}

// ── process ─────────────────────────────────────────────────────────

static void (*g_atexit)(void);
int atexit(void (*f)(void)) { g_atexit = f; return 0; }

void exit(int code)
{
    if (g_atexit) { void (*f)(void) = g_atexit; g_atexit = 0; f(); }
    epoc_exit(code);
    for (;;) {}
}
void abort(void) { exit(134); }

// ── integer division ────────────────────────────────────────────────

static int bitlen(unsigned x)           // x >= 1: the number of bits in x
{
    int n = 0;
    if (x >> 16) { x >>= 16; n += 16; }
    if (x >> 8)  { x >>= 8;  n += 8; }
    if (x >> 4)  { x >>= 4;  n += 4; }
    if (x >> 2)  { x >>= 2;  n += 2; }
    if (x >> 1)  { x >>= 1;  n += 1; }
    return n + 1;
}

unsigned udivmod(unsigned n, unsigned d, unsigned *rem)
{
    unsigned q = 0;
    int sh;
    if (d == 0) { if (rem) *rem = n; return 0xffffffffu; }
    if (n < d) { if (rem) *rem = n; return 0; }
    sh = bitlen(n) - bitlen(d);             // line d up under n's top bit,
    d <<= sh;
    if (d > n) { d >>= 1; sh--; }           // or one place lower if that overshoots
    for (;;) {
        if (n >= d) { n -= d; q |= 1u << sh; }
        if (!sh) break;
        d >>= 1; sh--;
    }
    if (rem) *rem = n;
    return q;
}

unsigned __aeabi_uidiv(unsigned n, unsigned d) { return udivmod(n, d, 0); }

int __aeabi_idiv(int n, int d)
{
    int neg = 0;
    unsigned un, ud, q;
    if (n < 0) { un = (unsigned)-n; neg ^= 1; } else un = (unsigned)n;
    if (d < 0) { ud = (unsigned)-d; neg ^= 1; } else ud = (unsigned)d;
    q = udivmod(un, ud, 0);
    return neg ? -(int)q : (int)q;
}

// __aeabi_uidivmod and __aeabi_idivmod return the quotient in r0 and the
// remainder in r1, which C cannot express; they are in start.S.

void __aeabi_memcpy(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memcpy4(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memcpy8(void *d, const void *s, size_t n) { memcpy(d, s, n); }
void __aeabi_memmove(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memmove4(void *d, const void *s, size_t n) { memmove(d, s, n); }
void __aeabi_memset(void *d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memset4(void *d, size_t n, int c) { memset(d, c, n); }
void __aeabi_memclr(void *d, size_t n) { memset(d, 0, n); }
void __aeabi_memclr4(void *d, size_t n) { memset(d, 0, n); }
void __aeabi_memclr8(void *d, size_t n) { memset(d, 0, n); }
