// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// What the Doom port's C library and engine glue need from the machine.

#ifndef DOOM_EPOC_H
#define DOOM_EPOC_H

#include <stddef.h>

// Memory. The zone is Doom's own heap (lumps are cached in it); the heap is
// what malloc hands out. Both are static, so the image's size says what it
// needs. The zone is the one worth growing on a machine with room.
#ifndef EPOC_HEAP_BYTES
#define EPOC_HEAP_BYTES (384 * 1024)
#endif
#ifndef EPOC_ZONE_BYTES
#define EPOC_ZONE_BYTES (2048 * 1024)
#endif

// An open file on the file server: an RFile is four words (see
// tools/romdump-er1/romdump.c), kept here without naming EPOC's classes.
typedef struct { int h[4]; } EpocFile;

int  epoc_file_open(EpocFile *f, const char *path, long *size);   // 0 on success
long epoc_file_read(EpocFile *f, long pos, void *buf, long n);    // bytes read, <0 on error
void epoc_file_close(EpocFile *f);

void epoc_con_write(const void *s, int n);      // text for the on-screen console
void epoc_exit(int code) __attribute__((noreturn));

unsigned udivmod(unsigned n, unsigned d, unsigned *rem);

// Doom's side.
extern unsigned char epoc_zone[];
extern const int epoc_zone_size;

#endif
