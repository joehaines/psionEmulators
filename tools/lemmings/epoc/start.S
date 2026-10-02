/* SPDX-License-Identifier: LicenseRef-PsionWebEmulator
 * Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
 *
 * Entry point, import thunks and the three kernel calls the game makes
 * itself. See tools/romdump-er1/start.S for what an EPOC32 EXE's entry
 * and import thunks are and why they look like this.
 */
    .syntax unified
    .arm

    .section .text.entry, "ax", %progbits
    .globl _entry
_entry:
    b       game_init_and_run

    .text
.macro IMPORT_THUNK name, slot
    .globl \name
\name:
    ldr     r12, [pc]
    ldr     pc, [r12]
    .word   \slot
.endm
#include "thunks.inc"

/* Exec::WaitForAnyRequest and the timer: User::After is these two with a
 * status word in between (EUser ordinal 17). */
    .globl Exec_WaitForAnyRequest
Exec_WaitForAnyRequest:
    svc     #0xC0004D
    mov     pc, lr
    .globl Exec_After
Exec_After:
    svc     #0xC0005E
    mov     pc, lr

    .section .iat, "a", %progbits
#include "iat.inc"
    .word 0
