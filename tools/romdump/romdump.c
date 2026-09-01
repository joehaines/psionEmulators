// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// ROMDUMP — copy a Psion EPOC Release 5 *Unicode* machine's ROM out to
// files on its own disk, for machines the ER5 (non-Unicode) ROM
// extractors cannot run on: the Conan / Revo-Bluetooth generation.
//
// It is an ordinary EPOC32 process — no application framework and no
// resource file — so all it needs from the ROM is the file server client
// and a text console:
//
//     EFSRV ordinal  18   RFs::Connect(TInt aMessageSlots)
//     EFSRV ordinal 121   RFile::Open(RFs&, const TDesC16&, TUint)
//     EFSRV ordinal 136   RFile::Read(TDes8&)
//     EFSRV ordinal 151   RFile::Replace(RFs&, const TDesC16&, TUint)
//     EFSRV ordinal 194   RFile::Write(const TDesC8&)
//     EFSRV ordinal  15   RFile::Close()
//     EUSER ordinal 511   CConsoleBase::Getch()
//     EUSER ordinal 731   Console::NewL(const TDesC16&, TSize)
//     EUSER ordinal 746   CTrapCleanup::New()
//     EUSER ordinal 837   CConsoleBase::Printf(TRefByValue<const TDesC16>, ...)
//
// Those ordinals were read out of the ER5u ROM itself rather than an
// SDK; docs/conan-rom-dumping.md records how, and why they are the same
// numbers on any ER5u build. The console four are the ones the ROM's own
// EShell uses to put its prompt on the screen, which is where they were
// found.
//
// The ROM is memory-mapped and readable from user mode at its own base
// address, so "dumping" it is just RFile::Write of a descriptor that
// points into it — no driver, no kernel-mode code, nothing device
// specific.  What *is* specific to this generation of machine is that
// the ROM (12 MB on the Conan) does not fit on the RAM disk it would be
// written to, so the dump is cut into parts and can be resumed:
//
//     C:\ROMDUMP.001 …  the parts, PART_BYTES each (last one short)
//     C:\ROMDUMP.PRG    4 bytes: the part to write next
//     C:\ROMDUMP.TXT    a plain-text summary of what happened
//
// It asks how many parts to write now, so a machine with room for two
// can be emptied and asked for two more; every part is read back off the
// disk and compared with the ROM before the next one starts; and the
// progress file is re-read before every batch, so the next batch — or
// the next run, after copying the parts off — carries on from exactly
// where the last one stopped. Nothing is ever deleted by this program.

typedef unsigned char TUint8;
typedef unsigned short TUint16;
typedef unsigned int TUint;
typedef int TInt;

// ── The bits of EPOC this needs ─────────────────────────────────────

// Descriptor type nibble, held in the top 4 bits of the length word.
// EPtrC/EPtr confirmed against EUser's own TPtrC16 constructor, which
// ORs 0x10000000 into the length it computes.
#define EPtrC 1u
#define EPtr  2u
#define TYPE_LEN(t, n) (((t) << 28) | ((n) & 0x0fffffffu))
#define DES_LEN(x)     ((x) & 0x0fffffffu)

typedef struct { TUint iTypeLength; const void *iPtr; } TPtrC_;
typedef struct { TUint iTypeLength; TInt iMaxLength; void *iPtr; } TPtr_;

// TFileMode. EFileRead and EFileShareExclusive are both zero.
#define EFileWrite 0x200u

#define KErrNone 0

// Private codes for what the read-back can find, kept well away from
// EPOC's own (which run -1 downwards).
#define ERR_SHORT_READ (-1001)
#define ERR_MISMATCH   (-1002)

// RFs is a session handle; RFile is that plus a subsession handle.
typedef struct { TInt iHandle; TInt iPad[3]; } RFs_;
typedef struct { TInt iHandle; TInt iSubSessionHandle; TInt iPad[2]; } RFile_;

extern void RFile_Close(RFile_ *aFile);
extern TInt RFs_Connect(RFs_ *aFs, TInt aMessageSlots);
extern TInt RFile_Open(RFile_ *aFile, RFs_ *aFs, const void *aName, TUint aMode);
extern TInt RFile_Read(RFile_ *aFile, TPtr_ *aDes);
extern TInt RFile_Replace(RFile_ *aFile, RFs_ *aFs, const void *aName, TUint aMode);
extern TInt RFile_Write(RFile_ *aFile, const TPtrC_ *aDes);

extern TInt Console_Getch(void *aConsole);
extern void *Console_NewL(const void *aTitle, TInt aWidth, TInt aHeight);
extern void *CleanupStack_New(void);
extern void Console_Printf(void *aConsole, const TPtrC_ *aFmt);

// ── Tunables ────────────────────────────────────────────────────────

// Where the ROM is mapped. Every EPOC32 ARM machine puts it here; the
// header there is read for the real base and length, so a machine whose
// ROM is shorter than the Conan's 12 MB dumps only what it has.
#define ROM_LINEAR_BASE 0x50000000u

// One part per file. 2 MB is small enough to leave room on a Revo-class
// RAM disk next to whatever the owner already keeps on it, and big
// enough that a 12 MB ROM is six files.
#define PART_BYTES (2u * 1024u * 1024u)

// Bytes per RFile::Write. The file server copies this out of our address
// space in one IPC transfer.
#define CHUNK_BYTES 0x8000u

// Bytes per RFile::Read on the read-back pass. This one lands on the
// stack, so it is the stack size (see build.sh) that caps it.
#define VERIFY_BYTES 0x1000u

// Chunks between progress dots, so each part draws a short row of them.
#define DOT_CHUNKS 8

// ── Progress record, for the emulator-side test ─────────────────────

// Left in memory so a RAM snapshot can be searched for it (the harness
// has no other way to see inside a running EPOC process). 'RDMP', then
// what the run did.
#define RESULT_MAGIC 0x504d4452u

static struct {
    TUint iMagic;
    TInt  iStep;        // last step reached (see STEP_* below)
    TInt  iError;       // first error that stopped it
    TUint iRomBase;
    TUint iRomSize;
    TInt  iFirstPart;   // part the last batch started at
    TInt  iNextPart;    // part the next batch should start at
    TUint iBytesWritten;
    TUint iPartsWritten;
    TUint iPartsVerified;
    TUint iBadAt;       // ROM address of the first byte that read back wrong
} gResult;

#define STEP_START     1
#define STEP_CONNECTED 2
#define STEP_SCANNED   3
#define STEP_WRITING   4
#define STEP_VERIFYING 5
#define STEP_DONE      6

static void *gConsole;          // 0 if the machine would not give us one

// ── Text, in the 16-bit characters this generation uses ─────────────

static TInt AppendText(TUint16 *aBuf, TInt aAt, const char *aText)
{
    while (*aText) aBuf[aAt++] = (TUint16)(TUint8)*aText++;
    return aAt;
}

static TInt AppendHex(TUint16 *aBuf, TInt aAt, TUint aValue, TInt aDigits)
{
    static const char kHex[] = "0123456789ABCDEF";
    TInt i;
    for (i = aDigits - 1; i >= 0; i--) aBuf[aAt++] = (TUint16)kHex[(aValue >> (i * 4)) & 0xf];
    return aAt;
}

static TInt AppendDec(TUint16 *aBuf, TInt aAt, TUint aValue)
{
    TUint16 tmp[12];
    TInt n = 0;
    do { tmp[n++] = (TUint16)('0' + (aValue % 10u)); aValue /= 10u; } while (aValue);
    while (n) aBuf[aAt++] = tmp[--n];
    return aAt;
}

// Printf takes a format, so nothing printed through it may carry a '%'.
// Nothing this program says does.
static void Print(const TUint16 *aBuf, TInt aLen)
{
    TPtrC_ d;
    if (!gConsole) return;
    d.iTypeLength = TYPE_LEN(EPtrC, (TUint)aLen);
    d.iPtr = aBuf;
    Console_Printf(gConsole, &d);
}

// Printed in pieces, so however long the text is it can never be longer
// than the buffer it is copied into. (It once was: a KERN-EXEC 3 on the
// machine, which is exactly how it was found.)
static void Say(const char *aText)
{
    TUint16 buf[128];
    while (*aText) {
        TInt n = 0;
        while (*aText && n < (TInt)(sizeof(buf) / sizeof(buf[0]))) {
            buf[n++] = (TUint16)(TUint8)*aText++;
        }
        Print(buf, n);
    }
}

static TInt Key(void)
{
    return gConsole ? Console_Getch(gConsole) : 'A';
}

#define EKeyEscape 27

// "Press any key" is the wrong prompt to end on: a key held long enough
// to repeat leaves the extra presses in the console's queue, and one of
// them closes the window the instant the instructions appear — which is
// exactly what it used to do. Esc has to be pressed on purpose.
static void WaitToClose(void)
{
    if (!gConsole) return;
    Say("\nPress Esc to close.");
    while (Key() != EKeyEscape) { }
}

// ── The ROM header ──────────────────────────────────────────────────

// TRomHeader fields, at the base of the ROM image: +0x8c iRomBase,
// +0x90 iRomSize, +0x94 iRomRootDirectoryList. Checking all three keeps
// a machine whose header is elsewhere from dumping nonsense.
#define ROMHDR_BASE 0x8c
#define ROMHDR_SIZE 0x90
#define ROMHDR_ROOT 0x94

static TUint ReadRomWord(TUint aAddr)
{
    return *(volatile const TUint *)aAddr;
}

static TInt FindRom(TUint *aBase, TUint *aSize)
{
    TUint base = ReadRomWord(ROM_LINEAR_BASE + ROMHDR_BASE);
    TUint size = ReadRomWord(ROM_LINEAR_BASE + ROMHDR_SIZE);
    TUint root = ReadRomWord(ROM_LINEAR_BASE + ROMHDR_ROOT);
    if (base != ROM_LINEAR_BASE) return -1;
    if (size < 0x00100000u || size > 0x04000000u) return -1;
    if (root < base || root >= base + size) return -1;
    *aBase = base;
    *aSize = size;
    return KErrNone;
}

// ── File names ──────────────────────────────────────────────────────

static const TUint16 KTitle[] = u"ROM dump";
static const TUint16 KPartName[] = u"C:\\ROMDUMP.001";
static const TUint16 KProgressName[] = u"C:\\ROMDUMP.PRG";
static const TUint16 KReportName[] = u"C:\\ROMDUMP.TXT";

static void Copy16(TUint16 *aDst, const TUint16 *aSrc, TInt aLen)
{
    TInt i;
    for (i = 0; i < aLen; i++) aDst[i] = aSrc[i];
}

static TInt Len16(const TUint16 *aStr)
{
    TInt n = 0;
    while (aStr[n]) n++;
    return n;
}

// Fills aBuf with the part file's name and points aDes at it.
static void PartName(TUint16 *aBuf, TPtrC_ *aDes, TInt aPart)
{
    TInt len = Len16(KPartName);
    Copy16(aBuf, KPartName, len);
    aBuf[len - 3] = (TUint16)('0' + (aPart / 100) % 10);
    aBuf[len - 2] = (TUint16)('0' + (aPart / 10) % 10);
    aBuf[len - 1] = (TUint16)('0' + aPart % 10);
    aDes->iTypeLength = TYPE_LEN(EPtrC, (TUint)len);
    aDes->iPtr = aBuf;
}

static void FixedName(TPtrC_ *aDes, const TUint16 *aName)
{
    aDes->iTypeLength = TYPE_LEN(EPtrC, (TUint)Len16(aName));
    aDes->iPtr = aName;
}

// ── Progress file ───────────────────────────────────────────────────

// Read before every batch, not just once a run: it is the only record of
// how far the dump has got, and re-reading it is what makes the next
// batch — or the next run, after the parts have been copied off — carry
// on instead of starting again.
static TInt ReadNextPart(RFs_ *aFs)
{
    RFile_ f;
    TPtrC_ name;
    TPtr_ des;
    TUint8 buf[4];
    TInt err;

    FixedName(&name, KProgressName);
    f.iHandle = 0; f.iSubSessionHandle = 0;
    err = RFile_Open(&f, aFs, &name, 0 /* EFileRead|EFileShareExclusive */);
    if (err != KErrNone) return 1;
    des.iTypeLength = TYPE_LEN(EPtr, 0);
    des.iMaxLength = 4;
    des.iPtr = buf;
    err = RFile_Read(&f, &des);
    RFile_Close(&f);
    if (err != KErrNone || DES_LEN(des.iTypeLength) != 4) return 1;
    {
        TInt part = (TInt)((TUint)buf[0] | ((TUint)buf[1] << 8) |
                           ((TUint)buf[2] << 16) | ((TUint)buf[3] << 24));
        return (part >= 1 && part <= 999) ? part : 1;
    }
}

static TInt WriteNextPart(RFs_ *aFs, TInt aPart)
{
    RFile_ f;
    TPtrC_ name, des;
    TUint8 buf[4];
    TInt err;

    buf[0] = (TUint8)aPart; buf[1] = (TUint8)(aPart >> 8);
    buf[2] = (TUint8)(aPart >> 16); buf[3] = (TUint8)(aPart >> 24);
    FixedName(&name, KProgressName);
    f.iHandle = 0; f.iSubSessionHandle = 0;
    err = RFile_Replace(&f, aFs, &name, EFileWrite);
    if (err != KErrNone) return err;
    des.iTypeLength = TYPE_LEN(EPtrC, 4);
    des.iPtr = buf;
    err = RFile_Write(&f, &des);
    RFile_Close(&f);
    return err;
}

// ── The report ──────────────────────────────────────────────────────

// The same account the console gives, left on the disk for afterwards.
// It is UCS-2 because the machine is: EShell's `type` reads a text file
// as 16-bit characters, so an 8-bit one comes out as garbage.
static void WriteReport(RFs_ *aFs)
{
    RFile_ f;
    TPtrC_ name, des;
    // The longest report — every count at its widest, and the two lines
    // a failed read-back writes — is around 360 characters.
    TUint16 buf[512];
    TInt at = 0;

    at = AppendText(buf, at, "Psion EPOC ER5u ROM dump\r\n\r\nROM base   0x");
    at = AppendHex(buf, at, gResult.iRomBase, 8);
    at = AppendText(buf, at, "\r\nROM size   0x");
    at = AppendHex(buf, at, gResult.iRomSize, 8);
    at = AppendText(buf, at, " (");
    at = AppendDec(buf, at, gResult.iRomSize >> 10);
    at = AppendText(buf, at, " KB)\r\nPart size  ");
    at = AppendDec(buf, at, PART_BYTES >> 10);
    at = AppendText(buf, at, " KB\r\nParts      ");
    at = AppendDec(buf, at, (gResult.iRomSize + PART_BYTES - 1u) / PART_BYTES);
    at = AppendText(buf, at, "\r\nLast batch ");
    if (gResult.iPartsWritten == 0) {
        at = AppendText(buf, at, "nothing");
    } else {
        at = AppendText(buf, at, "parts ");
        at = AppendDec(buf, at, (TUint)gResult.iFirstPart);
        at = AppendText(buf, at, "..");
        at = AppendDec(buf, at, (TUint)(gResult.iFirstPart + (TInt)gResult.iPartsWritten - 1));
        at = AppendText(buf, at, ", ");
        at = AppendDec(buf, at, gResult.iBytesWritten >> 10);
        at = AppendText(buf, at, " KB");
    }
    at = AppendText(buf, at, "\r\nNext part  ");
    if (gResult.iStep == STEP_DONE) at = AppendText(buf, at, "none, the ROM is all written");
    else at = AppendDec(buf, at, (TUint)gResult.iNextPart);
    at = AppendText(buf, at, "\r\nVerified   ");
    at = AppendDec(buf, at, gResult.iPartsVerified);
    at = AppendText(buf, at, " parts read back");
    if (gResult.iError == ERR_MISMATCH) {
        at = AppendText(buf, at, ", MISMATCH at 0x");
        at = AppendHex(buf, at, gResult.iBadAt, 8);
    } else if (gResult.iPartsVerified == gResult.iPartsWritten) {
        at = AppendText(buf, at, ", all match the ROM");
    }
    at = AppendText(buf, at, "\r\nResult     ");
    if (gResult.iError == KErrNone && gResult.iStep == STEP_DONE) {
        at = AppendText(buf, at, "complete");
    } else if (gResult.iError == KErrNone) {
        at = AppendText(buf, at, "stopped, more parts to write");
    } else if (gResult.iError == ERR_MISMATCH) {
        at = AppendText(buf, at, "a part does not match the ROM");
    } else if (gResult.iError == ERR_SHORT_READ) {
        at = AppendText(buf, at, "a part is shorter than it should be");
    } else {
        at = AppendText(buf, at, "error ");
        at = AppendDec(buf, at, (TUint)(-gResult.iError));
        at = AppendText(buf, at, " at step ");
        at = AppendDec(buf, at, (TUint)gResult.iStep);
    }
    at = AppendText(buf, at, "\r\n\r\nJoin the parts back together in order to\r\n"
                             "rebuild the ROM image.\r\n");

    FixedName(&name, KReportName);
    f.iHandle = 0; f.iSubSessionHandle = 0;
    if (RFile_Replace(&f, aFs, &name, EFileWrite) != KErrNone) return;
    // RFile::Write takes 8-bit descriptors, so the text goes out as the
    // bytes of those characters: 2 per character, little-endian.
    des.iTypeLength = TYPE_LEN(EPtrC, (TUint)at * 2u);
    des.iPtr = buf;
    RFile_Write(&f, &des);
    RFile_Close(&f);
}

// ── One part ────────────────────────────────────────────────────────

static TInt WritePart(RFs_ *aFs, TInt aPart, TUint aFrom, TUint aBytes)
{
    RFile_ f;
    TUint16 nameBuf[24];
    TPtrC_ name, des;
    TUint done = 0;
    TInt chunks = 0;
    TInt err;

    PartName(nameBuf, &name, aPart);
    f.iHandle = 0; f.iSubSessionHandle = 0;
    err = RFile_Replace(&f, aFs, &name, EFileWrite);
    if (err != KErrNone) return err;
    while (done < aBytes) {
        TUint n = aBytes - done;
        if (n > CHUNK_BYTES) n = CHUNK_BYTES;
        des.iTypeLength = TYPE_LEN(EPtrC, n);
        des.iPtr = (const void *)(aFrom + done);
        err = RFile_Write(&f, &des);
        if (err != KErrNone) break;
        done += n;
        gResult.iBytesWritten += n;
        if (++chunks % DOT_CHUNKS == 0) Say(".");
    }
    RFile_Close(&f);
    return err;
}

// Reads a part back off the disk and compares it with the ROM it came
// from. A dump nobody checked is worth very little — this is the one
// thing the machine can tell you that a size in a directory listing
// cannot — and it costs only what the read costs, on the same imports
// the write already needs.
static TInt VerifyPart(RFs_ *aFs, TInt aPart, TUint aFrom, TUint aBytes)
{
    RFile_ f;
    TUint16 nameBuf[24];
    TPtrC_ name;
    TPtr_ des;
    TUint8 buf[VERIFY_BYTES];
    TUint done = 0;
    TInt err;

    PartName(nameBuf, &name, aPart);
    f.iHandle = 0; f.iSubSessionHandle = 0;
    err = RFile_Open(&f, aFs, &name, 0 /* EFileRead|EFileShareExclusive */);
    if (err != KErrNone) return err;
    while (done < aBytes) {
        const TUint8 *rom = (const TUint8 *)(aFrom + done);
        TUint want = aBytes - done;
        TUint got, i;
        if (want > sizeof(buf)) want = sizeof(buf);
        des.iTypeLength = TYPE_LEN(EPtr, 0);
        des.iMaxLength = (TInt)want;
        des.iPtr = buf;
        err = RFile_Read(&f, &des);
        if (err != KErrNone) break;
        got = DES_LEN(des.iTypeLength);
        if (got == 0) { err = ERR_SHORT_READ; break; }
        for (i = 0; i < got; i++) {
            if (buf[i] != rom[i]) {
                gResult.iBadAt = aFrom + done + i;
                err = ERR_MISMATCH;
                break;
            }
        }
        if (err != KErrNone) break;
        done += got;
    }
    RFile_Close(&f);
    return err;
}

// ── What the user sees ──────────────────────────────────────────────

static void SayPart(const char *aLead, TInt aPart, TInt aLast)
{
    TUint16 buf[64];
    TInt at = AppendText(buf, 0, aLead);
    at = AppendDec(buf, at, (TUint)aPart);
    at = AppendText(buf, at, " of ");
    at = AppendDec(buf, at, (TUint)aLast);
    Print(buf, at);
}

static void SayStatus(TUint aRomBase, TUint aRomSize, TInt aNext, TInt aLast)
{
    TUint16 buf[128];
    TInt at = AppendText(buf, 0, "\nROM at 0x");
    at = AppendHex(buf, at, aRomBase, 8);
    at = AppendText(buf, at, " is ");
    at = AppendDec(buf, at, aRomSize >> 10);
    at = AppendText(buf, at, " KB: ");
    at = AppendDec(buf, at, (TUint)aLast);
    at = AppendText(buf, at, " parts of ");
    at = AppendDec(buf, at, PART_BYTES >> 10);
    at = AppendText(buf, at, " KB\n");
    Print(buf, at);
    if (aNext > aLast) {
        Say("Every part is already on C:.\n"
            "Delete C:\\ROMDUMP.PRG to dump the ROM again.\n");
    } else {
        SayPart("Next is part ", aNext, aLast);
        Say(", written to C:\\ROMDUMP.nnn\n");
    }
}

// How many parts are left decides what the prompt offers: there is no
// point asking for 9 when 4 remain, and a single digit cannot ask for
// more than 9 whatever the ROM's size.
static void SayMenu(TInt aRemaining, TInt aMaxDigit)
{
    TUint16 buf[80];
    TInt at;
    if (aRemaining == 1) {
        Say("Write the last part now?  1 = yes, Q = quit: ");
        return;
    }
    if (aRemaining <= 9) {
        at = AppendText(buf, 0, "Write how many of the ");
        at = AppendDec(buf, at, (TUint)aRemaining);
        at = AppendText(buf, at, " left?  1-");
        at = AppendDec(buf, at, (TUint)aMaxDigit);
        at = AppendText(buf, at, ", Q = quit: ");
    } else {
        at = AppendText(buf, 0, "Write how many now?  1-9, A = all ");
        at = AppendDec(buf, at, (TUint)aRemaining);
        at = AppendText(buf, at, ", Q = quit: ");
    }
    Print(buf, at);
}

static void SayInstructions(void)
{
    Say("\nTo get the dump off this machine:\n"
        " 1 copy C:\\ROMDUMP.* to a PC (PsiWin, or beam them)\n"
        " 2 delete the parts you have copied\n"
        " 3 run this again for the parts still to write\n"
        " 4 on the PC, join them in the right order:\n"
        "   copy /b ROMDUMP.001+ROMDUMP.002+... rom.img\n"
        "C:\\ROMDUMP.TXT keeps the same summary.\n");
}

// ── E32Main ─────────────────────────────────────────────────────────

TInt RomDumpMain(void)
{
    RFs_ fs;
    TPtrC_ title;
    TUint romBase = 0, romSize = 0;
    TInt lastPart, err;

    gResult.iMagic = RESULT_MAGIC;
    gResult.iStep = STEP_START;

    // The console is made the way the ROM's own EShell makes one, with a
    // cleanup stack under it for the allocations that go into it. If the
    // machine will not give us one the dump still runs — silently, with
    // the report on the disk as its only account.
    FixedName(&title, KTitle);
    if (CleanupStack_New()) gConsole = Console_NewL(&title, -1, -1);
    Say("Psion EPOC ER5u ROM dump\n");

    if (FindRom(&romBase, &romSize) != KErrNone) {
        Say("No EPOC ROM header at 0x50000000 - stopping.\n");
        gResult.iError = -1;
        WaitToClose();
        return -1;
    }
    gResult.iRomBase = romBase;
    gResult.iRomSize = romSize;
    gResult.iStep = STEP_SCANNED;

    fs.iHandle = 0;
    err = RFs_Connect(&fs, -1);
    if (err != KErrNone) {
        Say("Cannot reach the file server - stopping.\n");
        gResult.iError = err;
        WaitToClose();
        return err;
    }
    gResult.iStep = STEP_CONNECTED;
    lastPart = (TInt)((romSize + PART_BYTES - 1u) / PART_BYTES);

    for (;;) {
        TInt part = ReadNextPart(&fs);      // re-read: this is the resume
        TInt want, key;

        gResult.iNextPart = part;
        SayStatus(romBase, romSize, part, lastPart);
        if (part > lastPart) {
            gResult.iStep = STEP_DONE;
            break;
        }
        {
            const TInt remaining = lastPart - part + 1;
            const TInt maxDigit = remaining > 9 ? 9 : remaining;
            want = 0;
            for (;;) {
                SayMenu(remaining, maxDigit);
                key = Key();
                Say("\n");
                if (key >= '1' && key <= '0' + maxDigit) { want = key - '0'; break; }
                if (key == 'a' || key == 'A') { want = remaining; break; }
                if (key == 'q' || key == 'Q' || key == 27) break;
                // Anything else — including a key held long enough to
                // repeat into the next prompt — just asks again.
            }
            if (want == 0) break;
        }

        // Only now, so a pass that writes nothing (the last one round,
        // which finds the dump finished) leaves the batch it is
        // reporting on alone.
        gResult.iFirstPart = part;
        gResult.iPartsWritten = 0;
        gResult.iPartsVerified = 0;
        gResult.iBytesWritten = 0;
        for (; part <= lastPart && want > 0; part++, want--) {
            TUint from = romBase + (TUint)(part - 1) * PART_BYTES;
            TUint bytes = romSize - (TUint)(part - 1) * PART_BYTES;
            if (bytes > PART_BYTES) bytes = PART_BYTES;

            SayPart("Part ", part, lastPart);
            Say(" ");
            gResult.iStep = STEP_WRITING;
            err = WritePart(&fs, part, from, bytes);
            if (err != KErrNone) {
                Say(" cannot write it - the disk is probably full.\n");
                gResult.iError = err;
                break;
            }
            gResult.iPartsWritten++;
            gResult.iStep = STEP_VERIFYING;
            err = VerifyPart(&fs, part, from, bytes);
            if (err != KErrNone) {
                Say(" DOES NOT MATCH THE ROM - stopping.\n");
                gResult.iError = err;
                break;
            }
            gResult.iPartsVerified++;
            gResult.iNextPart = part + 1;
            WriteNextPart(&fs, gResult.iNextPart);
            Say(" written and checked.\n");
        }
        if (gResult.iError != KErrNone) break;
    }

    if (gResult.iError == KErrNone && gResult.iNextPart > lastPart) {
        gResult.iStep = STEP_DONE;
        Say("\nThe whole ROM is on C:, and every part matches it.\n");
    }
    SayInstructions();
    WriteReport(&fs);
    WaitToClose();
    return gResult.iError;
}
