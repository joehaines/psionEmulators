/* SPDX-License-Identifier: LicenseRef-PsionWebEmulator
 * Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
 *
 * Entry point, import thunks and the kernel calls the port makes itself.
 * See tools/romdump-er1/start.S for what an EPOC32 EXE's entry and import
 * thunks are and why they look like this.
 */
    .syntax unified
    .arm

    .section .text.entry, "ax", %progbits
    .globl _entry
_entry:
    /* copy the initial values of the writable data from the end of the code
     * section to where the loader left zeroed memory for them */
    ldr     r0, =__data_lma
    ldr     r1, =__data_start
    ldr     r2, =__data_end
1:  cmp     r1, r2
    ldrlo   r3, [r0], #4
    strlo   r3, [r1], #4
    blo     1b
    b       game_init_and_run
    .ltorg

    .text
.macro IMPORT_THUNK name, slot
    .globl \name
\name:
    ldr     r12, [pc]
    ldr     pc, [r12]
    .word   \slot
.endm
#include "thunks.inc"

/* Executive calls, which is how EUser makes them: Exec::WaitForAnyRequest,
 * the timer (User::After is these two with a status word between), and
 * Exec::TickCount, a count of 1/64 s. */
    .globl Exec_WaitForAnyRequest
Exec_WaitForAnyRequest:
    svc     #0xC0004D
    mov     pc, lr
    .globl Exec_After
Exec_After:
    svc     #0xC0005E
    mov     pc, lr
    .globl Exec_TickCount
Exec_TickCount:
    svc     #0x800070
    mov     pc, lr

/* The two-value division helpers of the ARM runtime ABI: quotient in r0,
 * remainder in r1. (udivmod and __aeabi_idiv are in libc.c.) */
    .globl __aeabi_uidivmod
__aeabi_uidivmod:
    stmfd   sp!, {r4, lr}
    sub     sp, sp, #8
    mov     r2, sp
    bl      udivmod
    ldr     r1, [sp]
    add     sp, sp, #8
    ldmfd   sp!, {r4, pc}

    .globl __aeabi_idivmod
__aeabi_idivmod:
    stmfd   sp!, {r4, r5, lr}
    mov     r4, r0
    mov     r5, r1
    bl      __aeabi_idiv
    mul     r2, r0, r5
    sub     r1, r4, r2
    ldmfd   sp!, {r4, r5, pc}

    .section .iat, "a", %progbits
#include "iat.inc"
    .word 0
