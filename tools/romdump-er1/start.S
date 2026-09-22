/* SPDX-License-Identifier: LicenseRef-PsionWebEmulator
 * Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
 *
 * Entry point and import thunks for the EPOC Release 1 ROMDUMP.EXE.
 *
 * An EPOC32 EXE is entered at offset 0 of its code section and its
 * return value is the process's exit code — that is the whole contract;
 * the kernel has already made the process heap and stack by then. The
 * ROM's own binaries reach it through a compiler-generated stub that
 * walks the static constructor list, calls E32Main and then the
 * destructor list; this program has no static constructors, so the
 * entry is the call.
 *
 * Imports are late-bound through the import address table: each thunk
 * loads the word the loader wrote into its IAT slot and jumps to it.
 * The slot holds the import's ordinal number until the loader replaces
 * it with the address that ordinal resolves to in the target DLL —
 * which is why .iat below reads as a list of ordinals. The thunk shape
 * and the terminating zero word are the ones R1's own binaries use.
 *
 * The jump is "ldr pc, [r12]" and not the "bx r12" its ER5u sibling
 * uses, because this machine's ARM710a is ARMv3: BX arrived with ARMv4T,
 * to interwork with a Thumb that an ARM710a does not have. The ROM's own
 * import stubs are this same pair of loads (see EFSRV at 0x50064cf0 in
 * any R1 ROM), which is where the shape comes from.
 *
 * The ordinals are EFSRV[100000bd].DLL's, the R1 file server client;
 * they are listed in ascending order because that is the order of the
 * import table, and the loader fills IAT slot k from import k.
 *
 * Built with -DNO_IMPORTS there are no thunks and no ordinals at all:
 * the image imports nothing, so there is nothing for the loader to
 * bind and nothing it can fail to find. That build gets its calls the
 * only other way there is — by finding them in the machine's own ROM
 * (see CheckThisMachine in romdump.c) — and is the one to try on a
 * machine the ordinary build will not load on.
 */

    .syntax unified
    .arm

    .section .text.entry, "ax", %progbits
    .globl _entry
    .type _entry, %function
_entry:
    b       RomDumpMain
    .size _entry, . - _entry

    .text

.macro IMPORT_THUNK name, slot
    .globl \name
    .type \name, %function
\name:
    ldr     r12, [pc]           /* the slot address, two words along */
    ldr     pc, [r12]           /* ARMv3: no BX on an ARM710a */
    .word   \slot
    .size \name, . - \name
.endm

#ifndef NO_IMPORTS
    IMPORT_THUNK RFile_Close,   iat_rfile_close
    IMPORT_THUNK RFs_Connect,   iat_rfs_connect
    IMPORT_THUNK RFile_Open,    iat_rfile_open
    IMPORT_THUNK RFile_Read,    iat_rfile_read
    IMPORT_THUNK RFile_Replace, iat_rfile_replace
    IMPORT_THUNK RFile_Write,   iat_rfile_write
#endif

/* The kernel's own executive calls, which is how EUser makes them: a
 * single SVC instruction per call, entered with BL. The handler returns
 * to the caller's lr, not to the instruction after the SVC — which is
 * how EUser can pack its whole table into one word per call (see
 * 0x5004bc3c onwards in a Series 5 ROM, where "svc #0xc00031" is
 * immediately followed by "svc #0x800032", a different call entirely).
 * The "mov pc, lr" after each one costs a word and covers the other
 * convention, in case a prototype's kernel takes it.
 *
 * Read out of the machine's own EUser.dll; see
 * docs/series5-prototype-rom-dumping.md. */
    .globl ExecSendReceive
    .type ExecSendReceive, %function
ExecSendReceive:                    /* (function, args, &status, handle) */
    svc     #0xC00031
    mov     pc, lr
    .size ExecSendReceive, . - ExecSendReceive

    .globl ExecCreateObject
    .type ExecCreateObject, %function
ExecCreateObject:                   /* (selector, args, &status) */
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
ExecRequestSignal:                  /* (count) */
    svc     #0xC0004E
    mov     pc, lr
    .size ExecRequestSignal, . - ExecRequestSignal

    .section .iat, "a", %progbits
#ifndef NO_IMPORTS
    /* EFSRV[100000bd].DLL */
iat_rfile_close:    .word 14    /* RFile::Close() */
iat_rfs_connect:    .word 17    /* RFs::Connect(TInt) */
iat_rfile_open:     .word 105   /* RFile::Open(RFs&, const TDesC8&, TUint) */
iat_rfile_read:     .word 119   /* RFile::Read(TDes8&) */
iat_rfile_replace:  .word 133   /* RFile::Replace(RFs&, const TDesC8&, TUint) */
iat_rfile_write:    .word 170   /* RFile::Write(const TDesC8&) */
#endif
                    .word 0
