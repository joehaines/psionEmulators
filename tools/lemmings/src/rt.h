// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#ifndef LEMMINGS_RT_H
#define LEMMINGS_RT_H

void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
void *memmove(void *d, const void *s, unsigned n);
int   memcmp(const void *a, const void *b, unsigned n);

// n / d and n % d without the compiler's help.
unsigned udivmod(unsigned n, unsigned d, unsigned *rem);

#endif
