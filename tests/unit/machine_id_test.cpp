// ETNA identity-PROM machine-ID test.
//
// EPOC shows this PROM's 32-bit word at offset 0x18 as the low half of
// "Unique id" in the System screen's Machine information dialog, and the
// frontend's "Show debugging" panel can reprogram it (EmuBase::setMachineId
// → Etna::setMachineId). Three things have to hold for that to reach EPOC:
//
//   1. The serial protocol has to match what the kernel clocks out: a
//      command frame (start bit, READ opcode, 6-bit word index) then 16
//      data bits — with however many idle clocks the ROM puts in front of
//      the start bit. The 5mx sends none and the MC218 sends one, so a
//      decoder that counts a fixed-width address swallows a data clock on
//      one of them and shifts every word it returns. That is what made
//      EPOC reject the whole image and report a zero machine ID.
//   2. The new ID must be visible through that reader — the ONLY path the
//      guest has to the PROM — little-endian at offset 0x18.
//   3. The image's trailing XOR checksum must be re-folded, or the
//      kernel's validity check reads the PROM as corrupt and falls back to
//      its "no identity chip" defaults, which is exactly the failure mode
//      a naive byte poke would produce.
//
// This test drives the guest-side interface only, then reconstructs all
// 128 PROM bytes from it and re-checks the checksum, so it also covers
// the device-name bytes surviving an ID write.
//
// Build via harness/build.sh. Run:
//   ./tests/unit/machine_id_test  -> exit 0 on pass, non-zero on fail.

#include "../../core/emubase.h"
#include "../../core/etna.h"
#include "../../core/arm710.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char *msg) {
    if (!cond) {
        std::printf("  FAIL: %s\n", msg);
        g_failures++;
    }
}

// Minimal ARM710 so Etna has an owner for its log() / getGPR() calls.
// Logging is off by default (ARM710::log short-circuits), so nothing is
// emitted and no memory is ever touched.
class StubCpu : public ARM710 {
public:
    StubCpu() : ARM710(true) {}
    MaybeU32 readPhysical(uint32_t, ValueSize) override { return {}; }
    bool writePhysical(uint32_t, uint32_t, ValueSize) override { return false; }
};

constexpr uint32_t kRegWake1 = 0x0C;

// Reads one 16-bit PROM word exactly the way the 5mx kernel does over
// Windermere's port B (ECust export 44, traced against v1.05(260)): bit 0
// high starts the transfer, then NINE pulses of bit 1 clock in a start bit,
// the 2-bit READ opcode (10) and a 6-bit word index — each preceded by a
// write of that bit into wake1 bit 2 — and sixteen more pulses clock the
// value out through wake1 bit 3, MSB first. 25 pulses per word.
//
// The pulse count is the point of this helper: an implementation that
// expects a 10-bit address swallows the first data clock, which shifts
// every word and silently invalidates the whole image.
constexpr int kFrameBits = 9;
constexpr uint16_t kReadCmd = 0x6;   // start=1, opcode=10, MSB-first

// `leadingZeros` is the idle padding the ROM clocks before the start bit:
// 0 on the 5mx (25 pulses per word), 1 on the MC218 (26).
uint16_t readPromWord(Etna &etna, uint8_t wordIndex, int leadingZeros = 0) {
    const uint16_t frame = (uint16_t)((kReadCmd << 6) | (wordIndex & 0x3F));
    etna.setPromBit0High();
    for (int i = 0; i < leadingZeros; i++) {
        etna.writeReg8(kRegWake1, 0x00);
        etna.setPromBit1High();
    }
    for (int b = kFrameBits - 1; b >= 0; b--) {
        etna.writeReg8(kRegWake1, ((frame >> b) & 1) ? 0x04 : 0x00);
        etna.setPromBit1High();
    }
    uint16_t value = 0;
    for (int b = 0; b < 16; b++) {
        etna.setPromBit1High();
        value = (uint16_t)((value << 1) | ((etna.readReg8(kRegWake1) >> 3) & 1));
    }
    etna.setPromBit0Low();
    return value;
}

// The 32-bit ID as the guest assembles it: little-endian at offset 0x18,
// i.e. the two words at indices 0x0C and 0x0D. EPOC shows this as the low
// half of Machine information's "Unique id" (the high half is the model's
// machine UID, which comes from the ROM).
uint32_t readPromMachineId(Etna &etna) {
    const uint32_t lo = readPromWord(etna, Etna::kPromMachineId / 2);
    const uint32_t hi = readPromWord(etna, Etna::kPromMachineId / 2 + 1);
    return lo | (hi << 16);
}

// Pulls the whole 128-byte image back through the bit-bang reader — the
// same 64-word sweep the kernel does before it XOR-checks the image.
void readPromImage(Etna &etna, uint8_t out[0x80]) {
    for (uint8_t w = 0; w < 0x40; w++) {
        const uint16_t v = readPromWord(etna, w);
        out[w * 2]     = (uint8_t)(v & 0xFF);
        out[w * 2 + 1] = (uint8_t)(v >> 8);
    }
}

// The kernel's check: XOR of bytes 0x00..0x7E, folded against 66, equals
// the byte at 0x7F.
bool checksumValid(const uint8_t image[0x80]) {
    uint8_t chk = 0;
    for (int i = 0; i < 0x7F; i++) chk ^= image[i];
    return image[0x7F] == (uint8_t)(chk ^ 66);
}

// Device name as stored: length at 0x28, then XOR-encoded ASCII.
void decodeDeviceName(const uint8_t image[0x80], char *out, size_t outSize) {
    const char *key = "PSIONPSIONPSION";
    size_t len = image[0x28];
    if (len > 15) len = 15;
    if (len > outSize - 1) len = outSize - 1;
    for (size_t i = 0; i < len; i++) out[i] = (char)(image[0x29 + i] ^ key[i]);
    out[len] = '\0';
}

} // namespace

int main() {
    StubCpu cpu;
    Etna etna(&cpu);

    std::printf("== default machine ID ==\n");
    check(etna.getMachineId() == Etna::kDefaultMachineId,
          "getMachineId() returns the factory default");
    check(readPromMachineId(etna) == Etna::kDefaultMachineId,
          "guest reads the factory default over the PROM bit-bang");

    uint8_t image[0x80];
    readPromImage(etna, image);
    check(checksumValid(image), "factory image checksum is valid");
    char name[16];
    decodeDeviceName(image, name, sizeof(name));
    check(std::strcmp(name, "PockEmul") == 0, "factory device name is readable");

    std::printf("== reprogrammed machine ID ==\n");
    // 0xDEADBEEF exercises every byte lane, and its top bit set catches a
    // signed-shift slip in the LE packing.
    etna.setMachineId(0xDEADBEEFu);
    check(etna.getMachineId() == 0xDEADBEEFu, "getMachineId() returns the new ID");
    check(readPromMachineId(etna) == 0xDEADBEEFu,
          "guest reads the new ID over the PROM bit-bang");

    readPromImage(etna, image);
    check(checksumValid(image), "checksum re-folded after the ID write");
    decodeDeviceName(image, name, sizeof(name));
    check(std::strcmp(name, "PockEmul") == 0, "device name survives the ID write");

    std::printf("== ID / device name are independent ==\n");
    etna.setDeviceName("Series 5mx");
    readPromImage(etna, image);
    check(etna.getMachineId() == 0xDEADBEEFu, "ID survives a device-name write");
    check(checksumValid(image), "checksum still valid after a device-name write");
    decodeDeviceName(image, name, sizeof(name));
    check(std::strcmp(name, "Series 5mx") == 0, "new device name is readable");

    std::printf("== frame padding: the ROMs disagree, both must work ==\n");
    // The 5mx clocks the command frame with no idle bits in front of the
    // start bit; the MC218 sends one first. A decoder that counts a fixed
    // number of address bits reads one of them a clock out of step and
    // returns shifted words, which invalidates the whole image. Reading
    // the same word both ways must give the same answer.
    etna.setMachineId(0x12345678u);
    {
        const uint8_t idx = Etna::kPromMachineId / 2;
        const uint16_t noPad   = readPromWord(etna, idx, 0);   // 5mx
        const uint16_t onePad  = readPromWord(etna, idx, 1);   // MC218
        const uint16_t manyPad = readPromWord(etna, idx, 5);   // any other ROM
        check(noPad == 0x5678, "unpadded frame reads the ID word");
        check(onePad == noPad, "one leading idle clock reads the same word");
        check(manyPad == noPad, "several leading idle clocks read the same word");
    }
    {
        // And the whole image, so a padding-sensitive decoder can't pass by
        // getting one word right.
        uint8_t padded[0x80];
        for (uint8_t w = 0; w < 0x40; w++) {
            const uint16_t v = readPromWord(etna, w, 1);
            padded[w * 2]     = (uint8_t)(v & 0xFF);
            padded[w * 2 + 1] = (uint8_t)(v >> 8);
        }
        readPromImage(etna, image);
        check(std::memcmp(padded, image, sizeof(image)) == 0,
              "the full 64-word sweep is identical with and without padding");
        check(checksumValid(padded), "padded sweep still passes the checksum");
    }

    std::printf("== edge values ==\n");
    etna.setMachineId(0);
    check(etna.getMachineId() == 0 && readPromMachineId(etna) == 0, "ID 0 round-trips");
    readPromImage(etna, image);
    check(checksumValid(image), "checksum valid with an all-zero ID");
    etna.setMachineId(0xFFFFFFFFu);
    check(etna.getMachineId() == 0xFFFFFFFFu && readPromMachineId(etna) == 0xFFFFFFFFu,
          "ID 0xFFFFFFFF round-trips");
    readPromImage(etna, image);
    check(checksumValid(image), "checksum valid with an all-ones ID");

    // ── The ROM half, on a machine whose OS lives on the CF card ───────
    //
    // The 5mx Pro and netBook boot a bootloader flash and load their OS
    // (SYS$ROM.BIN / D:\OS.IMG) off a card, so the model UID EPOC prints
    // is in the CARD image, not in ROM[]. Patching ROM[] on those two
    // changed nothing the user ever saw — the first eight digits snapped
    // straight back. findCardMachineIdWords is what locates the copies to
    // rewrite, and it has to keep working on a card that a previous
    // session already patched (which no longer carries the factory value).
    std::printf("== model UID on an OS card ==\n");
    {
        constexpr uint32_t kFactory = 0x1000118Au;
        constexpr uint32_t kSet     = 0x0BADF00Du;
        auto put = [](uint8_t *p, uint32_t v) {
            p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
            p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
        };
        auto get = [](const uint8_t *p) {
            return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
                 | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        };
        uint8_t card[256] = {};
        std::vector<size_t> offs;

        // A fresh card: two copies of the factory constant, one of them
        // deliberately at a word boundary next to unrelated data.
        put(card + 0x10, kFactory);
        put(card + 0x80, kFactory);
        put(card + 0x84, 0xDEADBEEFu);
        EmuBase::findCardMachineIdWords(card, sizeof(card), kFactory, kFactory, offs);
        check(offs.size() == 2 && offs[0] == 0x10 && offs[1] == 0x80,
              "both factory copies located on a fresh card");
        EmuBase::writeRomWords(card, offs, kSet);
        check(get(card + 0x10) == kSet && get(card + 0x80) == kSet,
              "every located copy is rewritten");
        check(get(card + 0x84) == 0xDEADBEEFu, "neighbouring words are left alone");

        // The same card next session: it carries the programmed value now,
        // so a factory-only scan would find nothing and the id would go
        // back to snapping to the ROM's.
        std::vector<size_t> again;
        EmuBase::findCardMachineIdWords(card, sizeof(card), kFactory, kSet, again);
        check(again == offs, "an already-patched card is located by its current value");

        // A half-patched card (one copy rewritten, one not) reports both,
        // exactly once each — the set is what gets rewritten together.
        put(card + 0x10, kFactory);
        std::vector<size_t> mixed;
        EmuBase::findCardMachineIdWords(card, sizeof(card), kFactory, kSet, mixed);
        check(mixed == offs, "factory and programmed copies merge without duplicates");

        // Nothing to find is not an error; it just means this card carries
        // no OS image (an empty slot, or a plain data card).
        uint8_t blank[64] = {};
        std::vector<size_t> none;
        EmuBase::findCardMachineIdWords(blank, sizeof(blank), kFactory, kSet, none);
        check(none.empty(), "a card with no copy of the constant yields no offsets");
    }

    if (g_failures) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll machine-ID checks passed.\n");
    return 0;
}
