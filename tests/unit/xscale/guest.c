// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
//
// Differential test program for the XScale core (core/xscale.cpp).
// Built twice by run-xscale-test.sh: once for the host, once for ARMv5TE
// (ARM state, and again with -mthumb). The ARM builds run on the emulated
// core; every build prints the same lines, and the script diffs them.
// Deterministic integer work only: fixed-width types, no floating point.
#include <stdarg.h>
#include <stdint.h>

#ifdef XS_GUEST
#define UART (*(volatile uint32_t *)0x40100000u)
static void out_char(char c) { UART = (uint8_t)c; }
#else
#include <stdio.h>
static void out_char(char c) { putchar(c); }
#endif

static void out_str(const char *s) { while (*s) out_char(*s++); }
static void out_hex(uint64_t v, int digits) {
	for (int i = digits - 1; i >= 0; i--) out_char("0123456789abcdef"[(v >> (i * 4)) & 15]);
}
static void out_dec(int64_t v) {
	char buf[24]; int n = 0;
	uint64_t u = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
	do { buf[n++] = (char)('0' + u % 10); u /= 10; } while (u);
	if (v < 0) out_char('-');
	while (n) out_char(buf[--n]);
}
// A tiny printf: %x (32-bit), %X (64-bit), %d (32-bit), %D (64-bit), %s.
static void p(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	for (; *fmt; fmt++) {
		if (*fmt != '%') { out_char(*fmt); continue; }
		switch (*++fmt) {
		case 'x': out_hex(va_arg(ap, uint32_t), 8); break;
		case 'X': out_hex(va_arg(ap, uint64_t), 16); break;
		case 'd': out_dec(va_arg(ap, int32_t)); break;
		case 'D': out_dec(va_arg(ap, int64_t)); break;
		case 's': out_str(va_arg(ap, const char *)); break;
		default: out_char(*fmt); break;
		}
	}
	va_end(ap);
}

static uint32_t rng = 0x12345678u;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static uint32_t mix = 0;
static void fold(uint32_t v) { mix = (mix ^ v) * 0x01000193u + (mix >> 7); }

__attribute__((noinline)) static uint32_t fib(uint32_t n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

typedef uint32_t (*binop)(uint32_t, uint32_t);
static uint32_t op_add(uint32_t a, uint32_t b) { return a + b; }
static uint32_t op_sub(uint32_t a, uint32_t b) { return a - b; }
static uint32_t op_xor(uint32_t a, uint32_t b) { return a ^ b; }
static uint32_t op_mul(uint32_t a, uint32_t b) { return a * b; }

__attribute__((noinline)) static uint32_t sw(uint32_t k, uint32_t v) {
	switch (k & 15) {
	case 0: return v + 1; case 1: return v * 3; case 2: return v >> 3; case 3: return ~v;
	case 4: return v ^ 0xdeadbeef; case 5: return v << 5; case 6: return v - 77; case 7: return v | 0x100;
	case 8: return v & 0xff00ff; case 9: return (v >> 16) | (v << 16); case 10: return v * v;
	case 11: return v / 7 + 1; case 12: return v % 13; case 13: return (uint32_t)(-(int32_t)v);
	case 14: return v + k; default: return 0;
	}
}

struct big { uint32_t w[9]; uint8_t b[3]; uint16_t h; };

__attribute__((noinline)) static void copy_big(struct big *d, const struct big *s) { *d = *s; }

static void sort(uint32_t *a, int n) {
	for (int i = 1; i < n; i++) {
		uint32_t v = a[i]; int j = i - 1;
		while (j >= 0 && (int32_t)a[j] > (int32_t)v) { a[j + 1] = a[j]; j--; }
		a[j + 1] = v;
	}
}

static uint32_t crc32(const uint8_t *d, int n) {
	uint32_t c = 0xffffffffu;
	for (int i = 0; i < n; i++) {
		c ^= d[i];
		for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
	}
	return ~c;
}

// Saturating / DSP ops: ARM state uses the instructions, everything else
// computes the architectural result in C.
static int32_t sat(int64_t v) { return v > INT32_MAX ? INT32_MAX : v < INT32_MIN ? INT32_MIN : (int32_t)v; }
#if defined(XS_GUEST) && !defined(__thumb__)
static int32_t do_qadd(int32_t a, int32_t b) { return __builtin_arm_qadd(a, b); }
static int32_t do_qsub(int32_t a, int32_t b) { return __builtin_arm_qsub(a, b); }
static int32_t do_smlabb(int32_t a, int32_t b, int32_t c) { return __builtin_arm_smlabb(a, b, c); }
static int32_t do_smlatt(int32_t a, int32_t b, int32_t c) { return __builtin_arm_smlatt(a, b, c); }
static int32_t do_smulwb(int32_t a, int32_t b) { return __builtin_arm_smulwb(a, b); }
static int32_t do_smlawt(int32_t a, int32_t b, int32_t c) { return __builtin_arm_smlawt(a, b, c); }
static int32_t do_qdadd(int32_t a, int32_t b) {
	int32_t r; __asm__("qdadd %0, %1, %2" : "=r"(r) : "r"(a), "r"(b)); return r;
}
static int64_t do_smlalbt(int64_t acc, int32_t a, int32_t b) {
	uint32_t lo = (uint32_t)acc, hi = (uint32_t)((uint64_t)acc >> 32);
	__asm__("smlalbt %0, %1, %2, %3" : "+r"(lo), "+r"(hi) : "r"(a), "r"(b));
	return (int64_t)(((uint64_t)hi << 32) | lo);
}
#else
static int32_t do_qadd(int32_t a, int32_t b) { return sat((int64_t)a + b); }
static int32_t do_qsub(int32_t a, int32_t b) { return sat((int64_t)a - b); }
static int32_t do_smlabb(int32_t a, int32_t b, int32_t c) { return (int32_t)((uint32_t)((int16_t)a * (int16_t)b) + (uint32_t)c); }
static int32_t do_smlatt(int32_t a, int32_t b, int32_t c) { return (int32_t)((uint32_t)((int16_t)(a >> 16) * (int16_t)(b >> 16)) + (uint32_t)c); }
static int32_t do_smulwb(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * (int16_t)b) >> 16); }
static int32_t do_smlawt(int32_t a, int32_t b, int32_t c) { return (int32_t)((uint32_t)(((int64_t)a * (int16_t)(b >> 16)) >> 16) + (uint32_t)c); }
static int32_t do_qdadd(int32_t a, int32_t b) { return sat((int64_t)a + sat((int64_t)b * 2)); }
static int64_t do_smlalbt(int64_t acc, int32_t a, int32_t b) { return acc + (int64_t)((int16_t)a * (int16_t)(b >> 16)); }
#endif

#if defined(XS_GUEST) && !defined(__thumb__)
// Interworking: these are compiled as Thumb and called from ARM.
__attribute__((target("thumb"), noinline)) static uint32_t thumb_poly(uint32_t x) {
	uint32_t acc = 0;
	for (uint32_t i = 0; i < 16; i++) acc = acc * 31u + (x >> i) + (i * x);
	return acc;
}
__attribute__((target("thumb"), noinline)) static uint32_t thumb_cb(binop f, uint32_t a, uint32_t b) { return f(a, b) + 1; }
#else
__attribute__((noinline)) static uint32_t thumb_poly(uint32_t x) {
	uint32_t acc = 0;
	for (uint32_t i = 0; i < 16; i++) acc = acc * 31u + (x >> i) + (i * x);
	return acc;
}
__attribute__((noinline)) static uint32_t thumb_cb(binop f, uint32_t a, uint32_t b) { return f(a, b) + 1; }
#endif

static uint8_t buf[300];
static uint32_t arr[64];

int main(void) {
	p("xscale differential test\n");

	// 32-bit ALU, shifts, compares
	for (int i = 0; i < 200; i++) {
		uint32_t a = rnd(), b = rnd(), s = rnd() & 31;
		fold(a + b); fold(a - b); fold(a * b); fold(a & ~b); fold(a | b);
		fold(a << s); fold(a >> s); fold((uint32_t)((int32_t)a >> s));
		fold((a >> s) | (a << ((32 - s) & 31)));
		fold((uint32_t)(a < b) + 2u * (uint32_t)((int32_t)a < (int32_t)b) + 4u * (a == b));
		fold(b ? a / b : 0); fold(b ? a % b : 0);
		int32_t sa = (int32_t)a, sb = (int32_t)(b | 1);
		if (!(sa == INT32_MIN && sb == -1)) { fold((uint32_t)(sa / sb)); fold((uint32_t)(sa % sb)); }
		fold((uint32_t)__builtin_clz(a | 1)); fold((uint32_t)__builtin_ctz(b | 0x80000000u));
		fold((uint32_t)__builtin_popcount(a));
		fold(__builtin_bswap32(a));
	}
	p("alu32 %x\n", mix);

	// 64-bit
	mix = 0;
	for (int i = 0; i < 200; i++) {
		uint64_t a = ((uint64_t)rnd() << 32) | rnd(), b = ((uint64_t)rnd() << 32) | rnd();
		uint32_t s = rnd() & 63;
		uint64_t r[8];
		r[0] = a + b; r[1] = a - b; r[2] = a * b; r[3] = a << s; r[4] = a >> s;
		r[5] = (uint64_t)((int64_t)a >> s);
		r[6] = (uint64_t)(uint32_t)a * (uint32_t)b;
		r[7] = (uint64_t)((int64_t)(int32_t)a * (int64_t)(int32_t)b);
		for (int k = 0; k < 8; k++) { fold((uint32_t)r[k]); fold((uint32_t)(r[k] >> 32)); }
		uint32_t sh2 = rnd() & 31;
		uint64_t d = (b >> sh2) | 1;
		fold((uint32_t)(a / d)); fold((uint32_t)(a % d));
		int neg = rnd() & 1;
		int64_t sd = (int64_t)d * (neg ? -1 : 1);
		fold((uint32_t)((int64_t)a / sd)); fold((uint32_t)((int64_t)a % sd));
	}
	p("alu64 %x\n", mix);

	// Memory: bytes, halves, unaligned byte copies, struct copy (LDM/STM)
	mix = 0;
	for (int i = 0; i < 300; i++) buf[i] = (uint8_t)rnd();
	for (int i = 0; i < 290; i++) {
		uint16_t h = (uint16_t)(buf[i] | (buf[i + 1] << 8));
		int16_t sh = (int16_t)h;
		int8_t sb = (int8_t)buf[i];
		fold(h); fold((uint32_t)(int32_t)sh); fold((uint32_t)(int32_t)sb);
	}
	for (int i = 0; i < 100; i++) buf[(i * 7) % 290 + 3] = buf[(i * 13) % 280];
	fold(crc32(buf, 300));
	struct big x, y;
	for (int i = 0; i < 9; i++) x.w[i] = rnd();
	x.b[0] = 1; x.b[1] = 2; x.b[2] = 3; x.h = 0xbeef;
	copy_big(&y, &x);
	for (int i = 0; i < 9; i++) fold(y.w[i]);
	fold(y.b[0] + y.b[1] * 3u + y.b[2] * 5u + y.h);
	p("mem %x\n", mix);

	// Control flow: recursion, jump tables, function pointers, sorting
	mix = 0;
	fold(fib(20));
	p("fib %x\n", mix);
	for (uint32_t k = 0; k < 64; k++) fold(sw(k, rnd()));
	p("switch %x\n", mix);
	binop ops[4] = { op_add, op_sub, op_xor, op_mul };
	for (int i = 0; i < 64; i++) { uint32_t a = rnd(), b = rnd(); fold(ops[i & 3](a, b)); }
	p("fnptr %x\n", mix);
	for (int i = 0; i < 64; i++) arr[i] = rnd();
	sort(arr, 64);
	for (int i = 0; i < 64; i++) fold(arr[i]);
	p("sort %x fib=%d\n", mix, (int32_t)fib(20));

	// DSP extensions
	mix = 0;
	for (int i = 0; i < 100; i++) {
		int32_t a = (int32_t)rnd(), b = (int32_t)rnd(), c = (int32_t)rnd();
		if (i & 1) { a |= 0x7f000000; b |= 0x7f000000; }
		fold((uint32_t)do_qadd(a, b)); fold((uint32_t)do_qsub(a, b));
		fold((uint32_t)do_qdadd(a, b));
		fold((uint32_t)do_smlabb(a, b, c)); fold((uint32_t)do_smlatt(a, b, c));
		fold((uint32_t)do_smulwb(a, b)); fold((uint32_t)do_smlawt(a, b, c));
		int64_t acc = do_smlalbt(((int64_t)c << 20) ^ a, a, b);
		fold((uint32_t)acc); fold((uint32_t)((uint64_t)acc >> 32));
	}
	p("dsp %x\n", mix);

	// Interworking
	mix = 0;
	for (int i = 0; i < 32; i++) { fold(thumb_poly(rnd())); uint32_t a = rnd(), b = rnd(); fold(thumb_cb(ops[i & 3], a, b)); }
	p("interwork %x\n", mix);

	p("done\n");
	return 0;
}
