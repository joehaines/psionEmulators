// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
//
// The run-time helpers a freestanding ARM EABI build calls for division.
// Plain shift-and-subtract; correctness is all that matters. Always built
// in ARM state, so a Thumb guest reaches them through linker-made BLX —
// one more interworking path under test.
#include <stdint.h>

#define ARM_FN __attribute__((target("arm")))

static ARM_FN uint64_t udivmod64(uint64_t n, uint64_t d, uint64_t *rem) {
	uint64_t q = 0, r = 0;
	for (int i = 63; i >= 0; i--) {
		r = (r << 1) | ((n >> i) & 1);
		if (r >= d) { r -= d; q |= (uint64_t)1 << i; }
	}
	*rem = r;
	return q;
}

ARM_FN uint32_t __aeabi_uidiv(uint32_t n, uint32_t d) { uint64_t r; return (uint32_t)udivmod64(n, d, &r); }
ARM_FN int32_t __aeabi_idiv(int32_t n, int32_t d) {
	uint32_t un = n < 0 ? 0u - (uint32_t)n : (uint32_t)n, ud = d < 0 ? 0u - (uint32_t)d : (uint32_t)d;
	uint64_t r; uint32_t q = (uint32_t)udivmod64(un, ud, &r);
	return ((n < 0) != (d < 0)) ? (int32_t)(0u - q) : (int32_t)q;
}
// {quotient, remainder} in r0:r1 is exactly how a uint64_t is returned.
ARM_FN uint64_t xs_uidivmod(uint32_t n, uint32_t d) __asm__("__aeabi_uidivmod");
ARM_FN uint64_t xs_uidivmod(uint32_t n, uint32_t d) {
	uint64_t r; uint32_t q = (uint32_t)udivmod64(n, d, &r);
	return ((uint64_t)(uint32_t)r << 32) | q;
}
ARM_FN uint64_t xs_idivmod(int32_t n, int32_t d) __asm__("__aeabi_idivmod");
ARM_FN uint64_t xs_idivmod(int32_t n, int32_t d) {
	int32_t q = __aeabi_idiv(n, d);
	return ((uint64_t)(uint32_t)(n - q * d) << 32) | (uint32_t)q;
}

// 64-bit: n in r0:r1, d in r2:r3; {q, r} back in r0-r3. The C halves take
// a pointer to a 16-byte buffer the naked wrappers keep on the stack.
ARM_FN void xs_uldivmod_impl(uint64_t n, uint64_t d, uint64_t *out) {
	out[0] = udivmod64(n, d, &out[1]);
}
ARM_FN void xs_ldivmod_impl(int64_t n, int64_t d, uint64_t *out) {
	uint64_t un = n < 0 ? 0 - (uint64_t)n : (uint64_t)n, ud = d < 0 ? 0 - (uint64_t)d : (uint64_t)d;
	uint64_t rem, q = udivmod64(un, ud, &rem);
	out[0] = ((n < 0) != (d < 0)) ? 0 - q : q;
	out[1] = n < 0 ? 0 - rem : rem;
}
#define DIVMOD_WRAPPER(name, impl)                 \
	ARM_FN __attribute__((naked)) void name(void) { \
		__asm__("push {r4, lr}\n"                  \
		        "sub sp, sp, #24\n"                \
		        "add r4, sp, #8\n"                 \
		        "str r4, [sp]\n"                   \
		        "bl " #impl "\n"                   \
		        "ldr r0, [sp, #8]\n"               \
		        "ldr r1, [sp, #12]\n"              \
		        "ldr r2, [sp, #16]\n"              \
		        "ldr r3, [sp, #20]\n"              \
		        "add sp, sp, #24\n"                \
		        "pop {r4, pc}\n");                 \
	}
DIVMOD_WRAPPER(__aeabi_uldivmod, xs_uldivmod_impl)
DIVMOD_WRAPPER(__aeabi_ldivmod, xs_ldivmod_impl)

// 64-bit multiply and shifts (Thumb-1 has no UMULL, so the compiler calls out).
ARM_FN uint64_t __aeabi_lmul(uint64_t a, uint64_t b) {
	uint32_t al = (uint32_t)a, ah = (uint32_t)(a >> 32), bl = (uint32_t)b, bh = (uint32_t)(b >> 32);
	return (uint64_t)al * bl + ((uint64_t)(al * bh + ah * bl) << 32);
}
ARM_FN uint64_t __aeabi_llsl(uint64_t v, int s) {
	uint32_t lo = (uint32_t)v, hi = (uint32_t)(v >> 32);
	if (s == 0) return v;
	if (s >= 32) { hi = lo << (s - 32); lo = 0; }
	else { hi = (hi << s) | (lo >> (32 - s)); lo <<= s; }
	return ((uint64_t)hi << 32) | lo;
}
ARM_FN uint64_t __aeabi_llsr(uint64_t v, int s) {
	uint32_t lo = (uint32_t)v, hi = (uint32_t)(v >> 32);
	if (s == 0) return v;
	if (s >= 32) { lo = hi >> (s - 32); hi = 0; }
	else { lo = (lo >> s) | (hi << (32 - s)); hi >>= s; }
	return ((uint64_t)hi << 32) | lo;
}
ARM_FN int64_t __aeabi_lasr(int64_t v, int s) {
	uint32_t lo = (uint32_t)v; int32_t hi = (int32_t)((uint64_t)v >> 32);
	if (s == 0) return v;
	if (s >= 32) { lo = (uint32_t)(hi >> (s - 32)); hi = hi >> 31; }
	else { lo = (lo >> s) | ((uint32_t)hi << (32 - s)); hi >>= s; }
	return (int64_t)(((uint64_t)(uint32_t)hi << 32) | lo);
}
ARM_FN int __clzsi2(uint32_t v) { return v ? __builtin_clz(v) : 32; }
ARM_FN int __ctzsi2(uint32_t v) { int n = 0; while (n < 32 && !(v & 1)) { v >>= 1; n++; } return n; }
ARM_FN int __popcountsi2(uint32_t v) { int n = 0; while (v) { n += v & 1; v >>= 1; } return n; }

ARM_FN void *memcpy(void *d, const void *s, unsigned n) {
	uint8_t *dd = d; const uint8_t *ss = s;
	while (n--) *dd++ = *ss++;
	return d;
}
ARM_FN void *memset(void *d, int c, unsigned n) {
	uint8_t *dd = d;
	while (n--) *dd++ = (uint8_t)c;
	return d;
}
ARM_FN void __aeabi_memcpy(void *d, const void *s, unsigned n) { memcpy(d, s, n); }
ARM_FN void __aeabi_memcpy4(void *d, const void *s, unsigned n) { memcpy(d, s, n); }
ARM_FN void __aeabi_memcpy8(void *d, const void *s, unsigned n) { memcpy(d, s, n); }
ARM_FN void __aeabi_memclr(void *d, unsigned n) { memset(d, 0, n); }
ARM_FN void __aeabi_memclr4(void *d, unsigned n) { memset(d, 0, n); }
ARM_FN void __aeabi_memclr8(void *d, unsigned n) { memset(d, 0, n); }
