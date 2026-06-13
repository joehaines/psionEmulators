// VCFCard write-path test.
//
// Reproduces the EPOC pccd_ata WRITE SECTORS sequence at the VCFCard level and
// asserts the contract that the "Disk corrupt on copy/format" fix depends on:
//
//   1. Issuing a WRITE SECTORS command must NOT raise IREQ# (real ATA asserts
//      DRQ only; IREQ# comes after the data is written).  A spurious
//      post-command IREQ that survives the driver's "clear any pending
//      interrupt" Status read fires the moment interrupts are enabled — before
//      the host has pushed the sector — and the driver aborts the write.
//   2. The completion IREQ# IS raised once a sector's data has been flushed.
//   3. The bytes the host pushes actually land in the image at the right LBA.
//
// The sequence mirrors DPcCardMediaDriverAta::InitiateWriteCommand():
//   IssueAtaCommand(WRITE) -> WaitForNotBusy (poll Status) -> read Status to
//   clear any pending IRQ -> (enable card IRQ) -> push 512 bytes.
//
// Run for BOTH faithfulMode=false (Series 7 / booted netBook OS) and
// faithfulMode=true (netBook bootloader read path) so the write path is correct
// regardless of the ATA-semantics mode.

#include "../../core/vcfcard.h"
#include <cstdio>
#include <cstring>
#include <vector>

static int g_failures = 0;

static void check(bool cond, const char *msg) {
    if (!cond) {
        std::printf("  FAIL: %s\n", msg);
        g_failures++;
    }
}

// Drive a single WRITE SECTORS of `lba` with a known pattern and verify the
// contract above.  Returns by mutating g_failures.
static void testWriteOneSector(bool faithful) {
    std::printf("== WRITE SECTORS, faithfulMode=%s ==\n", faithful ? "true" : "false");

    // 64 KiB blank image, distinct fill so we can see the write land.
    std::vector<uint8_t> img(64 * 1024, 0xAA);
    VCFCard cf;
    cf.setFaithfulMode(faithful);
    cf.attach(img.data(), img.size());

    const uint32_t lba = 7;

    // --- IssueAtaCommand(KAtaCmdWriteSectors, lba, 1) ---
    cf.ataWrite8(0x6, 0xE0 | ((lba >> 24) & 0x0F)); // drive/head: LBA mode, dev0
    cf.ataWrite8(0x3, (uint8_t)(lba));              // LBA[7:0]
    cf.ataWrite8(0x4, (uint8_t)(lba >> 8));         // LBA[15:8]
    cf.ataWrite8(0x5, (uint8_t)(lba >> 16));        // LBA[23:16]
    cf.ataWrite8(0x2, 1);                           // sector count
    cf.ataWrite8(0x7, 0x30);                        // WRITE SECTORS

    // --- WaitForNotBusy(): poll Status until BSY clears ---
    for (int i = 0; i < 8; i++) {
        uint8_t s = cf.ataRead8(0x7);
        if (!(s & 0x80)) break; // BSY clear
    }

    // --- AtaRegister8(KAtaStatusRd8): "Clear any pending interrupt" ---
    cf.ataRead8(0x7);

    // The driver now enables the card IRQ.  At THIS point there must be no
    // interrupt pending — otherwise it fires before any data is pushed.  Tick
    // the assertion delay right out so a latent irqPending would surface.
    cf.tickIrqDelay(1000000);
    check(!cf.irqAsserted(),
          "spurious IREQ# asserted after WRITE command (before data) — "
          "this is the copy/format corruption");

    // --- EmptySectBufferToDrive(): push the 512-byte sector (16-bit PIO) ---
    for (int w = 0; w < 256; w++) {
        // pattern: low byte = w, high byte = w ^ 0x5A
        uint16_t v = (uint16_t)((uint8_t)w | ((uint8_t)(w ^ 0x5A) << 8));
        cf.ataWrite16(v);
    }

    // After the sector is flushed the card asserts the completion IREQ#.
    cf.tickIrqDelay(1000000);
    check(cf.irqAsserted(), "completion IREQ# not asserted after sector write");

    // Reading Status acks the completion IRQ and reports command-complete.
    uint8_t finalStatus = cf.ataRead8(0x7);
    check((finalStatus & 0x01) == 0, "ERR set in Status after a clean write");
    check(!cf.irqAsserted(), "IREQ# still asserted after Status-read ack");

    // The bytes must have landed at lba*512 — and nowhere else.
    const uint8_t *out = cf.data();
    bool dataOk = true;
    for (int b = 0; b < 512; b++) {
        uint8_t expect = (b & 1) ? (uint8_t)((b >> 1) ^ 0x5A) : (uint8_t)(b >> 1);
        if (out[lba * 512 + b] != expect) { dataOk = false; break; }
    }
    check(dataOk, "written sector bytes did not land correctly at the LBA");
    check(out[(lba - 1) * 512] == 0xAA && out[(lba + 1) * 512] == 0xAA,
          "write spilled into adjacent sectors");
}

// Two-sector write: each sector must produce its own completion IREQ# and land
// at consecutive LBAs (the multi-sector copy path).
static void testWriteTwoSectors(bool faithful) {
    std::printf("== WRITE SECTORS x2, faithfulMode=%s ==\n", faithful ? "true" : "false");

    std::vector<uint8_t> img(64 * 1024, 0x11);
    VCFCard cf;
    cf.setFaithfulMode(faithful);
    cf.attach(img.data(), img.size());

    const uint32_t lba = 3;
    cf.ataWrite8(0x6, 0xE0);
    cf.ataWrite8(0x3, (uint8_t)lba);
    cf.ataWrite8(0x4, 0);
    cf.ataWrite8(0x5, 0);
    cf.ataWrite8(0x2, 2); // two sectors
    cf.ataWrite8(0x7, 0x30);

    for (int i = 0; i < 8; i++) { if (!(cf.ataRead8(0x7) & 0x80)) break; }
    cf.ataRead8(0x7); // clear-any-pending
    cf.tickIrqDelay(1000000);
    check(!cf.irqAsserted(), "spurious IREQ# before data (2-sector)");

    for (int s = 0; s < 2; s++) {
        for (int w = 0; w < 256; w++)
            cf.ataWrite16((uint16_t)(0xC000 + s * 256 + w));
        cf.tickIrqDelay(1000000);
        check(cf.irqAsserted(), "per-sector completion IREQ# missing (2-sector)");
        cf.ataRead8(0x7); // ack between sectors
    }

    const uint8_t *out = cf.data();
    bool ok = true;
    for (int s = 0; s < 2 && ok; s++)
        for (int w = 0; w < 256; w++) {
            uint16_t v = (uint16_t)(0xC000 + s * 256 + w);
            uint32_t off = (lba + s) * 512 + w * 2;
            if (out[off] != (uint8_t)v || out[off + 1] != (uint8_t)(v >> 8)) { ok = false; break; }
        }
    check(ok, "2-sector write data did not land at consecutive LBAs");
}

int main() {
    for (bool faithful : {false, true}) {
        testWriteOneSector(faithful);
        testWriteTwoSectors(faithful);
    }
    if (g_failures == 0) {
        std::printf("\nALL CF WRITE TESTS PASSED\n");
        return 0;
    }
    std::printf("\n%d CF WRITE TEST(S) FAILED\n", g_failures);
    return 1;
}
