@ SPDX-License-Identifier: LicenseRef-PsionWebEmulator
@ Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
@ Entry for the XScale differential test: stack, bss, main, then report the
@ exit code through the test bus's "exit" register.
	.syntax unified
	.arm
	.section .text.start, "ax"
	.global _start
_start:
	ldr	sp, =0xa0100000
	ldr	r0, =__bss_start
	ldr	r1, =__bss_end
	mov	r2, #0
1:	cmp	r0, r1
	strlo	r2, [r0], #4
	blo	1b
	bl	main
	ldr	r1, =0x40100004
	str	r0, [r1]
2:	b	2b
