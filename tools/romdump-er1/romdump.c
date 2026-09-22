// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// ROMDUMP for EPOC Release 1 — copy a Series 5 (or Series 5 prototype,
// or Geofox) machine's own ROM out to files on its own disk.
//
// The ROM extractors that circulate — PsiROMx and its kin — are built
// against a later EPOC. That is why they fail here before they run a
// single instruction: an EPOC32 image names the DLLs it imports from by
// UID as well as by name, and on an R1 machine the file server client is
// EFSRV[100000bd].DLL where the later ones are EFSRV[100039e4].DLL. The
// loader looks for a DLL that is not in the ROM, and that is the end of
// it. The ordinals inside differ too.
//
// So this is the same job done against R1's own numbers. Everything it
// needs is six functions from one DLL:
//
//     EFSRV ordinal  14   RFile::Close()
//     EFSRV ordinal  17   RFs::Connect(TInt aMessageSlots)
//     EFSRV ordinal 105   RFile::Open(RFs&, const TDesC8&, TUint)
//     EFSRV ordinal 119   RFile::Read(TDes8&)
//     EFSRV ordinal 133   RFile::Replace(RFs&, const TDesC8&, TUint)
//     EFSRV ordinal 170   RFile::Write(const TDesC8&)
//
// Those numbers were read out of the machine's own ROM rather than an
// SDK — docs/series5-prototype-rom-dumping.md records how each one was
// identified, and that EFSrv.dll is the *same build*, export for export,
// in the Series 5 prototype, the shipping Series 5 and the Geofox. One
// binary therefore fits all three.
//
// There is deliberately nothing else. No console, no cleanup stack, no
// EUser import at all: EUser is the one library that is not identical
// between those ROMs (1699 exports in the prototype against 1701 in the
// shipping machine), and a console also needs Econs.dll to load and the
// window server to be up. None of that can fail if none of it is asked
// for. What the run did is written to ROMDUMP.TXT instead, and rewritten
// after every part, so it can be read on the machine while the dump is
// still going.
//
// The ROM is memory-mapped and readable from user mode at its own base,
// so dumping it is a file write of a descriptor that points into it: no
// driver, no kernel-mode code, nothing device-specific.
//
//     D:\ROMDUMP.001 …   the parts, 1 MB each
//     D:\ROMDUMP.PRG     where the next part starts (32 bytes)
//     D:\ROMDUMP.TXT     what the run did, in plain 8-bit text
//
// D: is tried first — a CompactFlash card holds a whole 6 MB ROM, where
// the internal RAM disk usually cannot — and C: is the fallback. A run
// that fills the disk stops cleanly, and the next run carries on from
// the byte it stopped at, so a machine with room for two parts at a time
// can be emptied and run again.
//
// Every part is read back off the disk and compared with the ROM before
// the dump moves past it, and — the point of doing it that way — a part
// cut short by a full disk is *recorded as short* rather than left
// looking like a whole one. That is the failure this program is built
// around: the ER5u dumper this one is descended from lost 2.8 MB of a
// real machine's ROM to a half-written part whose name said nothing
// about it, and the joined image looked perfect (docs/conan-rom-dumping.md).

typedef unsigned char TUint8;
typedef unsigned int TUint;
typedef int TInt;

// ── The bits of EPOC this needs ─────────────────────────────────────

// A descriptor is a length word whose top nibble is the type, then the
// rest. Both values are R1's own: EUser's TPtrC8(const TText8*) ORs
// 0x10000000 into the length it counts, and its TPtr8 copy constructor
// masks that nibble off and ORs 0x20000000 in.
#define EPtrC 1u
#define EPtr  2u
#define TYPE_LEN(t, n) (((t) << 28) | ((n) & 0x0fffffffu))
#define DES_LEN(x)     ((x) & 0x0fffffffu)

typedef struct { TUint iTypeLength; const void *iPtr; } TPtrC8_;
typedef struct { TUint iTypeLength; TInt iMaxLength; void *iPtr; } TPtr8_;

// TFileMode. EFileWrite is 0x200 — the mode twelve of the ROM's own
// callers of RFile::Replace pass. EFileRead and EFileShareExclusive are
// both zero, which is what the read-back opens with.
#define EFileWrite 0x200u
#define EFileRead  0x000u

#define KErrNone 0

// Private codes, kept well clear of EPOC's own (which run -1 downwards).
#define ERR_MISMATCH  (-1001)
#define ERR_NO_ROM    (-1002)
#define ERR_NO_DRIVE  (-1003)
#define ERR_DISK_FULL (-1004)
#define ERR_NO_CALLS  (-1005)
#define ERR_KERNEL_SILENT (-1006)

// The shapes the machine itself uses, read out of EUser: an
// RSessionBase is three words — the handle, then the delay and count a
// busy server is retried with — and an RSubSessionBase is that plus the
// subsession handle at +12. The file server's own client library writes
// these offsets, and so does the code further down that talks to the
// server without it, so the two agree about what an open file is.
typedef struct { TInt iHandle; TInt iRetryDelay; TInt iRetryCount; TInt iPad; } RFs_;
typedef struct { TInt iHandle; TInt iRetryDelay; TInt iRetryCount;
                 TInt iSubSessionHandle; } RFile_;

// Built the ordinary way, these are the thunks in start.S, and the
// loader binds them by ordinal. Built with -DNO_IMPORTS they do not
// exist: the image imports nothing at all, and every call has to be
// found in the machine's own ROM before anything can happen. That
// build cannot be refused by the loader for want of a DLL, which is
// the one failure the ordinary build cannot talk its way out of.
#ifndef NO_IMPORTS
extern void RFile_Close(RFile_ *aFile);
extern TInt RFs_Connect(RFs_ *aFs, TInt aMessageSlots);
extern TInt RFile_Open(RFile_ *aFile, RFs_ *aFs, const TPtrC8_ *aName, TUint aMode);
extern TInt RFile_Read(RFile_ *aFile, TPtr8_ *aDes);
extern TInt RFile_Replace(RFile_ *aFile, RFs_ *aFs, const TPtrC8_ *aName, TUint aMode);
extern TInt RFile_Write(RFile_ *aFile, const TPtrC8_ *aDes);
#endif

// ── Talking to the file server with nothing but the kernel ──────────

// Everything above this point needs a library in the ROM to make its
// calls. This does not. It is the file server's client side — the part
// EFSRV would do — written out here, over the kernel's own executive
// calls, so that a machine missing that library, or carrying one under
// another name, can still be dumped.
//
// All of it was read out of an R1 machine's EUser.dll (see
// docs/series5-prototype-rom-dumping.md):
//
//   * a message is SVC 0xC00031 with the function in r0, a four-word
//     argument block in r1, a request status in r2 and the session
//     handle in r3; the status completes with the result;
//   * a session is SVC 0xC00076 with selector 22 and a four-word block
//     of descriptors — where to put the new handle, the server's name,
//     the version and message slots, and a second out-parameter — and
//     then a message with function -1, which is the half of connecting
//     the *server* does: the kernel makes the session, and that message
//     is what makes the server build its side of it;
//   * a subsession is an ordinary message whose fourth argument is a
//     modifiable buffer the server writes the new handle into, and
//     every later message on it carries that handle as its fourth
//     argument.
//
// The function codes are the file server's own, and are the same ones
// the library's exports are recognised by above: open 0x1b, replace
// 0x1d, read 0x1f, write 0x20, close a subsession 0x1a.

extern TInt ExecSendReceive(TUint aFunction, TUint *aArgs, TInt *aStatus, TInt aHandle);
extern TInt ExecCreateObject(TInt aSelector, TUint *aArgs, TInt *aStatus);
extern void ExecWaitForAnyRequest(void);
extern void ExecRequestSignal(TInt aCount);

// The value a TRequestStatus holds while its request is outstanding.
// EUser writes it before every send and compares against it in
// User::WaitForRequest; both instructions are "mov/cmp r3, #96, #6",
// which is 0x60 rotated right by six.
#define KRequestPending ((TInt)0x80000001u)
#define EXEC_CREATE_SESSION 22
#define KErrServerBusy (-16)

// A message that is sent and never answered takes the whole program
// with it: there is no log line to write, because the call has not come
// back, and nothing runs afterwards that could write one. So every
// kernel call writes down what it is about to do *before* it does it.
// A RAM snapshot of a machine that stopped still holds this block —
// find the magic below — and it is the only account of such a hang
// there can be. It is what found the one in the history of this file:
// a session created but never connected, stage left at SEND with the
// file server's replace code in it, after which the thread was killed
// inside an SVC that never returned.
#define KSTAGE_IDLE     0
#define KSTAGE_SEND     1   // inside the send executive call
#define KSTAGE_WAIT     2   // inside WaitForAnyRequest
#define KSTAGE_CREATE   3   // inside the create-object executive call
#define KSTAGE_DONE     4   // the last call came back
#define KSTAGE_SENT     5   // the send came back; the answer is still to come

#define KCALL_MAGIC 0x314C434bu   /* 'KCL1' */

// volatile, because a breadcrumb nothing reads is a breadcrumb the
// compiler is entitled to drop — and the one run where it matters is
// the run that never gets far enough to read it.
static volatile struct {
    TUint iMagic;           // so a RAM snapshot can find this block
    TInt  iStage;           // KSTAGE_*, what is in flight right now
    TUint iFunction;        // the file server function code
    TInt  iHandle;          // the session handle it went to
    TInt  iRc;              // what the exec call itself returned
    TInt  iStatus;          // the request status, as last read
    TInt  iWaits;           // WaitForAnyRequest calls that have returned
    TUint iArgs[4];         // the four argument words
} gKCall;

// -1 is not a file server function at all: it is the message every
// EPOC server answers by making the client a session of its own.
#define FS_CONNECT  0xffffffffu
#define FS_OPEN     0x1bu
#define FS_REPLACE  0x1du
#define FS_SUBCLOSE 0x1au
#define FS_READ     0x1fu
#define FS_WRITE    0x20u

// A descriptor type nibble of 3 is EBuf: a modifiable buffer whose
// bytes follow its two header words. It is what the kernel and the
// server write small out-parameters into.
#define EBuf 3u

// User::WaitForRequest, written out. Each WaitForAnyRequest consumes
// one of the thread's request signals, which is not necessarily the one
// this request will complete with: anything else outstanding in the
// process signals the same semaphore. So the surplus is counted and
// handed back, which is exactly what EUser's own loop does before it
// returns.
//
// The spin cap is a stop rather than a policy: a machine that never
// completes would otherwise hang here for ever, and a log that ends at
// the call is more use than that.
static TInt RawWait(volatile TInt *aStatus)
{
    TInt extra = -1;
    while (1) {
        extra++;
        gKCall.iStage = KSTAGE_WAIT;
        gKCall.iStatus = *aStatus;
        gKCall.iWaits = extra;
        ExecWaitForAnyRequest();
        if (*aStatus != KRequestPending) break;
        if (extra > 4000) return ERR_KERNEL_SILENT;
    }
    if (extra) ExecRequestSignal(extra);
    gKCall.iStatus = *aStatus;
    gKCall.iStage = KSTAGE_DONE;
    return *aStatus;
}

// The retry is EUser's: a file server with no free message slot answers
// KErrServerBusy, and the answer to that is to wait and ask again
// rather than to fail the dump. Four slots were asked for at connect,
// so this is the belt to that braces.
static TInt RawSend(TInt aHandle, TUint aFunction, TUint *aArgs)
{
    TInt tries;
    for (tries = 0; tries < 8; tries++) {
        volatile TInt status = KRequestPending;
        TInt err;
        gKCall.iMagic = KCALL_MAGIC;
        gKCall.iStage = KSTAGE_SEND;
        gKCall.iFunction = aFunction;
        gKCall.iHandle = aHandle;
        gKCall.iRc = 0;
        gKCall.iStatus = status;
        gKCall.iWaits = 0;
        gKCall.iArgs[0] = aArgs[0]; gKCall.iArgs[1] = aArgs[1];
        gKCall.iArgs[2] = aArgs[2]; gKCall.iArgs[3] = aArgs[3];
        err = ExecSendReceive(aFunction, aArgs, (TInt *)&status, aHandle);
        gKCall.iStage = KSTAGE_SENT;
        gKCall.iRc = err;
        if (err == KErrServerBusy) continue;  // never waited on: it was not sent
        if (err != KErrNone) { gKCall.iStage = KSTAGE_DONE; return err; }
        return RawWait(&status);
    }
    gKCall.iStage = KSTAGE_DONE;
    return KErrServerBusy;
}

static TInt RawSubSend(RFile_ *aFile, TUint aFunction, TUint a0, TUint a1, TUint a2)
{
    TUint args[4];
    args[0] = a0;
    args[1] = a1;
    args[2] = a2;
    args[3] = (TUint)aFile->iSubSessionHandle;
    return RawSend(aFile->iHandle, aFunction, args);
}

static TInt RawConnect(RFs_ *aFs, TInt aSlots)
{
    static const char kFileServer[] = "FileServer";
    volatile TInt status = KRequestPending;
    TPtrC8_ name;
    TPtr8_ handleDes, extraDes;
    TUint verBuf[4];
    TUint args[4];
    TInt extra = 0, err;

    aFs->iHandle = 0;
    aFs->iRetryDelay = 0;
    aFs->iRetryCount = 0;

    name.iTypeLength = TYPE_LEN(EPtrC, 10u);          // "FileServer"
    name.iPtr = kFileServer;

    // TPtr8(&iHandle, 0, 4): empty, with room for the word the kernel
    // writes. EUser builds both of these the same way, length and all.
    handleDes.iTypeLength = TYPE_LEN(EPtr, 0u);       // where the handle goes
    handleDes.iMaxLength = 4;
    handleDes.iPtr = &aFs->iHandle;
    extraDes.iTypeLength = TYPE_LEN(EPtr, 0u);        // the kernel's second out-parameter
    extraDes.iMaxLength = 4;
    extraDes.iPtr = &extra;

    // TBuf8<8>: the version asked for, then the message slots. A
    // TVersion is { TInt8 iMajor; TInt8 iMinor; TInt16 iBuild }, and
    // 1.0.0 is what every R1 file server is at least.
    verBuf[0] = TYPE_LEN(EBuf, 8u);
    verBuf[1] = 8;
    verBuf[2] = 0x00000001u;
    verBuf[3] = (TUint)aSlots;

    args[0] = (TUint)&handleDes;
    args[1] = (TUint)&name;
    args[2] = (TUint)verBuf;
    args[3] = (TUint)&extraDes;

    gKCall.iMagic = KCALL_MAGIC;
    gKCall.iStage = KSTAGE_CREATE;
    gKCall.iFunction = EXEC_CREATE_SESSION;
    gKCall.iHandle = 0;
    gKCall.iRc = 0;
    gKCall.iStatus = status;
    gKCall.iWaits = 0;
    gKCall.iArgs[0] = args[0]; gKCall.iArgs[1] = args[1];
    gKCall.iArgs[2] = args[2]; gKCall.iArgs[3] = args[3];
    err = ExecCreateObject(EXEC_CREATE_SESSION, args, (TInt *)&status);
    gKCall.iStage = KSTAGE_SENT;
    gKCall.iRc = err;
    if (err != KErrNone) { gKCall.iStage = KSTAGE_DONE; return err; }
    err = RawWait(&status);
    if (err != KErrNone) return err;

    // The retry the library's own sessions carry. Nothing here uses
    // them — RawSend does its own retrying — but a session made here
    // can end up being used by calls found in the ROM, and those do.
    aFs->iRetryDelay = 200000;
    aFs->iRetryCount = 10;

    // Half of connecting is still to do. All the kernel has made is a
    // session; the *server* has not been told about it, and until it is
    // it has nothing to answer with. Function -1 is what tells it: the
    // word the kernel handed back above, and the version that was asked
    // for, sent as an ordinary message. Miss it out and the session
    // looks perfectly good — a handle, no error — right up until the
    // first real request, which the server then answers by killing the
    // thread that sent it. That is a hang with no log and no panic
    // dialog, and it is why this is here.
    {
        TPtrC8_ verDes;
        TUint cargs[4];
        TUint wanted = 0x00000001u;         // the same TVersion, by value
        verDes.iTypeLength = TYPE_LEN(EPtrC, 4u);
        verDes.iPtr = &wanted;
        cargs[0] = (TUint)extra;
        cargs[1] = (TUint)&verDes;
        cargs[2] = 0;
        cargs[3] = 0;
        err = RawSend(aFs->iHandle, FS_CONNECT, cargs);
    }
    return err;
}

static TInt RawCreateSub(RFile_ *aFile, RFs_ *aFs, TUint aFunction,
                         const TPtrC8_ *aName, TUint aMode)
{
    TUint args[4];
    TUint handleBuf[3];          // TBuf8<4>, which the server fills in
    TInt err;

    handleBuf[0] = TYPE_LEN(EBuf, 4u);   // TBuf8<4>, full length: EUser's
    handleBuf[1] = 4;
    handleBuf[2] = 0;

    aFile->iHandle = aFs->iHandle;
    aFile->iRetryDelay = aFs->iRetryDelay;
    aFile->iRetryCount = aFs->iRetryCount;
    aFile->iSubSessionHandle = 0;

    args[0] = (TUint)aName;
    args[1] = aMode;
    args[2] = 0;
    args[3] = (TUint)handleBuf;
    err = RawSend(aFs->iHandle, aFunction, args);
    if (err != KErrNone) return err;
    aFile->iSubSessionHandle = (TInt)handleBuf[2];
    return KErrNone;
}

static TInt RawOpen(RFile_ *aFile, RFs_ *aFs, const TPtrC8_ *aName, TUint aMode)
{
    return RawCreateSub(aFile, aFs, FS_OPEN, aName, aMode);
}

static TInt RawReplace(RFile_ *aFile, RFs_ *aFs, const TPtrC8_ *aName, TUint aMode)
{
    return RawCreateSub(aFile, aFs, FS_REPLACE, aName, aMode);
}

// The same three arguments the library's own Read and Write send, with
// 0x80000000 for "at the current position".
static TInt RawRead(RFile_ *aFile, TPtr8_ *aDes)
{
    return RawSubSend(aFile, FS_READ, (TUint)aDes, (TUint)aDes->iMaxLength, 0x80000000u);
}

static TInt RawWrite(RFile_ *aFile, const TPtrC8_ *aDes)
{
    TUint n = DES_LEN(aDes->iTypeLength);
    if (n == 0) return KErrNone;
    return RawSubSend(aFile, FS_WRITE, (TUint)aDes, n, 0x80000000u);
}

static void RawClose(RFile_ *aFile)
{
    if (!aFile->iSubSessionHandle) return;
    RawSubSend(aFile, FS_SUBCLOSE, 0, 0, 0);
    aFile->iSubSessionHandle = 0;
}

// ── What the program actually calls ─────────────────────────────────

typedef void (*TFnClose)(RFile_ *);
typedef TInt (*TFnConnect)(RFs_ *, TInt);
typedef TInt (*TFnOpen)(RFile_ *, RFs_ *, const TPtrC8_ *, TUint);
typedef TInt (*TFnRead)(RFile_ *, TPtr8_ *);
typedef TInt (*TFnReplace)(RFile_ *, RFs_ *, const TPtrC8_ *, TUint);
typedef TInt (*TFnWrite)(RFile_ *, const TPtrC8_ *);

typedef struct {
    TFnClose   iClose;
    TFnConnect iConnect;
    TFnOpen    iOpen;
    TFnRead    iRead;
    TFnReplace iReplace;
    TFnWrite   iWrite;
} TFsCalls;

// What the program calls. Nothing writes to it while the check below is
// running: that resolves into a copy and publishes it in one go, so the
// logging the check does itself still goes through calls that are known
// to work.
static TFsCalls gFs;

// What the machine's own ROM turned out to offer. Filled at most once,
// by RomCallsOnce below: the check reads the ROM and writes a good deal
// of the log, and there is no sense doing either twice.
static TFsCalls gRomCalls;
static TInt gRomCallsDone;

// Where a run starts from: the loader's bindings if there are any, and
// nothing at all if the image has no imports.
static void StartingCalls(TFsCalls *aOut)
{
#ifdef NO_IMPORTS
    aOut->iClose = 0; aOut->iConnect = 0; aOut->iOpen = 0;
    aOut->iRead = 0; aOut->iReplace = 0; aOut->iWrite = 0;
#else
    aOut->iClose = RFile_Close;
    aOut->iConnect = RFs_Connect;
    aOut->iOpen = RFile_Open;
    aOut->iRead = RFile_Read;
    aOut->iReplace = RFile_Replace;
    aOut->iWrite = RFile_Write;
#endif
}

// The set that needs nothing from the ROM but the kernel underneath it.
static void KernelCalls(TFsCalls *aOut)
{
    aOut->iClose = RawClose;
    aOut->iConnect = RawConnect;
    aOut->iOpen = RawOpen;
    aOut->iRead = RawRead;
    aOut->iReplace = RawReplace;
    aOut->iWrite = RawWrite;
}

static void CopyCalls(TFsCalls *aTo, const TFsCalls *aFrom)
{
    aTo->iClose = aFrom->iClose;
    aTo->iConnect = aFrom->iConnect;
    aTo->iOpen = aFrom->iOpen;
    aTo->iRead = aFrom->iRead;
    aTo->iReplace = aFrom->iReplace;
    aTo->iWrite = aFrom->iWrite;
}

// The ordinals this program was linked against, for the log to compare
// with what the machine says.
#define ORD_CLOSE    14
#define ORD_CONNECT  17
#define ORD_OPEN    105
#define ORD_READ    119
#define ORD_REPLACE 133
#define ORD_WRITE   170


// ── Tunables ────────────────────────────────────────────────────────

// Where the ROM is mapped. Every EPOC32 ARM machine puts it here; the
// header found there is what the base and length are actually read
// from, so a machine with a shorter ROM dumps only what it has.
#define ROM_LINEAR_BASE 0x50000000u

// Stamped into the log, so a log sent back from a machine names the
// binary that wrote it. build.sh passes -DBUILD_ID.
#ifndef BUILD_ID
#define BUILD_ID "unstamped"
#endif

// One part per file. 1 MB is small enough that a RAM disk with very
// little free space still gets somewhere, and big enough that a 6 MB
// ROM is six files.
#define PART_BYTES (1u * 1024u * 1024u)

// Bytes per RFile::Write. The file server copies this out of our address
// space in one transfer.
#define CHUNK_BYTES 0x8000u

// Bytes per RFile::Read on the read-back pass. In .bss, not on the
// stack: the stack is the 8 KB every R1 binary asks for.
#define VERIFY_BYTES 0x2000u

// Enough parts for a 64 MB ROM at 1 MB each, and the name has room for
// three digits.
#define MAX_PARTS 999

// Drives to try, best first. D: is the CompactFlash / SSD slot, which
// is the only place a whole ROM is likely to fit; C: is the internal
// RAM disk. A machine without the drive simply fails the probe.
static const char KDrives[] = "DC";

// ── What the run did ────────────────────────────────────────────────

// Kept in one place so the report can be written from it at any point,
// and left in memory with a magic in front so a RAM snapshot taken by
// the emulator harness can find it — with no console, that is the only
// way a test can see inside a running process.
#define RESULT_MAGIC 0x3152444du   /* 'MDR1' */

static struct {
    TUint iMagic;
    TInt  iStep;
    TInt  iError;
    TUint iRomBase;
    TUint iRomSize;
    TUint iStartOffset;     // where this run started
    TUint iOffset;          // where the dump has got to, over all runs
    TInt  iNextPart;
    TInt  iPartsThisRun;
    TUint iBytesThisRun;
    TUint iBadAt;           // ROM address of the first byte that read back wrong
    TInt  iDrive;           // the drive letter in use

} gResult;

#define STEP_START     1
#define STEP_ROM_FOUND 2
#define STEP_CONNECTED 3
#define STEP_DRIVE     4
#define STEP_WRITING   5
#define STEP_VERIFYING 6
#define STEP_DONE      7

// ── Text ────────────────────────────────────────────────────────────

// R1 is a non-Unicode build: its text is 8-bit, so the report is 8-bit
// and the machine's own editors can read it.

// An ARM710a is ARMv3: it has MUL, but not the 64-bit multiplies that
// arrived with ARMv3M, and a compiler turns every division by a constant
// into one of those. Worse, the machine does not refuse the instruction —
// the encoding lands in data-processing space and it quietly executes
// something else (an SBC, as it happens), so the division returns
// nonsense and the program carries the nonsense until it falls over.
// That is what made the first build of this program panic with
// KERN-EXEC 3 on the machine.
//
// So the two places this program divides — turning numbers into decimal
// digits — do it in shifts and adds instead. This is the usual
// divide-by-ten: about three quarters of the value, folded down, then
// shifted, and corrected once. tools/e32/armv3check.mts fails the build
// if a long multiply (or a BX, or a halfword load) reaches the binary
// anyway.
static TUint DivTen(TUint aValue)
{
    TUint q = (aValue >> 1) + (aValue >> 2);
    TUint r;
    q += q >> 4;
    q += q >> 8;
    q += q >> 16;
    q >>= 3;
    r = aValue - (((q << 2) + q) << 1);       /* aValue - 10q */
    return q + (r > 9u);
}

static TUint ModTen(TUint aValue)
{
    TUint q = DivTen(aValue);
    return aValue - (((q << 2) + q) << 1);
}

static TInt AppendText(char *aBuf, TInt aAt, const char *aText)
{
    while (*aText) aBuf[aAt++] = *aText++;
    return aAt;
}

static TInt AppendHex(char *aBuf, TInt aAt, TUint aValue, TInt aDigits)
{
    static const char kHex[] = "0123456789ABCDEF";
    TInt i;
    for (i = aDigits - 1; i >= 0; i--) aBuf[aAt++] = kHex[(aValue >> (i * 4)) & 0xf];
    return aAt;
}

static TInt AppendDec(char *aBuf, TInt aAt, TUint aValue)
{
    char tmp[12];
    TInt n = 0;
    do { tmp[n++] = (char)('0' + ModTen(aValue)); aValue = DivTen(aValue); } while (aValue);
    while (n) aBuf[aAt++] = tmp[--n];
    return aAt;
}

// ── The ROM header ──────────────────────────────────────────────────

// TRomHeader, at the base of the image: +0x8c iRomBase, +0x90 iRomSize,
// +0x94 iRomRootDirectoryList. R1 puts them at the same offsets as
// every later release. Checking all three is what stops a machine whose
// header is somewhere else from dumping nonsense: 0x50000000 is always
// mapped and always readable, so reading it is safe, but believing it
// without checking is not.
#define ROMHDR_BASE 0x8c
#define ROMHDR_SIZE 0x90
#define ROMHDR_ROOT 0x94

static TUint RomWord(TUint aAddr)
{
    return *(volatile const TUint *)aAddr;
}

static TInt FindRom(TUint *aBase, TUint *aSize)
{
    TUint base = RomWord(ROM_LINEAR_BASE + ROMHDR_BASE);
    TUint size = RomWord(ROM_LINEAR_BASE + ROMHDR_SIZE);
    TUint root = RomWord(ROM_LINEAR_BASE + ROMHDR_ROOT);
    if (base != ROM_LINEAR_BASE) return ERR_NO_ROM;
    if (size < 0x00100000u || size > 0x04000000u) return ERR_NO_ROM;
    if (size & 0xfffu) return ERR_NO_ROM;
    if (root < base || root >= base + size) return ERR_NO_ROM;
    *aBase = base;
    *aSize = size;
    return KErrNone;
}

// ── File names ──────────────────────────────────────────────────────

// Built into a caller-owned buffer rather than edited in place in a
// constant, because there is no writable initialised data in this image
// (see romdump.ld) and there does not need to be.

#define NAME_MAX 20

static void MakeName(char *aBuf, TPtrC8_ *aDes, char aDrive, const char *aLeaf)
{
    TInt at = 0;
    aBuf[at++] = aDrive;
    aBuf[at++] = ':';
    aBuf[at++] = '\\';
    at = AppendText(aBuf, at, aLeaf);
    aDes->iTypeLength = TYPE_LEN(EPtrC, (TUint)at);
    aDes->iPtr = aBuf;
}

static TInt AppendPartDigits(char *aBuf, TInt aAt, TInt aPart)
{
    TUint n = (TUint)aPart;
    TUint tens = DivTen(n);
    TUint hundreds = DivTen(tens);
    aBuf[aAt++] = (char)('0' + ModTen(hundreds));
    aBuf[aAt++] = (char)('0' + ModTen(tens));
    aBuf[aAt++] = (char)('0' + ModTen(n));
    return aAt;
}

static void MakePartName(char *aBuf, TPtrC8_ *aDes, char aDrive, TInt aPart)
{
    TInt at;
    MakeName(aBuf, aDes, aDrive, "ROMDUMP.");
    at = AppendPartDigits(aBuf, (TInt)DES_LEN(aDes->iTypeLength), aPart);
    aDes->iTypeLength = TYPE_LEN(EPtrC, (TUint)at);
}

// ── Whole-file helpers ──────────────────────────────────────────────

static TInt WriteWholeFile(RFs_ *aFs, const TPtrC8_ *aName, const void *aData, TUint aLen)
{
    RFile_ f;
    TPtrC8_ des;
    TInt err;

    f.iHandle = 0; f.iSubSessionHandle = 0;
    err = gFs.iReplace(&f, aFs, aName, EFileWrite);
    if (err != KErrNone) return err;
    des.iTypeLength = TYPE_LEN(EPtrC, aLen);
    des.iPtr = aData;
    err = gFs.iWrite(&f, &des);
    gFs.iClose(&f);
    return err;
}

static TInt ReadWholeFile(RFs_ *aFs, const TPtrC8_ *aName, void *aBuf, TInt aMax, TUint *aGot)
{
    RFile_ f;
    TPtr8_ des;
    TInt err;

    f.iHandle = 0; f.iSubSessionHandle = 0;
    err = gFs.iOpen(&f, aFs, aName, EFileRead);
    if (err != KErrNone) return err;
    des.iTypeLength = TYPE_LEN(EPtr, 0);
    des.iMaxLength = aMax;
    des.iPtr = aBuf;
    err = gFs.iRead(&f, &des);
    gFs.iClose(&f);
    if (err != KErrNone) return err;
    *aGot = DES_LEN(des.iTypeLength);
    return KErrNone;
}

// ── The log ─────────────────────────────────────────────────────────

// A panic takes the process's memory with it, so anything this program
// wants to be able to say afterwards has to be on the disk before it
// happens. ROMDUMP.LOG is rewritten from a buffer after every line, so
// whatever went wrong, the last line of the log is where it went wrong.
//
// It is always written at its full length, padded with spaces. A file
// that already occupies its clusters can be rewritten on a disk with
// nothing left on it; one that has to grow cannot, and a log that
// disappears exactly when the disk fills up would be worse than no log
// at all.
#define LOG_BYTES 8192

static char gLog[LOG_BYTES];
static TInt gLogAt;
static TInt gLogLost;            // lines dropped because the buffer filled
static char gLogDrive;           // 0 until a drive has taken a log
static RFs_ *gLogFs;

static void LogFlush(void)
{
    char nameBuf[NAME_MAX];
    TPtrC8_ name;
    TInt i;
    if (!gLogDrive || !gLogFs) return;
    for (i = gLogAt; i < LOG_BYTES; i++) gLog[i] = ' ';
    MakeName(nameBuf, &name, gLogDrive, "ROMDUMP.LOG");
    WriteWholeFile(gLogFs, &name, gLog, (TUint)LOG_BYTES);
}

static void LogCh(char aCh)
{
    if (gLogAt >= LOG_BYTES - 64) { gLogLost++; return; }
    gLog[gLogAt++] = aCh;
}

static void LogText(const char *aText)
{
    while (*aText) LogCh(*aText++);
}

static void LogHex(TUint aValue, TInt aDigits)
{
    static const char kHex[] = "0123456789ABCDEF";
    TInt i;
    for (i = aDigits - 1; i >= 0; i--) LogCh(kHex[(aValue >> (i * 4)) & 0xf]);
}

static void LogDec(TUint aValue)
{
    char tmp[12];
    TInt n = 0;
    do { tmp[n++] = (char)('0' + ModTen(aValue)); aValue = DivTen(aValue); } while (aValue);
    while (n) LogCh(tmp[--n]);
}

// File server error codes are negative and are the single most useful
// thing in the log, so they are written as EPOC writes them.
static void LogErr(TInt aValue)
{
    if (aValue < 0) { LogCh('-'); LogDec((TUint)(-aValue)); }
    else LogDec((TUint)aValue);
}

static void LogEol(void)
{
    LogCh('\r');
    LogCh('\n');
    LogFlush();
}

// Everything the log says about a call: what was asked, and what came
// back. Called after each one, so the last line names the call that did
// not return.
static void LogCall(const char *aWhat, TInt aErr)
{
    LogText(aWhat);
    LogText(" rc=");
    LogErr(aErr);
    LogEol();
}

// ── The machine's own ROM, read as a directory ──────────────────────

// Everything below reads the ROM through this, which refuses an address
// outside the image. A prototype whose directory is laid out
// differently would otherwise walk this program straight into a data
// abort — the very failure the log exists to explain.
static TUint gRomBase, gRomEnd;

static TInt RomOk(TUint aAddr, TUint aLen)
{
    return gRomEnd && aAddr >= gRomBase && aAddr + aLen > aAddr && aAddr + aLen <= gRomEnd;
}

static TUint RomWordAt(TUint aAddr, TInt *aOk)
{
    if (!RomOk(aAddr, 4)) { *aOk = 0; return 0; }
    return *(volatile const TUint *)aAddr;
}

static TUint8 RomByteAt(TUint aAddr, TInt *aOk)
{
    if (!RomOk(aAddr, 1)) { *aOk = 0; return 0; }
    return *(volatile const TUint8 *)aAddr;
}

static char Lower(char aCh)
{
    return (aCh >= 'A' && aCh <= 'Z') ? (char)(aCh + 32) : aCh;
}

// A TRomEntry's name against a leaf name, without case.
static TInt NameIs(TUint aNameAddr, TInt aLen, const char *aWant)
{
    TInt i, ok = 1;
    for (i = 0; i < aLen; i++) {
        char c = (char)RomByteAt(aNameAddr + (TUint)i, &ok);
        if (!ok || !aWant[i] || Lower(c) != Lower(aWant[i])) return 0;
    }
    return aWant[aLen] == 0;
}

// Walks the ROM's directory tree for a file, by leaf name. The tree is
// { TInt iSize; TLinAddr iAddressLin; TUint8 iAtt; TUint8 iNameLength;
// TText iName[] } records, word-aligned, 0x10 in iAtt marking a
// directory, with 8-bit names — R1's layout (tools/e32/romfs1.mts reads
// the same thing on a PC).
#define ROM_WALK_DEPTH 8

static TInt RomFindFile(const char *aLeaf, TUint *aAddr, TUint *aSize)
{
    TUint stack[ROM_WALK_DEPTH];
    TUint ends[ROM_WALK_DEPTH];
    TInt sp = 0, ok = 1;
    TUint root = RomWordAt(gRomBase + ROMHDR_ROOT, &ok);
    if (!ok || !RomOk(root, 4)) return 0;
    stack[0] = root + 4;
    ends[0] = root + RomWordAt(root, &ok);
    if (!ok) return 0;
    sp = 1;

    while (sp > 0) {
        TUint p = stack[sp - 1], end = ends[sp - 1];
        if (p + 10 > end || !RomOk(p, 10)) { sp--; continue; }
        {
            TUint size = RomWordAt(p, &ok);
            TUint addr = RomWordAt(p + 4, &ok);
            TUint8 att = RomByteAt(p + 8, &ok);
            TUint8 len = RomByteAt(p + 9, &ok);
            if (!ok || len == 0) { sp--; continue; }
            stack[sp - 1] = p + ((10u + len + 3u) & ~3u);
            if (att & 0x10) {
                if (sp < ROM_WALK_DEPTH && RomOk(addr, 4)) {
                    stack[sp] = addr + 4;
                    ends[sp] = addr + RomWordAt(addr, &ok);
                    if (ok) sp++;
                }
            } else if (NameIs(p + 10, len, aLeaf)) {
                *aAddr = addr;
                *aSize = size;
                return 1;
            }
        }
    }
    return 0;
}

// ── Which export is which, on this machine ──────────────────────────

// The six calls this program makes are EFSRV ordinals, and the numbers
// were read out of three R1 ROMs — every one of which carries the same
// EFSrv.dll, export for export. A prototype need not. So rather than
// trust the numbers, the program looks them up on the machine it is
// running on: it finds EFSrv.dll in the ROM, walks its export table,
// and recognises each call by what only that call does — the function
// code it sends the file server, or the literal it loads. What it finds
// is what it calls; what it was linked against is only the fallback,
// and the log says which is which.
//
// This is safe to do because an R1 ROM's EFSrv.dll has no data and no
// bss (its ROM image header says so, and its import table is resolved
// in ROM at build time): the code is complete where it stands.

// TRomImageHeader, R1's: uid1/2/3 at 0, code address at +0x14, code
// size at +0x1c, export count at +0x3c, export table at +0x40.
#define RIMG_UID1  0x00
#define RIMG_UID3  0x08
#define RIMG_CODE  0x14
#define RIMG_CSIZE 0x1c
#define RIMG_NEXP  0x3c
#define RIMG_EXP   0x40

#define KEfsrvUid3      0x100000bdu
#define KDynamicLibUid  0x10000079u

#define FP_WORDS 48              // an export body is far shorter than this

typedef struct {
    TUint iFirst;        // the body's first word, if it is distinctive
    TUint iNeed[3];      // words that must appear
    TUint iTwice;        // a word that must appear at least twice
    TUint iForbid;       // a word that must not appear
} TPrint;

static TUint gExpDir;
static TInt  gExpCount;
static TUint gEfsrvCode, gEfsrvEnd;

// Where an export's body ends: the next export along, in address
// order. Without this a fingerprint reads on into the next function —
// and the machine said so, because RFile::Read's asynchronous twin sits
// immediately after it and carries the very instruction Read's
// fingerprint forbids.
static TUint NextExportAddr(TUint aAddr)
{
    TInt ord, ok = 1;
    TUint best = gEfsrvEnd;
    for (ord = 1; ord <= gExpCount; ord++) {
        TUint a = RomWordAt(gExpDir + (TUint)(ord - 1) * 4u, &ok);
        if (!ok) break;
        if (a > aAddr && a < best) best = a;
    }
    return best;
}

static TUint ExportAddr(TInt aOrdinal)
{
    TInt ok = 1;
    TUint a;
    if (aOrdinal < 1 || aOrdinal > gExpCount) return 0;
    a = RomWordAt(gExpDir + (TUint)(aOrdinal - 1) * 4u, &ok);
    if (!ok || a < gEfsrvCode || a >= gEfsrvEnd) return 0;
    return a;
}

static TInt Matches(TUint aAddr, const TPrint *aPrint)
{
    TInt i, k, ok = 1, need = 0, got = 0, twice = 0;
    TUint end;
    if (!aAddr) return 0;
    end = NextExportAddr(aAddr);
    if (aPrint->iFirst && RomWordAt(aAddr, &ok) != aPrint->iFirst) return 0;
    // Which of the needed words have been seen, not how many words
    // matched something: one of them appearing three times is not three
    // of them appearing. (It was, once, and the machine said so —
    // RFile::Open matched the fingerprint for RFile::Read.)
    for (k = 0; k < 3; k++) if (aPrint->iNeed[k]) need |= 1 << k;
    for (i = 0; i < FP_WORDS; i++) {
        TUint at = aAddr + (TUint)i * 4u;
        TUint w;
        if (at >= end) break;
        w = RomWordAt(at, &ok);
        if (!ok) break;
        if (aPrint->iForbid && w == aPrint->iForbid) return 0;
        if (aPrint->iTwice && w == aPrint->iTwice) twice++;
        for (k = 0; k < 3; k++) if (aPrint->iNeed[k] && w == aPrint->iNeed[k]) got |= 1 << k;
    }
    if ((got & need) != need) return 0;
    if (aPrint->iTwice && twice < 2) return 0;
    return 1;
}

// The ordinal whose body is the only one of its kind. Anything else —
// nothing matching, or more than one thing — is reported rather than
// guessed at.
static TInt FindOrdinal(const TPrint *aPrint, TInt *aHits)
{
    TInt ord, found = 0, hits = 0;
    for (ord = 1; ord <= gExpCount; ord++) {
        if (!Matches(ExportAddr(ord), aPrint)) continue;
        hits++;
        if (!found) found = ord;
    }
    *aHits = hits;
    return hits ? found : 0;
}

// RFs::Connect is the export that loads the address of the string the
// file server is known by. Nothing else in EFSRV does.
static TInt FindConnect(TInt *aHits)
{
    static const char kFileServer[] = "FileServer";
    TInt ord, found = 0, hits = 0, ok = 1, i;
    for (ord = 1; ord <= gExpCount; ord++) {
        TUint a = ExportAddr(ord);
        TUint end;
        if (!a) continue;
        end = NextExportAddr(a);
        for (i = 0; i < FP_WORDS; i++) {
            TUint at = a + (TUint)i * 4u;
            TUint w;
            if (at >= end) break;
            w = RomWordAt(at, &ok);
            if (!ok) break;
            if (w < gEfsrvCode || w >= gEfsrvEnd) continue;
            if (NameIs(w, 10, kFileServer) && RomByteAt(w + 10u, &ok) == 0) {
                hits++;
                if (!found) found = ord;
                break;
            }
        }
    }
    *aHits = hits;
    return hits ? found : 0;
}

// ── The progress file ───────────────────────────────────────────────

// Thirty-two bytes that say where the dump has got to, written only
// after a part has been written *and* read back clean. It records a
// byte offset rather than a part number, because a part stopped by a
// full disk is shorter than the rest and the next one has to start
// where it really stopped — not where a whole part would have ended.
//
//   0  'R' 'D' 'P' '1'
//   4  iRomBase       8  iRomSize      12  iOffset
//  16  iNextPart     20  iPartBytes    24  0
//  28  sum of words 0..6
#define PRG_BYTES 32
#define PRG_MAGIC0 'R'
#define PRG_MAGIC1 'D'
#define PRG_MAGIC2 'P'
#define PRG_MAGIC3 '1'

static TUint GetWord(const TUint8 *aBuf, TInt aAt)
{
    return (TUint)aBuf[aAt] | ((TUint)aBuf[aAt + 1] << 8) |
           ((TUint)aBuf[aAt + 2] << 16) | ((TUint)aBuf[aAt + 3] << 24);
}

static void PutWord(TUint8 *aBuf, TInt aAt, TUint aValue)
{
    aBuf[aAt] = (TUint8)aValue;
    aBuf[aAt + 1] = (TUint8)(aValue >> 8);
    aBuf[aAt + 2] = (TUint8)(aValue >> 16);
    aBuf[aAt + 3] = (TUint8)(aValue >> 24);
}

static TUint PrgSum(const TUint8 *aBuf)
{
    TUint sum = 0;
    TInt i;
    for (i = 0; i < PRG_BYTES - 4; i += 4) sum += GetWord(aBuf, i);
    return sum;
}

// Returns the offset the next part starts at, or 0 for "nothing usable
// here". A progress file that does not check out, or that belongs to a
// different ROM, is treated as absent: starting again is the only safe
// reading of it, and every part it would rewrite is rewritten whole.
static TUint ReadProgress(RFs_ *aFs, char aDrive, TUint aRomBase, TUint aRomSize, TInt *aNextPart)
{
    char nameBuf[NAME_MAX];
    TPtrC8_ name;
    TUint8 buf[PRG_BYTES];
    TUint got = 0;
    TUint offset;
    TInt part;

    MakeName(nameBuf, &name, aDrive, "ROMDUMP.PRG");
    if (ReadWholeFile(aFs, &name, buf, PRG_BYTES, &got) != KErrNone) return 0;
    if (got != PRG_BYTES) return 0;
    if (buf[0] != PRG_MAGIC0 || buf[1] != PRG_MAGIC1 ||
        buf[2] != PRG_MAGIC2 || buf[3] != PRG_MAGIC3) return 0;
    if (GetWord(buf, PRG_BYTES - 4) != PrgSum(buf)) return 0;
    if (GetWord(buf, 4) != aRomBase || GetWord(buf, 8) != aRomSize) return 0;
    offset = GetWord(buf, 12);
    part = (TInt)GetWord(buf, 16);
    if (offset > aRomSize) return 0;
    if (part < 1 || part > MAX_PARTS) return 0;
    *aNextPart = part;
    return offset;
}

static TInt WriteProgress(RFs_ *aFs, char aDrive, TUint aRomBase, TUint aRomSize,
                          TUint aOffset, TInt aNextPart)
{
    char nameBuf[NAME_MAX];
    TPtrC8_ name;
    TUint8 buf[PRG_BYTES];
    TInt i;

    for (i = 0; i < PRG_BYTES; i++) buf[i] = 0;
    buf[0] = PRG_MAGIC0; buf[1] = PRG_MAGIC1; buf[2] = PRG_MAGIC2; buf[3] = PRG_MAGIC3;
    PutWord(buf, 4, aRomBase);
    PutWord(buf, 8, aRomSize);
    PutWord(buf, 12, aOffset);
    PutWord(buf, 16, (TUint)aNextPart);
    PutWord(buf, 20, PART_BYTES);
    PutWord(buf, PRG_BYTES - 4, PrgSum(buf));
    MakeName(nameBuf, &name, aDrive, "ROMDUMP.PRG");
    return WriteWholeFile(aFs, &name, buf, PRG_BYTES);
}

// ── The report ──────────────────────────────────────────────────────

// Rewritten from scratch after every part, so the machine's own text
// editor shows a dump in progress as well as a finished one. It is the
// only account this program gives: there is no console to print to.
//
// The per-part rows are built as they happen, in their own buffer, and
// the rest of the report is built around them each time.
static char gReport[4096];
static char gRows[1024];
static TInt gRowCount;
static TInt gRowsAt;

#define MAX_ROWS 16

// One line per part written this run: which part, which bytes of the
// ROM it holds, and that it was read back and matched.
static void AddPartRow(TInt aPart, TUint aFrom, TUint aBytes)
{
    TInt at = gRowsAt;
    gRowCount++;
    if (gRowCount > MAX_ROWS) return;        // the buffer, not the dump, is the limit
    at = AppendText(gRows, at, "  ROMDUMP.");
    at = AppendPartDigits(gRows, at, aPart);
    at = AppendText(gRows, at, "  ROM 0x");
    at = AppendHex(gRows, at, aFrom, 6);
    at = AppendText(gRows, at, "..0x");
    at = AppendHex(gRows, at, aFrom + aBytes - 1u, 6);
    at = AppendText(gRows, at, "  ");
    at = AppendDec(gRows, at, aBytes >> 10);
    at = AppendText(gRows, at, " KB  checked\r\n");
    gRowsAt = at;
}

static void ForgetRows(void)
{
    gRowCount = 0;
    gRowsAt = 0;
}

static TInt AppendKb(TInt aAt, TUint aBytes)
{
    aAt = AppendDec(gReport, aAt, aBytes >> 10);
    return AppendText(gReport, aAt, " KB");
}

static void WriteReport(RFs_ *aFs, char aDrive)
{
    char nameBuf[NAME_MAX];
    TPtrC8_ name;
    TInt at, i;

    // The result first, then the numbers, then the detail: it is read
    // on a screen four inches across, and the first thing anyone wants
    // to know is whether the dump is finished.
    at = AppendText(gReport, 0, "Psion EPOC R1 ROM dump\r\n\r\nResult      ");
    if (gResult.iError == KErrNone && gResult.iOffset >= gResult.iRomSize) {
        at = AppendText(gReport, at, "complete - the dump is the whole ROM");
    } else if (gResult.iError == ERR_DISK_FULL) {
        at = AppendText(gReport, at, "stopped - the disk is full");
    } else if (gResult.iError == ERR_MISMATCH) {
        at = AppendText(gReport, at, "STOPPED - a part did not read back the same as the ROM, at 0x");
        at = AppendHex(gReport, at, gResult.iBadAt, 8);
    } else if (gResult.iError == ERR_NO_ROM) {
        at = AppendText(gReport, at, "no EPOC ROM header at 0x50000000");
    } else if (gResult.iError == ERR_NO_DRIVE) {
        at = AppendText(gReport, at, "no drive would take the dump");
    } else if (gResult.iError == ERR_KERNEL_SILENT) {
        at = AppendText(gReport, at, "the machine did not answer a request it was sent");
    } else if (gResult.iError == ERR_NO_CALLS) {
        at = AppendText(gReport, at, "the file server's own library is not where this ROM says");
    } else if (gResult.iError != KErrNone) {
        at = AppendText(gReport, at, "file server error ");
        at = AppendDec(gReport, at, (TUint)(-gResult.iError));
        at = AppendText(gReport, at, " at step ");
        at = AppendDec(gReport, at, (TUint)gResult.iStep);
    } else {
        at = AppendText(gReport, at, "stopped with more to write");
    }

    at = AppendText(gReport, at, "\r\nNext run    ");
    if (gResult.iOffset >= gResult.iRomSize) {
        at = AppendText(gReport, at, "nothing - the whole ROM is written");
    } else {
        at = AppendText(gReport, at, "part ");
        at = AppendDec(gReport, at, (TUint)gResult.iNextPart);
        at = AppendText(gReport, at, ", from ROM 0x");
        at = AppendHex(gReport, at, gResult.iRomBase + gResult.iOffset, 8);
    }

    at = AppendText(gReport, at, "\r\n\r\nROM base    0x");
    at = AppendHex(gReport, at, gResult.iRomBase, 8);
    at = AppendText(gReport, at, "\r\nROM size    0x");
    at = AppendHex(gReport, at, gResult.iRomSize, 8);
    at = AppendText(gReport, at, " (");
    at = AppendKb(at, gResult.iRomSize);
    at = AppendText(gReport, at, ")\r\nWritten to  ");
    gReport[at++] = (char)(gResult.iDrive ? gResult.iDrive : '?');
    at = AppendText(gReport, at, ":\\ROMDUMP.nnn\r\nDone so far ");
    at = AppendKb(at, gResult.iOffset);
    at = AppendText(gReport, at, " of ");
    at = AppendKb(at, gResult.iRomSize);

    at = AppendText(gReport, at, "\r\n\r\nThis run\r\n");
    if (gResult.iPartsThisRun == 0) {
        at = AppendText(gReport, at, "  nothing written\r\n");
    } else {
        for (i = 0; i < gRowsAt; i++) gReport[at++] = gRows[i];
        if (gRowCount > MAX_ROWS) {
            at = AppendText(gReport, at, "  ... and ");
            at = AppendDec(gReport, at, (TUint)(gRowCount - MAX_ROWS));
            at = AppendText(gReport, at, " more\r\n");
        }
        at = AppendText(gReport, at, "  ");
        at = AppendDec(gReport, at, (TUint)gResult.iPartsThisRun);
        at = AppendText(gReport, at, " parts, ");
        at = AppendKb(at, gResult.iBytesThisRun);
        at = AppendText(gReport, at, ", every one read back and matching\r\n");
    }

    at = AppendText(gReport, at,
        "\r\nTo finish the dump\r\n"
        "  1 copy ROMDUMP.* off this machine (PsiWin, or beam them)\r\n"
        "  2 delete the parts you have copied, keep ROMDUMP.PRG\r\n"
        "  3 run this again - it carries on from the byte above\r\n"
        "  4 on the PC join the parts in order, lowest number first:\r\n"
        "    copy /b ROMDUMP.001+ROMDUMP.002+... rom.img\r\n"
        "The joined file should be exactly the ROM size above.\r\n");

    MakeName(nameBuf, &name, aDrive, "ROMDUMP.TXT");
    WriteWholeFile(aFs, &name, gReport, (TUint)at);
}

// ── One part ────────────────────────────────────────────────────────

// Writes aBytes of ROM from aFrom into part aPart, and says how it got
// on. A write that fails part way — which on a machine like this means
// the disk filled up — is not the end of anything: whatever reached the
// file is good, and it is the read-back below, not this, that decides
// how much of the part to keep. That is deliberate. A write rejected
// for want of space can still have put some of its bytes in the file,
// so counting the writes that succeeded would leave those bytes to be
// written a second time in the next part, and the joined dump would
// have them twice.
static TInt WritePart(RFs_ *aFs, char aDrive, TInt aPart, TUint aFrom, TUint aBytes)
{
    RFile_ f;
    char nameBuf[NAME_MAX];
    TPtrC8_ name, des;
    TUint done = 0;
    TInt err;

    MakePartName(nameBuf, &name, aDrive, aPart);
    f.iHandle = 0; f.iSubSessionHandle = 0;
    err = gFs.iReplace(&f, aFs, &name, EFileWrite);
    if (err != KErrNone) return err;
    while (done < aBytes) {
        TUint n = aBytes - done;
        if (n > CHUNK_BYTES) n = CHUNK_BYTES;
        des.iTypeLength = TYPE_LEN(EPtrC, n);
        des.iPtr = (const void *)(aFrom + done);
        err = gFs.iWrite(&f, &des);
        if (err != KErrNone) break;
        done += n;
    }
    gFs.iClose(&f);
    return err;
}

// Reads the part back off the disk and compares it with the ROM, and
// returns how many bytes from the start of the part matched. Anything
// that does not match is an error the caller must stop on; a file that
// simply ends early is the disk having filled up, and the bytes before
// the end are still the ROM's.
static TUint VerifyPart(RFs_ *aFs, char aDrive, TInt aPart, TUint aFrom, TUint aBytes, TInt *aErr)
{
    static TUint8 buf[VERIFY_BYTES];
    RFile_ f;
    char nameBuf[NAME_MAX];
    TPtrC8_ name;
    TPtr8_ des;
    TUint done = 0;

    *aErr = KErrNone;
    MakePartName(nameBuf, &name, aDrive, aPart);
    f.iHandle = 0; f.iSubSessionHandle = 0;
    *aErr = gFs.iOpen(&f, aFs, &name, EFileRead);
    if (*aErr != KErrNone) return 0;
    while (done < aBytes) {
        const volatile TUint8 *rom = (const volatile TUint8 *)(aFrom + done);
        TUint want = aBytes - done;
        TUint got, i;
        if (want > VERIFY_BYTES) want = VERIFY_BYTES;
        des.iTypeLength = TYPE_LEN(EPtr, 0);
        des.iMaxLength = (TInt)want;
        des.iPtr = buf;
        *aErr = gFs.iRead(&f, &des);
        if (*aErr != KErrNone) break;
        got = DES_LEN(des.iTypeLength);
        if (got == 0) break;                 // end of a part cut short
        for (i = 0; i < got; i++) {
            if (buf[i] != rom[i]) {
                gResult.iBadAt = aFrom + done + i;
                *aErr = ERR_MISMATCH;
                break;
            }
        }
        if (*aErr != KErrNone) break;
        done += got;
    }
    gFs.iClose(&f);
    return done;
}

// ── The dump ────────────────────────────────────────────────────────

// Writes parts on aDrive until the ROM is finished or the disk is not.
// Returns the number of bytes this run added.
//
// aWholeFirstPart is how a drive is turned down: a card with a few
// hundred bytes left on it will take a part file and a little of the
// first part, and stopping there would be a worse answer than the RAM
// disk next to it. Asked for a whole first part and unable to write
// one, this records nothing and reports nothing written, so the caller
// can try the next drive; the short part file it leaves behind is
// rewritten from its first byte whenever the dump comes back.
static TUint DumpTo(RFs_ *aFs, char aDrive, TUint aOffset, TInt aPart, TInt aWholeFirstPart)
{
    TUint added = 0;

    gResult.iDrive = aDrive;
    gResult.iOffset = aOffset;
    gResult.iStartOffset = aOffset;
    gResult.iNextPart = aPart;
    gResult.iStep = STEP_DRIVE;

    while (gResult.iOffset < gResult.iRomSize && gResult.iNextPart <= MAX_PARTS) {
        TUint from = gResult.iRomBase + gResult.iOffset;
        TUint want = gResult.iRomSize - gResult.iOffset;
        TUint good;
        TInt writeErr, err;

        if (want > PART_BYTES) want = PART_BYTES;

        LogText("part "); LogDec((TUint)gResult.iNextPart);
        LogText(" from ROM 0x"); LogHex(from, 8);
        LogText(" want "); LogDec(want >> 10); LogText(" KB");
        LogEol();

        // Written before the part, as well as after it, so that the
        // thirty-two bytes it needs are already on the disk when the
        // part fills it: rewriting a file that exists takes no more
        // room than it already has, but creating one on a full disk
        // takes none at all. Until the part below has been written and
        // checked this says exactly what it said before — the offset
        // the dump has actually reached.
        WriteProgress(aFs, aDrive, gResult.iRomBase, gResult.iRomSize,
                      gResult.iOffset, gResult.iNextPart);

        gResult.iStep = STEP_WRITING;
        writeErr = WritePart(aFs, aDrive, gResult.iNextPart, from, want);
        LogCall("  wrote it,", writeErr);

        // Read back against what the part was *meant* to hold: what the
        // file really holds, checked byte for byte against the ROM, is
        // the only number worth carrying forward.
        gResult.iStep = STEP_VERIFYING;
        good = VerifyPart(aFs, aDrive, gResult.iNextPart, from, want, &err);
        LogText("  read back "); LogDec(good >> 10); LogText(" KB of ");
        LogDec(want >> 10); LogText(" KB, rc="); LogErr(err);
        if (err == ERR_MISMATCH) { LogText(" MISMATCH at 0x"); LogHex(gResult.iBadAt, 8); }
        LogEol();
        if (err == ERR_MISMATCH) {           // never advance past bad bytes
            gResult.iError = err;
            break;
        }
        if (good == 0) {
            // Nothing usable went in. A file server error says which
            // kind of nothing; without one it is a disk with no room.
            if (err != KErrNone) gResult.iError = err;
            else if (writeErr != KErrNone) gResult.iError = writeErr;
            else gResult.iError = ERR_DISK_FULL;
            break;
        }

        if (aWholeFirstPart && added == 0 && good < want) {
            gResult.iError = ERR_DISK_FULL;      // not a drive worth starting on
            break;
        }

        gResult.iOffset += good;
        added += good;
        gResult.iBytesThisRun += good;
        gResult.iPartsThisRun++;
        AddPartRow(gResult.iNextPart, from, good);
        gResult.iNextPart++;

        // Only now, and in this order: the progress file is the promise
        // that everything before this offset is on the disk and checked.
        LogCall("  progress written,",
                WriteProgress(aFs, aDrive, gResult.iRomBase, gResult.iRomSize,
                              gResult.iOffset, gResult.iNextPart));
        WriteReport(aFs, aDrive);

        if (good < want) {                   // the part was cut short
            gResult.iError = ERR_DISK_FULL;
            break;
        }
    }

    if (gResult.iOffset >= gResult.iRomSize && gResult.iError == KErrNone) {
        gResult.iStep = STEP_DONE;
    }
    return added;
}

// Writing the first report is also how a drive is tried: a drive that
// is not there, or is write-protected, or is full before we start,
// fails here and the next one is tried instead. It leaves nothing
// behind that a later run could mistake for progress — the progress
// file is only ever written after a part has been checked.
static TInt ProbeDrive(RFs_ *aFs, char aDrive)
{
    char nameBuf[NAME_MAX];
    TPtrC8_ name;
    RFile_ f;
    TInt err;

    gResult.iDrive = aDrive;
    WriteReport(aFs, aDrive);
    MakeName(nameBuf, &name, aDrive, "ROMDUMP.TXT");
    f.iHandle = 0; f.iSubSessionHandle = 0;
    err = gFs.iOpen(&f, aFs, &name, EFileRead);
    LogCall("  wrote a report there and reopened it,", err);
    if (err != KErrNone) return 0;
    gFs.iClose(&f);
    return 1;
}

// ── E32Main ─────────────────────────────────────────────────────────

// Picks a drive for the log by writing one. It goes on the same
// candidates in the same order as the dump, so the two normally end up
// together, and a machine that will not take the log at all still gets
// as far as the dump (silently, which the report then says).
static void LogOpen(RFs_ *aFs)
{
    char nameBuf[NAME_MAX];
    TPtrC8_ name;
    RFile_ f;
    TInt i;
    gLogFs = aFs;
    for (i = 0; KDrives[i]; i++) {
        gLogDrive = KDrives[i];
        LogFlush();
        MakeName(nameBuf, &name, gLogDrive, "ROMDUMP.LOG");
        f.iHandle = 0; f.iSubSessionHandle = 0;
        if (gFs.iOpen(&f, aFs, &name, EFileRead) == KErrNone) {
            gFs.iClose(&f);
            return;                      // this drive keeps the log
        }
    }
    gLogDrive = 0;                       // nowhere to write it
}

// Writes down what this machine is, in the terms that decide whether
// this program can work on it: the ROM header it found, and the file
// server client it is about to call. Everything here is read from the
// machine, not assumed — which is the point, because a prototype is
// exactly the machine that might answer differently.
static void CheckThisMachine(TFsCalls *aOut)
{
    static const TPrint kClosePrint =
        { 0xe3a0101au, { 0, 0, 0 }, 0, 0 };                        /* mov r1,#0x1a */
    static const TPrint kOpenPrint =
        { 0, { 0xe1a0300du, 0xe3a0201bu, 0 }, 0, 0 };              /* args on the stack, code 0x1b */
    static const TPrint kReplacePrint =
        { 0, { 0xe1a0300du, 0xe3a0201du, 0 }, 0, 0 };              /* ... code 0x1d */
    static const TPrint kReadPrint =
        { 0, { 0xe3a0101fu, 0xe3a03480u, 0xe5913004u }, 0, 0xe1a03002u };
    static const TPrint kWritePrint =
        { 0, { 0xe3a01020u, 0xe3a03480u, 0 }, 0xe3c334f0u, 0xe1a03002u };

    TUint efsrv = 0, size = 0;
    TInt ok = 1, hits = 0, ord;
    TUint uid1, uid3;

    LogText("rom hdr base=0x"); LogHex(gResult.iRomBase, 8);
    LogText(" size=0x"); LogHex(gResult.iRomSize, 8);
    LogText(" root=0x"); LogHex(RomWordAt(gRomBase + ROMHDR_ROOT, &ok), 8);
    LogEol();

    // Sixteen words of the header, so a machine whose ROM is laid out
    // differently can be recognised from the log alone.
    {
        TInt i;
        LogText("rom hdr +0x80:");
        for (i = 0; i < 16; i++) {
            LogCh(' ');
            LogHex(RomWordAt(gRomBase + 0x80u + (TUint)i * 4u, &ok), 8);
            if ((i & 7) == 7 && i != 15) { LogEol(); LogText("             "); }
        }
        LogEol();
    }

    if (!RomFindFile("EFSrv.dll", &efsrv, &size)) {
        LogText("EFSrv.dll NOT FOUND in the ROM directory - "
                "using the ordinals this build was linked with");
        LogEol();
        return;
    }
    uid1 = RomWordAt(efsrv + RIMG_UID1, &ok);
    uid3 = RomWordAt(efsrv + RIMG_UID3, &ok);
    gEfsrvCode = RomWordAt(efsrv + RIMG_CODE, &ok);
    gEfsrvEnd = gEfsrvCode + RomWordAt(efsrv + RIMG_CSIZE, &ok);
    gExpCount = (TInt)RomWordAt(efsrv + RIMG_NEXP, &ok);
    gExpDir = RomWordAt(efsrv + RIMG_EXP, &ok);

    LogText("EFSrv.dll at 0x"); LogHex(efsrv, 8);
    LogText(" uid1=0x"); LogHex(uid1, 8);
    LogText(" uid3=0x"); LogHex(uid3, 8);
    LogText(" exports="); LogDec((TUint)gExpCount);
    LogEol();
    LogText("  code 0x"); LogHex(gEfsrvCode, 8);
    LogText("..0x"); LogHex(gEfsrvEnd, 8);
    LogText(" exportdir=0x"); LogHex(gExpDir, 8);
    LogEol();

    if (!ok || uid1 != KDynamicLibUid || gExpCount < 1 || gExpCount > 4096 ||
        !RomOk(gExpDir, (TUint)gExpCount * 4u) || gEfsrvEnd <= gEfsrvCode) {
        LogText("  that is not an export table this program can read - "
                "keeping the linked ordinals");
        LogEol();
        gExpCount = 0;
        return;
    }
    if (uid3 != KEfsrvUid3) {
        LogText("  NOTE its UID3 is not 0x100000bd, so this binary's import "
                "should not have bound at all");
        LogEol();
    }

    // Each call, by what only it does. The usual answer is that the
    // ordinal this build was linked with is the one whose body has the
    // fingerprint, and the log says "confirmed". If it is not — which
    // is what a prototype with an older EFSrv.dll would look like — and
    // exactly one other export has it, the program calls that one
    // instead and says so in capitals. Anything less clear than that is
    // left alone, because a wrong address is worse than an old one.
#define RESOLVE(field, want, print, fn)                                        \
    do {                                                                       \
        TInt linkedOk = Matches(ExportAddr(want), print);                      \
        ord = FindOrdinal(print, &hits);                                       \
        LogText("  " #field " linked="); LogDec((TUint)(want));                \
        LogText(" found="); LogDec((TUint)ord);                                \
        LogText(" matches="); LogDec((TUint)hits);                             \
        if (linkedOk) {                                                        \
            aOut->field = (fn)ExportAddr(want);                                  \
            LogText(" confirmed at 0x"); LogHex((TUint)ExportAddr(want), 8);   \
        } else if (ord && hits == 1) {                                         \
            aOut->field = (fn)ExportAddr(ord);                                   \
            LogText(" ** NOT THE LINKED ORDINAL ** using 0x");                 \
            LogHex((TUint)ExportAddr(ord), 8);                                 \
        } else {                                                               \
            LogText(" - unrecognisable, keeping the linked one");              \
        }                                                                      \
        LogEol();                                                              \
    } while (0)

    // The two closes in EFSRV have the same body; the lower of them is
    // RFile's (every binary in an R1 ROM that touches a file imports
    // that one, and nothing imports the other), so a second match is
    // expected here and is not a reason to fall back.
    // RFile::Close: two exports in EFSRV have the same two-instruction
    // body, and the lower of them is RFile's — every binary in an R1
    // ROM that touches a file imports that one and nothing imports the
    // other — so a second match here is expected.
    ord = FindOrdinal(&kClosePrint, &hits);
    LogText("  iClose linked="); LogDec(ORD_CLOSE);
    LogText(" found="); LogDec((TUint)ord);
    LogText(" matches="); LogDec((TUint)hits);
    if (Matches(ExportAddr(ORD_CLOSE), &kClosePrint)) {
        aOut->iClose = (TFnClose)ExportAddr(ORD_CLOSE);
        LogText(" confirmed at 0x"); LogHex((TUint)ExportAddr(ORD_CLOSE), 8);
    } else if (ord) {
        aOut->iClose = (TFnClose)ExportAddr(ord);
        LogText(" ** NOT THE LINKED ORDINAL ** using 0x");
        LogHex((TUint)ExportAddr(ord), 8);
    } else {
        LogText(" - unrecognisable, keeping the linked one");
    }
    LogEol();

    ord = FindConnect(&hits);
    LogText("  iConnect linked="); LogDec(ORD_CONNECT);
    LogText(" found="); LogDec((TUint)ord);
    LogText(" matches="); LogDec((TUint)hits);
    if (ord == ORD_CONNECT) {
        aOut->iConnect = (TFnConnect)ExportAddr(ORD_CONNECT);
        LogText(" confirmed at 0x"); LogHex((TUint)ExportAddr(ORD_CONNECT), 8);
    } else if (ord && hits == 1) {
        aOut->iConnect = (TFnConnect)ExportAddr(ord);
        LogText(" ** NOT THE LINKED ORDINAL ** using 0x");
        LogHex((TUint)ExportAddr(ord), 8);
    } else {
        LogText(" - unrecognisable, keeping the linked one");
    }
    LogEol();

    RESOLVE(iOpen, ORD_OPEN, &kOpenPrint, TFnOpen);
    RESOLVE(iReplace, ORD_REPLACE, &kReplacePrint, TFnReplace);
    RESOLVE(iRead, ORD_READ, &kReadPrint, TFnRead);
    RESOLVE(iWrite, ORD_WRITE, &kWritePrint, TFnWrite);
#undef RESOLVE
}

// What this machine's own ROM offers, worked out once. It is both the
// fallback for the calls in this program and the most useful thing the
// log can say about an unknown machine, so it runs whether or not its
// answers are going to be used.
static void RomCallsOnce(void)
{
    if (gRomCallsDone) return;
    gRomCallsDone = 1;
    StartingCalls(&gRomCalls);
    if (gResult.iError == KErrNone) CheckThisMachine(&gRomCalls);
}

// A test seam, and only that: run the startup self-check on its own
// against whatever ROM is mapped, and leave the log behind for the
// caller to read. tests/unit/er1_romdump_test.c calls it with a real
// R1 ROM image mapped where the machine keeps one, which is how the
// export-recognising above is checked against all three R1 ROMs
// without a Psion. It stops before anything is dumped, because the
// addresses it resolves are ARM code in that ROM: the machine can call
// them and a PC cannot.
TInt RomDumpCheckOnly(RFs_ *aFs)
{
    TUint romBase = 0, romSize = 0;
    TInt err;

    gLogAt = 0; gLogLost = 0; gLogDrive = 0; gLogFs = 0;
    gRomBase = 0; gRomEnd = 0; gExpCount = 0;
    StartingCalls(&gFs);

    LogText("ROMDUMP for EPOC R1 - " BUILD_ID " (self-check only)"); LogEol();
    LogOpen(aFs);
    err = FindRom(&romBase, &romSize);
    gResult.iRomBase = romBase;
    gResult.iRomSize = romSize;
    if (err != KErrNone) { LogText("no ROM header"); LogEol(); return err; }
    gRomBase = romBase;
    gRomEnd = romBase + romSize;
    {
        TFsCalls found;
        CopyCalls(&found, &gFs);
        CheckThisMachine(&found);       // deliberately not published: see above
    }
    LogFlush();
    return KErrNone;
}

TInt RomDumpMain(void)
{
    RFs_ fs;
    TUint romBase = 0, romSize = 0;
    TInt err, i, pass;

    // A process starts with all of this zeroed, so clearing it again
    // costs nothing on a Psion — and it is what lets the same code be
    // run twice over in one process, which is how it is tested
    // (tests/unit/er1_romdump_test.c).
    {
        TUint8 *p = (TUint8 *)&gResult;
        TInt n = (TInt)sizeof(gResult);
        while (n--) *p++ = 0;
    }
    ForgetRows();
    gLogAt = 0;
    gLogLost = 0;
    gLogDrive = 0;
    gLogFs = 0;
    gRomBase = 0;
    gRomEnd = 0;
    gExpCount = 0;
    gRomCallsDone = 0;
    gResult.iMagic = RESULT_MAGIC;
    gResult.iStep = STEP_START;

    // The calls start as the ones the loader bound by ordinal.
    // CheckThisMachine may replace them with what it finds in this
    // machine's own ROM.
    StartingCalls(&gFs);

    LogText("ROMDUMP for EPOC R1 - " BUILD_ID); LogEol();

    // Three ways to reach the file server, in the order they are tried.
    // Which one a machine needs is the first thing the log says.
#ifdef NO_IMPORTS
    LogText("imports: none - nothing for the loader to bind"); LogEol();

    // Nothing can be written anywhere until the file server answers, so
    // the log is built up in memory and appears as soon as one does.
    gResult.iError = FindRom(&romBase, &romSize);
    gResult.iRomBase = romBase;
    gResult.iRomSize = romSize;
    if (gResult.iError == KErrNone) {
        gRomBase = romBase;
        gRomEnd = romBase + romSize;
        gResult.iStep = STEP_ROM_FOUND;
    }

    // 1. The kernel on its own: the client side of the file server is
    //    in this program, so nothing in the ROM has to be found first.
    KernelCalls(&gFs);
    fs.iHandle = 0;
    err = gFs.iConnect(&fs, 4);
    LogCall("connect with the kernel calls in this program,", err);

    // 2. Failing that, the machine's own file server library, wherever
    //    it is and whatever it is called.
    if (err != KErrNone && gResult.iError == KErrNone) {
        RomCallsOnce();
        if (gRomCalls.iConnect) {
            CopyCalls(&gFs, &gRomCalls);
            fs.iHandle = 0;
            err = gFs.iConnect(&fs, 4);
            LogCall("connect with the library found in the ROM,", err);
        }
    }
    if (err != KErrNone) { gResult.iError = err; return err; }
    gResult.iStep = STEP_CONNECTED;
#else
    LogText("imports linked: EFSRV[100000bd].DLL 14,17,105,119,133,170"); LogEol();

    // The file server comes first, before anything that could go wrong
    // has a chance to: with no console, a file on the disk is the only
    // way this program can say anything at all, and it cannot write one
    // until it has connected. 4 message slots is what every caller of
    // RFs::Connect in the ROM asks for.
    fs.iHandle = 0;
    err = gFs.iConnect(&fs, 4);
    LogCall("connect to the file server", err);
    if (err != KErrNone) {
        // The library the loader bound is there but will not answer.
        // The kernel calls in this program need nothing from it.
        KernelCalls(&gFs);
        fs.iHandle = 0;
        err = gFs.iConnect(&fs, 4);
        LogCall("connect with the kernel calls in this program instead,", err);
    }
    if (err != KErrNone) {
        gResult.iError = err;           // nothing can be said; the exit code is all there is
        return err;
    }
    gResult.iStep = STEP_CONNECTED;
#endif

    LogOpen(&fs);
    LogText("log on drive ");
    LogCh(gLogDrive ? gLogDrive : '?');
    LogEol();

#ifndef NO_IMPORTS
    gResult.iError = FindRom(&romBase, &romSize);
    gResult.iRomBase = romBase;
    gResult.iRomSize = romSize;
    if (gResult.iError == KErrNone) {
        gRomBase = romBase;
        gRomEnd = romBase + romSize;
        gResult.iStep = STEP_ROM_FOUND;
    } else {
        // The three words the check looks at, whatever they were, so a
        // machine that keeps its ROM somewhere else says so in the log.
        TInt ok = 1;
        gRomBase = ROM_LINEAR_BASE;
        gRomEnd = ROM_LINEAR_BASE + 0x100u;     // enough to read the header
        LogText("NO ROM HEADER at 0x50000000: base=0x");
        LogHex(RomWordAt(ROM_LINEAR_BASE + ROMHDR_BASE, &ok), 8);
        LogText(" size=0x"); LogHex(RomWordAt(ROM_LINEAR_BASE + ROMHDR_SIZE, &ok), 8);
        LogText(" root=0x"); LogHex(RomWordAt(ROM_LINEAR_BASE + ROMHDR_ROOT, &ok), 8);
        LogEol();
        gRomEnd = 0;
    }
#endif

#ifndef NO_IMPORTS
    if (gFs.iConnect != RawConnect) {
        RomCallsOnce();
        CopyCalls(&gFs, &gRomCalls);    // one step, once the log is written
    } else {
        RomCallsOnce();                 // for the record; nothing adopts it
    }
#else
    // The kernel calls got this far on their own, which is the whole
    // point of this build — but what the machine's own ROM has to say
    // is still the first thing anyone reading a log off an unknown
    // prototype will want, so the check runs anyway. Its answers go
    // into a copy nothing calls: they are for the record, not for use.
    if (gFs.iConnect == RawConnect) {
        LogText("the calls in use are this program's own, over the kernel"); LogEol();
        RomCallsOnce();
    } else {
        LogText("(the calls above were found before the log could be opened)"); LogEol();
    }
#endif

    // A session that answers is not the same as one that works. The log
    // is the first thing written, so a machine where no drive would take
    // it has either no writable disk or a file server this program is
    // talking to wrongly — and the second is worth one more try. This
    // program gets one run on a prototype; a silent one is a wasted one.
    if (!gLogDrive) {
        TFsCalls other;
        TInt have;
        if (gFs.iConnect == RawConnect) {
            RomCallsOnce();
            CopyCalls(&other, &gRomCalls);
            have = other.iConnect != 0;
        } else {
            KernelCalls(&other);
            have = 1;
        }
        if (have) {
            CopyCalls(&gFs, &other);
            fs.iHandle = 0;
            err = gFs.iConnect(&fs, 4);
            LogCall("no drive would take the log - connecting the other way,", err);
            if (err == KErrNone) {
                LogOpen(&fs);
                LogText("log on drive "); LogCh(gLogDrive ? gLogDrive : '?'); LogEol();
            }
        }
    }

    // A dump already under way decides the drive: carry on where the
    // parts already are, whatever else is plugged in now.
    if (gResult.iError == KErrNone) {
        for (i = 0; KDrives[i]; i++) {
            TInt part = 1;
            TUint offset = ReadProgress(&fs, KDrives[i], romBase, romSize, &part);
            LogText("progress on "); LogCh(KDrives[i]);
            LogText(": offset=0x"); LogHex(offset, 8);
            LogText(" next part="); LogDec((TUint)part);
            LogEol();
            if (offset > 0) {
                DumpTo(&fs, KDrives[i], offset, part, 0);
                WriteReport(&fs, KDrives[i]);
                LogText("done, resumed on "); LogCh(KDrives[i]);
                LogText(", error="); LogErr(gResult.iError);
                LogEol();
                return gResult.iError;
            }
        }
    }

    // Otherwise take the first drive that will have us: one that can
    // hold a whole part if there is one, and failing that any drive
    // that will take anything at all, since a machine with very little
    // room still gets somewhere one run at a time. Writing that first
    // report is also what tells whoever is holding the machine that the
    // program ran at all.
    for (pass = 1; pass >= 0; pass--) {
        for (i = 0; KDrives[i]; i++) {
            LogText("try drive "); LogCh(KDrives[i]);
            LogText(pass ? " (needs room for a whole part)" : " (any room at all)");
            LogEol();
            if (!ProbeDrive(&fs, KDrives[i])) {
                LogText("  will not take a file - next drive"); LogEol();
                continue;
            }
            if (gResult.iError != KErrNone) {
                LogText("  stopping: "); LogErr(gResult.iError); LogEol();
                return gResult.iError;   // no ROM to dump; the report says so
            }
            if (DumpTo(&fs, KDrives[i], 0, 1, pass) > 0) {
                WriteReport(&fs, KDrives[i]);
                LogText("done on "); LogCh(KDrives[i]);
                LogText(", error="); LogErr(gResult.iError);
                LogText(", written="); LogDec(gResult.iOffset >> 10); LogText(" KB");
                LogEol();
                return gResult.iError;
            }
            WriteReport(&fs, KDrives[i]);   // say on that drive why it was passed over
            LogText("  nothing written there, error="); LogErr(gResult.iError); LogEol();
            gResult.iError = KErrNone;      // that drive's failure, not the dump's
            gResult.iPartsThisRun = 0;
            gResult.iBytesThisRun = 0;
            gResult.iOffset = 0;
            gResult.iNextPart = 1;
            ForgetRows();
        }
    }

    gResult.iError = ERR_NO_DRIVE;
    LogText("NO DRIVE would take the dump"); LogEol();
    for (i = 0; KDrives[i]; i++) WriteReport(&fs, KDrives[i]);
    return gResult.iError;
}
