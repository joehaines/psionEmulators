/* SPDX-License-Identifier: LicenseRef-PsionWebEmulator
 * Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
 *
 * Entry points for ROMDUMP.APP: the same dumper, packaged as an
 * application so that the System screen starts it the way it starts
 * Word or Agenda, rather than having to recognise an EXE.
 *
 * An EPOC32 application is a DLL (UID2 0x1000006c) that AppRun.exe
 * loads into its own process and asks for ordinal 1, NewApplication —
 * which is meant to return the application object. This one does the
 * whole dump inside that call instead, and returns no object at all:
 * by the time the framework finds out, the ROM is on the disk.
 *
 * The one thing an R1 DLL cannot have is static data. The loader gives
 * a RAM-loaded DLL none — every R1 application in applib/ has
 * iDataSize and iBssSize 0 — and the dumper keeps its log, its report
 * and its read-back buffer in globals. So romdump.c is compiled with
 * -frwpi for this build: every global is addressed as an offset from
 * r9, and NewApplication points r9 at a block on its own stack, zeroed,
 * the size of what would have been .bss. The code that uses it is the
 * code ROMDUMP0.EXE runs, unchanged. r9 is callee-saved in the APCS
 * the ROM was built with, so the ROM's own functions — the file server
 * library, if the dumper ends up calling it — leave it alone.
 *
 * AppRun's main thread has a 12 KB stack (its ROM header, +0x34), so
 * the app build asks romdump.c for smaller buffers; see APP_BUILD there.
 */

    .syntax unified
    .arm

    .section .text.entry, "ax", %progbits
    .globl _entry
    .type _entry, %function
_entry:                         /* E32Dll(TDllReason): nothing to attach */
    mov     r0, #0
    mov     pc, lr
    .size _entry, . - _entry

    .text

    .globl NewApplication
    .type NewApplication, %function
NewApplication:
    stmfd   sp!, {r4-r11, lr}
    mov     r4, sp
    ldr     r5, =__rw_size      /* an absolute size: the same in both links */
    sub     sp, sp, r5
    bic     sp, sp, #7
    mov     r0, sp              /* zero it, as the loader zeroes .bss */
    mov     r1, #0
1:  cmp     r5, #0
    strgt   r1, [r0], #4
    subgt   r5, r5, #4
    bgt     1b
    mov     r9, sp              /* the static base: see romdump_app.ld */
    mov     r0, r4              /* where the caller's stack was, for the log */
    bl      AppStackNote
    bl      RomDumpMain
    mov     sp, r4
    mov     r0, #0              /* no application object: nothing more to run */
    ldmfd   sp!, {r4-r11, pc}
    .size NewApplication, . - NewApplication
    .ltorg

/* The kernel's executive calls, exactly as in start.S. */
    .globl ExecSendReceive
    .type ExecSendReceive, %function
ExecSendReceive:
    svc     #0xC00031
    mov     pc, lr
    .size ExecSendReceive, . - ExecSendReceive

    .globl ExecCreateObject
    .type ExecCreateObject, %function
ExecCreateObject:
    svc     #0xC00076
    mov     pc, lr
    .size ExecCreateObject, . - ExecCreateObject

    .globl ExecWaitForAnyRequest
    .type ExecWaitForAnyRequest, %function
ExecWaitForAnyRequest:
    svc     #0xC0004D
    mov     pc, lr
    .size ExecWaitForAnyRequest, . - ExecWaitForAnyRequest

    .globl ExecRequestSignal
    .type ExecRequestSignal, %function
ExecRequestSignal:
    svc     #0xC0004E
    mov     pc, lr
    .size ExecRequestSignal, . - ExecRequestSignal

    .section .iat, "a", %progbits
                    .word 0     /* no imports: the terminating zero alone */
