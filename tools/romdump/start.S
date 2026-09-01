/* SPDX-License-Identifier: LicenseRef-PsionWebEmulator
 * Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
 *
 * Entry point and import thunks for ROMDUMP.EXE.
 *
 * An EPOC32 EXE is entered at offset 0 of its code section and its return
 * value is the process's exit code — that is the whole contract; the
 * kernel has already made the process heap and stack by then. The ROM's
 * own binaries reach it through a compiler-generated stub that walks the
 * static constructor list, calls E32Main and then the destructor list;
 * this program has no static constructors, so the entry is the call.
 *
 * Imports are late-bound through the import address table: each thunk
 * loads the word the loader wrote into its IAT slot and jumps to it. The
 * slot holds the import's ordinal number until the loader replaces it
 * with the address that ordinal resolves to in the target DLL — which is
 * why .iat below reads as a list of ordinals. The thunk shape (and the
 * terminating zero word) is the one the ER5u ROM's own binaries use.
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
    ldr     r12, [pc, #4]
    ldr     r12, [r12]
    bx      r12
    .word   \slot
    .size \name, . - \name
.endm

    /* One thunk per import, in .iat order — which is the order of the
     * import table itself, DLL block by DLL block (see build.sh). */
    IMPORT_THUNK RFile_Close,    iat_rfile_close
    IMPORT_THUNK RFs_Connect,    iat_rfs_connect
    IMPORT_THUNK RFile_Open,     iat_rfile_open
    IMPORT_THUNK RFile_Read,     iat_rfile_read
    IMPORT_THUNK RFile_Replace,  iat_rfile_replace
    IMPORT_THUNK RFile_Write,    iat_rfile_write
    IMPORT_THUNK Console_Getch,  iat_console_getch
    IMPORT_THUNK Console_NewL,   iat_console_newl
    IMPORT_THUNK CleanupStack_New, iat_cleanup_new
    IMPORT_THUNK Console_Printf, iat_console_printf

    .section .iat, "a", %progbits
    /* EFSRV[100039e4].DLL */
iat_rfile_close:    .word 15
iat_rfs_connect:    .word 18
iat_rfile_open:     .word 121
iat_rfile_read:     .word 136
iat_rfile_replace:  .word 151
iat_rfile_write:    .word 194
    /* EUSER[100039e5].DLL */
iat_console_getch:  .word 511
iat_console_newl:   .word 731
iat_cleanup_new:    .word 746
iat_console_printf: .word 837
                    .word 0
