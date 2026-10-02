// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The bits of a C runtime the game leans on, because a Series 5 image
// links against nothing. The compiler will ask for memcpy and memset
// whenever it feels like it, and for the __aeabi division helpers
// whenever a divisor is not a power of two, so those exist.
//
// The division is shift-and-subtract, on purpose: an ARM710a is an
// ARMv3, which has no long multiply, and a compiler that divides by a
// constant does so with one. tools/e32/armv3check.mts fails the build if
// any get through, so a stray "/ 10" in the game is caught rather than
// executed as something else.

#include "rt.h"

void *memset(void *d, int c, unsigned n)
{
    unsigned char *p = (unsigned char *)d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

void *memcpy(void *d, const void *s, unsigned n)
{
    unsigned char *p = (unsigned char *)d;
    const unsigned char *q = (const unsigned char *)s;
    while (n--) *p++ = *q++;
    return d;
}

void *memmove(void *d, const void *s, unsigned n)
{
    unsigned char *p = (unsigned char *)d;
    const unsigned char *q = (const unsigned char *)s;
    if (p < q) {
        while (n--) *p++ = *q++;
    } else {
        p += n; q += n;
        while (n--) *--p = *--q;
    }
    return d;
}

int memcmp(const void *a, const void *b, unsigned n)
{
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    while (n--) {
        if (*p != *q) return *p < *q ? -1 : 1;
        p++; q++;
    }
    return 0;
}

unsigned udivmod(unsigned n, unsigned d, unsigned *rem)
{
    unsigned q = 0, r = 0;
    int i;
    if (d == 0) { if (rem) *rem = n; return 0xffffffffu; }
    for (i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) { r -= d; q |= 1u << i; }
    }
    if (rem) *rem = r;
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

// The two-value returns of the EABI: quotient in r0, remainder in r1.
// Returning a struct of two words puts them there.
typedef struct { unsigned q, r; } UDivMod;
typedef struct { int q, r; } IDivMod;

UDivMod __aeabi_uidivmod(unsigned n, unsigned d)
{
    UDivMod x;
    x.q = udivmod(n, d, &x.r);
    return x;
}

IDivMod __aeabi_idivmod(int n, int d)
{
    IDivMod x;
    x.q = __aeabi_idiv(n, d);
    x.r = n - x.q * d;
    return x;
}
