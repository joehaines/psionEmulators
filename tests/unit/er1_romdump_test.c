// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// What tools/romdump-er1/romdump.c does, run on this machine instead of
// on a Psion.
//
// The program itself is compiled unchanged — the same source the ARM
// build compiles — against a stand-in for the six EFSRV calls it
// imports and a stand-in ROM mapped at 0x50000000, which is where it
// looks. That makes everything except the instruction set real: the
// part sizes, the read-back, the progress file, the drive it picks,
// what it writes into the report.
//
// The cases are the ones a Psion actually puts it through:
//
//   1  a card with room for the whole ROM — one run, every part checked
//   2  a RAM disk with room for two and a bit — the run stops where the
//      disk does, and the *next* run carries on from that exact byte.
//      This is the case the ER5u dumper got wrong on a real machine: a
//      part cut short by a full disk looked like a whole one, and 2.8 MB
//      went missing from a dump that joined up perfectly (see
//      docs/conan-rom-dumping.md).
//   3  a card that is there but takes nothing — the dump moves to C:
//   4  a disk that gives back something other than what was written —
//      the run stops, and does not count the part as done
//   5  a finished dump — running it again changes nothing
//
//   bash tests/unit/run-er1-romdump-test.sh

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

// ── The stand-in ROM ────────────────────────────────────────────────

#define ROM_BASE 0x50000000u
#define ROM_SIZE (6u * 1024u * 1024u)       // a Series 5's 6 MB
#define ROM_MAP  (16u * 1024u * 1024u)      // room for a real ROM image too

static unsigned char *gRom;

static void MakeRom(unsigned aSize)
{
    unsigned i;
    unsigned int x = 0x13572468u;
    gRom = mmap((void *)(unsigned long)ROM_BASE, ROM_MAP, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (gRom != (unsigned char *)(unsigned long)ROM_BASE) {
        fprintf(stderr, "cannot map a stand-in ROM at 0x%x\n", ROM_BASE);
        exit(2);
    }
    for (i = 0; i < aSize; i++) {           // xorshift, so every byte is checkable
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        gRom[i] = (unsigned char)x;
    }
    *(unsigned *)(gRom + 0x8c) = ROM_BASE;          // TRomHeader::iRomBase
    *(unsigned *)(gRom + 0x90) = aSize;             // iRomSize
    *(unsigned *)(gRom + 0x94) = ROM_BASE + 0x1000; // iRomRootDirectoryList
}

// A real machine's ROM, put where the program looks for one. The
// program's own self-check then walks that ROM's directory, finds its
// EFSrv.dll and works out which export is which — the thing that has to
// work on a prototype whose ordinals nobody has measured. Doing it here
// rather than only on the emulator turns a three-minute round trip into
// a millisecond, and covers every R1 ROM in the repository.
static int LoadRealRom(const char *aPath)
{
    FILE *f = fopen(aPath, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(gRom, 1, ROM_MAP, f);
    fclose(f);
    return n > 0x1000;
}

// ── The stand-in file server ────────────────────────────────────────

#define KErrNotFound (-1)
#define KErrNotReady (-18)
#define KErrDiskFull (-22)

typedef struct { char iName[24]; unsigned char *iData; unsigned iSize; } FakeFile;
typedef struct { char iLetter; int iPresent; unsigned iFree; } FakeDrive;

#define MAX_FILES 64
#define LOG_READ 9000
static FakeFile gFiles[MAX_FILES];
static int gFileCount;
static FakeDrive gDrives[2];
static int gCorruptPart;            // the part number the disk gives back wrong
static unsigned gWritten;

static FakeDrive *DriveOf(const char *aName)
{
    int i;
    for (i = 0; i < 2; i++) if (gDrives[i].iLetter == aName[0]) return &gDrives[i];
    return 0;
}

static FakeFile *FileNamed(const char *aName)
{
    int i;
    for (i = 0; i < gFileCount; i++) if (strcmp(gFiles[i].iName, aName) == 0) return &gFiles[i];
    return 0;
}

static void DeleteFile(const char *aName)
{
    FakeFile *f = FileNamed(aName);
    FakeDrive *d = DriveOf(aName);
    int i;
    if (!f) return;
    d->iFree += f->iSize;
    free(f->iData);
    i = (int)(f - gFiles);
    memmove(&gFiles[i], &gFiles[i + 1], (size_t)(gFileCount - i - 1) * sizeof(FakeFile));
    gFileCount--;
}

// The descriptors and handles the program uses, in the same shape.
typedef struct { unsigned iTypeLength; const void *iPtr; } TPtrC8_;
typedef struct { unsigned iTypeLength; int iMaxLength; void *iPtr; } TPtr8_;
typedef struct { int iHandle; int iPad[3]; } RFs_;
typedef struct { int iHandle; int iSubSessionHandle; int iPad[2]; } RFile_;

typedef struct { FakeFile *iFile; unsigned iPos; int iWritable; } Open;
static Open gOpen[8];

static void NameOf(const TPtrC8_ *aDes, char *aOut)
{
    unsigned n = aDes->iTypeLength & 0x0fffffffu;
    if (n > 23) n = 23;
    memcpy(aOut, aDes->iPtr, n);
    aOut[n] = 0;
}

int RFs_Connect(RFs_ *aFs, int aSlots)
{
    if (aSlots != 4) { fprintf(stderr, "FAIL: RFs::Connect(%d), the ROM's own callers pass 4\n", aSlots); exit(1); }
    aFs->iHandle = 1;
    return 0;
}

static int OpenSlot(RFile_ *aFile, FakeFile *aF, int aWritable)
{
    int i;
    for (i = 1; i < 8; i++) if (!gOpen[i].iFile) break;
    if (i == 8) return KErrNotFound;
    gOpen[i].iFile = aF; gOpen[i].iPos = 0; gOpen[i].iWritable = aWritable;
    aFile->iHandle = 1;
    aFile->iSubSessionHandle = i;
    return 0;
}

int RFile_Replace(RFile_ *aFile, RFs_ *aFs, const TPtrC8_ *aName, unsigned aMode)
{
    char name[24];
    FakeDrive *d;
    FakeFile *f;
    (void)aFs;
    if (aMode != 0x200u) { fprintf(stderr, "FAIL: RFile::Replace mode 0x%x, not EFileWrite\n", aMode); exit(1); }
    NameOf(aName, name);
    d = DriveOf(name);
    if (!d || !d->iPresent) return KErrNotReady;
    DeleteFile(name);                       // Replace truncates whatever was there
    if (gFileCount == MAX_FILES) return KErrDiskFull;
    f = &gFiles[gFileCount++];
    memset(f, 0, sizeof(*f));
    strcpy(f->iName, name);
    return OpenSlot(aFile, f, 1);
}

int RFile_Open(RFile_ *aFile, RFs_ *aFs, const TPtrC8_ *aName, unsigned aMode)
{
    char name[24];
    FakeDrive *d;
    FakeFile *f;
    (void)aFs;
    if (aMode != 0u) { fprintf(stderr, "FAIL: RFile::Open mode 0x%x, not EFileRead\n", aMode); exit(1); }
    NameOf(aName, name);
    d = DriveOf(name);
    if (!d || !d->iPresent) return KErrNotReady;
    f = FileNamed(name);
    if (!f) return KErrNotFound;
    return OpenSlot(aFile, f, 0);
}

int RFile_Write(RFile_ *aFile, const TPtrC8_ *aDes)
{
    Open *o = &gOpen[aFile->iSubSessionHandle];
    FakeDrive *d = DriveOf(o->iFile->iName);
    unsigned n = aDes->iTypeLength & 0x0fffffffu;
    unsigned room = d->iFree < n ? d->iFree : n;
    unsigned char *grown;
    if (!o->iWritable) { fprintf(stderr, "FAIL: wrote to a file opened for reading\n"); exit(1); }
    if (n == 0) return 0;
    // A disk that fills up part way through a write keeps what fitted,
    // which is exactly the case the program has to survive.
    grown = realloc(o->iFile->iData, o->iFile->iSize + room);
    if (!grown) return KErrDiskFull;
    o->iFile->iData = grown;
    memcpy(o->iFile->iData + o->iFile->iSize, aDes->iPtr, room);
    if (gCorruptPart) {
        char want[24];
        sprintf(want, "%c:\\ROMDUMP.%03d", o->iFile->iName[0], gCorruptPart);
        if (strcmp(o->iFile->iName, want) == 0 && o->iFile->iSize == 0) {
            o->iFile->iData[0] ^= 0xff;              // the disk gives back something else
        }
    }
    o->iFile->iSize += room;
    d->iFree -= room;
    gWritten += room;
    return room == n ? 0 : KErrDiskFull;
}

int RFile_Read(RFile_ *aFile, TPtr8_ *aDes)
{
    Open *o = &gOpen[aFile->iSubSessionHandle];
    unsigned want = (unsigned)aDes->iMaxLength;
    unsigned have = o->iFile->iSize - o->iPos;
    unsigned n = want < have ? want : have;
    memcpy(aDes->iPtr, o->iFile->iData + o->iPos, n);
    o->iPos += n;
    aDes->iTypeLength = (2u << 28) | n;
    return 0;
}

void RFile_Close(RFile_ *aFile)
{
    if (aFile->iSubSessionHandle) gOpen[aFile->iSubSessionHandle].iFile = 0;
    aFile->iHandle = 0;
    aFile->iSubSessionHandle = 0;
}

int RomDumpMain(void);
int RomDumpCheckOnly(RFs_ *aFs);

// The kernel's executive calls. On a Psion these are single SVC
// instructions (tools/romdump-er1/start.S); here they are stand-ins
// that fail, because this machine has no EPOC kernel under it — which
// is the case the program has to handle anyway, and is what sends it
// on to the library path. The dumping above therefore goes through the
// stand-in file server, exactly as before.
int ExecSendReceive(unsigned aFunction, unsigned *aArgs, int *aStatus, int aHandle)
{ (void)aFunction; (void)aArgs; (void)aStatus; (void)aHandle; return -5; }
int ExecCreateObject(int aSelector, unsigned *aArgs, int *aStatus)
{ (void)aSelector; (void)aArgs; (void)aStatus; return -5; }
void ExecWaitForAnyRequest(void) { }
void ExecRequestSignal(int aCount) { (void)aCount; }

// ── Checking ────────────────────────────────────────────────────────

static int gFailures;

static void Check(int aOk, const char *aWhat)
{
    printf("%s %s\n", aOk ? "ok  " : "FAIL", aWhat);
    if (!aOk) gFailures++;
}

static void Reset(int aDPresent, unsigned aDFree, unsigned aCFree)
{
    int i;
    for (i = 0; i < gFileCount; i++) free(gFiles[i].iData);
    gFileCount = 0;
    memset(gOpen, 0, sizeof(gOpen));
    gDrives[0].iLetter = 'D'; gDrives[0].iPresent = aDPresent; gDrives[0].iFree = aDFree;
    gDrives[1].iLetter = 'C'; gDrives[1].iPresent = 1; gDrives[1].iFree = aCFree;
    gCorruptPart = 0;
    gWritten = 0;
}

// Joins every part on a drive, in order, and says how many bytes of the
// ROM they are — and whether they are the ROM.
static unsigned JoinedBytes(char aDrive, int *aMatches)
{
    unsigned at = 0;
    int part;
    *aMatches = 1;
    for (part = 1; part <= 999; part++) {
        char name[24];
        FakeFile *f;
        sprintf(name, "%c:\\ROMDUMP.%03d", aDrive, part);
        f = FileNamed(name);
        if (!f) break;
        if (at + f->iSize > ROM_SIZE || memcmp(f->iData, gRom + at, f->iSize) != 0) *aMatches = 0;
        at += f->iSize;
    }
    return at;
}

static const char *Report(char aDrive)
{
    char name[24];
    static char text[8192];
    FakeFile *f;
    sprintf(name, "%c:\\ROMDUMP.TXT", aDrive);
    f = FileNamed(name);
    if (!f) return "";
    memcpy(text, f->iData, f->iSize < sizeof(text) - 1 ? f->iSize : sizeof(text) - 1);
    text[f->iSize < sizeof(text) - 1 ? f->iSize : sizeof(text) - 1] = 0;
    return text;
}

static const char *LogText_(char aDrive)
{
    char name[24];
    static char text[LOG_READ];
    FakeFile *f;
    sprintf(name, "%c:\\ROMDUMP.LOG", aDrive);
    f = FileNamed(name);
    if (!f) return "";
    memcpy(text, f->iData, f->iSize < sizeof(text) - 1 ? f->iSize : sizeof(text) - 1);
    text[f->iSize < sizeof(text) - 1 ? f->iSize : sizeof(text) - 1] = 0;
    return text;
}

static int PartCount(char aDrive)
{
    int part, n = 0;
    for (part = 1; part <= 999; part++) {
        char name[24];
        sprintf(name, "%c:\\ROMDUMP.%03d", aDrive, part);
        if (FileNamed(name)) n++;
    }
    return n;
}

// What the owner does between runs: copy the parts off and delete them,
// keeping the progress file.
static void TakeThePartsOff(char aDrive, unsigned *aKept)
{
    int part;
    for (part = 1; part <= 999; part++) {
        char name[24];
        FakeFile *f;
        sprintf(name, "%c:\\ROMDUMP.%03d", aDrive, part);
        f = FileNamed(name);
        if (!f) continue;
        *aKept += f->iSize;
        DeleteFile(name);
    }
}

int main(void)
{
    MakeRom(ROM_SIZE);

    // ── 1. A card with room for all of it ───────────────────────────
    {
        int matches;
        unsigned joined;
        Reset(1, 16u * 1024 * 1024, 1024 * 1024);
        Check(RomDumpMain() == 0, "a card with room: the run reports no error");
        joined = JoinedBytes('D', &matches);
        Check(joined == ROM_SIZE, "a card with room: the parts add up to the whole ROM");
        Check(matches, "a card with room: every byte of every part is the ROM's");
        Check(PartCount('D') == 6, "a card with room: a 6 MB ROM is six 1 MB parts");
        Check(PartCount('C') == 0, "a card with room: nothing is written to C:");
        Check(strstr(Report('D'), "complete") != 0, "a card with room: the report says complete");
        Check(strstr(Report('D'), "Written to  D:\\ROMDUMP.nnn") != 0, "a card with room: the report names the drive");
    }

    // ── 2. A RAM disk with room for two and a bit ───────────────────
    {
        unsigned kept = 0;
        int runs = 0, matches;
        Reset(0, 0, 2u * 1024 * 1024 + 512 * 1024);
        while (runs < 10) {
            RomDumpMain();
            runs++;
            if (strstr(Report('C'), "the whole ROM is written")) break;
            Check(strstr(Report('C'), "the disk is full") != 0,
                  "a full RAM disk: the run that stops says the disk is full");
            TakeThePartsOff('C', &kept);        // copied off, deleted, room again
        }
        TakeThePartsOff('C', &kept);
        Check(runs == 3, "a full RAM disk: 6 MB in 2.5 MB of disk takes three runs");
        Check(kept == ROM_SIZE, "a full RAM disk: the runs together wrote the whole ROM");
        // And the parts really do join up: dump it again in one go on a
        // roomy drive and compare the two.
        Reset(1, 16u * 1024 * 1024, 1024 * 1024);
        RomDumpMain();
        JoinedBytes('D', &matches);
        Check(matches, "a full RAM disk: the parts a resumed dump leaves are the ROM, in order");
    }

    // ── 3. A card that is there but takes nothing ───────────────────
    {
        int matches;
        Reset(1, 900, 16u * 1024 * 1024);       // room for a report, not for a part
        RomDumpMain();
        Check(PartCount('D') <= 1, "a card with no room: it is not filled with parts");
        Check(JoinedBytes('C', &matches) == ROM_SIZE && matches,
              "a card with no room: the dump goes to C: instead, whole");
        Check(strstr(Report('C'), "complete") != 0, "a card with no room: C:'s report says complete");
        // And the part the card was left with does not draw the next
        // run back to it: nothing was recorded there, so the dump on C:
        // is the one that gets picked up.
        RomDumpMain();
        Check(JoinedBytes('C', &matches) == ROM_SIZE && matches,
              "a card with no room: a later run still finds the dump on C:");
    }

    // ── 3b. A disk too small for even one whole part ────────────────
    {
        unsigned kept = 0;
        int runs = 0, matches;
        Reset(0, 0, 400 * 1024);                // 400 KB, parts are 1 MB
        while (runs < 40) {
            RomDumpMain();
            runs++;
            if (strstr(Report('C'), "the whole ROM is written")) break;
            TakeThePartsOff('C', &kept);
        }
        TakeThePartsOff('C', &kept);
        Check(kept == ROM_SIZE,
              "a disk smaller than one part: run by run, it still writes the whole ROM");
        Reset(1, 16u * 1024 * 1024, 1024 * 1024);
        RomDumpMain();
        JoinedBytes('D', &matches);
        Check(matches, "a disk smaller than one part: and what it wrote joins up");
    }

    // ── 4. A disk that gives back something else ────────────────────
    {
        unsigned offset;
        char name[24];
        FakeFile *prg;
        Reset(1, 16u * 1024 * 1024, 1024 * 1024);
        gCorruptPart = 3;                       // part 3 comes back wrong
        RomDumpMain();
        Check(strstr(Report('D'), "did not read back the same as the ROM") != 0,
              "a lying disk: the report says a part did not match");
        sprintf(name, "D:\\ROMDUMP.PRG");
        prg = FileNamed(name);
        Check(prg != 0 && prg->iSize == 32, "a lying disk: the progress file is still 32 bytes");
        offset = prg ? *(unsigned *)(prg->iData + 12) : 0;
        Check(offset == 2u * 1024 * 1024,
              "a lying disk: the dump does not count the part that did not match");
    }

    // ── 5. A dump that is already finished ──────────────────────────
    {
        int matches;
        unsigned before;
        Reset(1, 16u * 1024 * 1024, 1024 * 1024);
        RomDumpMain();
        before = JoinedBytes('D', &matches);
        RomDumpMain();
        Check(JoinedBytes('D', &matches) == before && matches,
              "a finished dump: running it again leaves the parts alone");
        Check(strstr(Report('D'), "nothing - the whole ROM is written") != 0,
              "a finished dump: the report says there is nothing left to do");
    }

    // ── 6. A machine whose ROM header does not check out ────────────
    {
        unsigned saved = *(unsigned *)(gRom + 0x90);
        Reset(1, 16u * 1024 * 1024, 1024 * 1024);
        *(unsigned *)(gRom + 0x90) = 0x12345;       // not a length a ROM has
        RomDumpMain();
        Check(PartCount('D') == 0 && PartCount('C') == 0,
              "a bad ROM header: nothing is dumped");
        Check(strstr(Report('D'), "no EPOC ROM header at 0x50000000") != 0,
              "a bad ROM header: the report says why");
        *(unsigned *)(gRom + 0x90) = saved;
    }

    // ── 7. The log ─────────────────────────────────────────────────
    {
        const char *log;
        Reset(1, 16u * 1024 * 1024, 1024 * 1024);
        RomDumpMain();
        log = Report('D');                      /* same reader, different file */
        log = LogText_('D');
        Check(log[0] != 0, "a log is written");
        Check(strstr(log, "connect to the file server rc=0") != 0,
              "the log records the call that has to work first");
        Check(strstr(log, "part 1 from ROM 0x50000000") != 0,
              "the log records each part and where it came from");
        Check(strstr(log, "read back 1024 KB of 1024 KB, rc=0") != 0,
              "the log records the read-back of each part");
        Check(strstr(log, "EFSrv.dll NOT FOUND") != 0,
              "and says so when the machine has no ROM directory to check");
        printf("\n--- D:\\ROMDUMP.LOG (first 1200 bytes) ---\n%.1200s\n--- end ---\n\n", log);
    }

    // ── 8. The self-check, against real R1 ROMs ────────────────────
    {
        static const char *kRoms[] = {
            "roms/S5_v1.00(113)_eng.bin",       /* the Series 5 prototype */
            "roms/series5_v1.01(144)_eng.bin",  /* the shipping Series 5 */
            "roms/Geofox_v1.01(146)_eng.bin",   /* the Geofox One */
        };
        static const char *kCalls[] = {
            "iClose", "iConnect", "iOpen", "iReplace", "iRead", "iWrite",
        };
        unsigned r, c;
        for (r = 0; r < sizeof(kRoms) / sizeof(kRoms[0]); r++) {
            char what[160];
            const char *log;
            unsigned romSize;
            if (!LoadRealRom(kRoms[r])) {
                printf("skip %s — not present\n", kRoms[r]);
                continue;
            }
            romSize = *(unsigned *)(gRom + 0x90);
            Reset(1, 32u * 1024 * 1024, 1024 * 1024);
            {
                RFs_ fs;
                fs.iHandle = 1;
                RomDumpCheckOnly(&fs);     /* the check only: see its comment */
            }
            log = LogText_('D');
            sprintf(what, "%s: the self-check finds EFSrv.dll in the ROM", kRoms[r]);
            Check(strstr(log, "EFSrv.dll at 0x") != 0 &&
                  strstr(log, "uid3=0x100000BD") != 0, what);
            for (c = 0; c < sizeof(kCalls) / sizeof(kCalls[0]); c++) {
                char want[64];
                sprintf(want, "  %s linked=", kCalls[c]);
                {
                    const char *line = strstr(log, want);
                    int confirmed = line && strstr(line, "confirmed at 0x") != 0 &&
                                    (strchr(line, '\r') == 0 ||
                                     strstr(line, "confirmed at 0x") < strchr(line, '\r'));
                    sprintf(what, "%s: %s is confirmed against the machine's own ROM",
                            kRoms[r], kCalls[c]);
                    Check(confirmed, what);
                }
            }
            sprintf(what, "%s: the ROM it read is the %u KB one", kRoms[r], romSize >> 10);
            {
                char want[40];
                sprintf(want, "size=0x%08X", romSize);
                Check(strstr(log, want) != 0, what);
            }
            if (r == 0) {
                printf("\n--- the self-check on %s ---\n", kRoms[r]);
                { const char *e = strstr(log, "progress on"); 
                  printf("%.*s--- end ---\n\n", e ? (int)(e - log) : 600, log); }
            }
        }
        MakeRom(ROM_SIZE);                 /* put the stand-in ROM back */
    }

    // What the owner of a machine actually reads, once.
    {
        Reset(1, 16u * 1024 * 1024, 1024 * 1024);
        RomDumpMain();
        printf("\n--- D:\\ROMDUMP.TXT after a complete dump ---\n%s", Report('D'));
        printf("--- end ---\n\n");
    }

    printf("%s\n", gFailures ? "FAILURES" : "all checks pass");
    return gFailures ? 1 : 0;
}
