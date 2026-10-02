//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Fixed point implementation.
//



#include "stdlib.h"

#include "doomtype.h"
#include "i_system.h"

#include "m_fixed.h"




// EPOC port: an ARM710a (ARMv3) has no long multiply and no divide, so the
// 16.16 multiply and divide are written out in 32-bit pieces. The results are
// the same as the 64-bit versions: FixedMul floors (an arithmetic shift of
// the full product) and FixedDiv truncates toward zero.

extern unsigned udivmod(unsigned n, unsigned d, unsigned *rem);

fixed_t FixedMul(fixed_t a, fixed_t b)
{
    // (ah*2^16 + al) * (bh*2^16 + bl) >> 16, with ah and bh signed and al and
    // bl unsigned, is ah*bh*2^16 + ah*bl + al*bh + (al*bl >> 16) exactly -
    // an arithmetic shift of the whole product - and needs no sign handling.
    int ah = a >> 16, bh = b >> 16;
    unsigned al = (unsigned)a & 0xffff, bl = (unsigned)b & 0xffff;
    return (fixed_t)(((unsigned)(ah * bh) << 16) + (unsigned)(ah * (int)bl)
                     + (unsigned)((int)al * bh) + ((al * bl) >> 16));
}

fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    unsigned ua = a < 0 ? (unsigned)-a : (unsigned)a;
    unsigned ub = b < 0 ? (unsigned)-b : (unsigned)b;
    int neg = (a ^ b) < 0;
    unsigned q;

    if ((ua >> 14) >= ub)
    {
        return neg ? INT_MIN : INT_MAX;
    }
    if (ua < 0x10000u)
    {
        q = udivmod(ua << 16, ub, 0);       // the numerator fits in 32 bits
    }
    else
    {
        // (ua << 16) / ub with a 48-bit numerator, a bit at a time
        unsigned rem = ua >> 16, lo = ua << 16;
        int i;
        q = 0;
        for (i = 0; i < 32; i++)
        {
            rem = (rem << 1) | (lo >> 31);
            lo <<= 1;
            q <<= 1;
            if (rem >= ub) { rem -= ub; q |= 1; }
        }
    }
    return neg ? -(fixed_t)q : (fixed_t)q;
}
