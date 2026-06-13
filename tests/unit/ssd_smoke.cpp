// SSD frame-protocol round-trip — instantiates a PsionSSD with a 128K
// RAM image and drives the SIBO control / data frame sequence the way
// EPOC16 does to read the info byte and write/read SSD bytes.
//
// Build via harness/build.sh (the SSD module is now in SOURCES). Run:
//   ./tests/ssd_smoke  -> exit 0 on pass, non-zero with a message on fail.

#include "../../core/psion_ssd.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

// Convenience helpers so the test code reads like the SIBO protocol.
constexpr uint16_t CTRL(uint8_t b) { return PsionSSD::CONTROL_FRAME | b; }
constexpr uint16_t DATA(uint8_t b) { return PsionSSD::DATA_FRAME    | b; }

// Latch a 17-bit address (within a 128K device, mem_width=17) by:
//   1. CONTROL 0x82 + DATA(0x02) — set Port B mode = latch
//   2. CONTROL 0x81 + DATA(addr & 0xFF)               — Port B latches LSB
//   3. CONTROL 0x93 + DATA((addr>>8) & 0xFF) + DATA(0)
//      Multi Port D and C writes — D latches mid byte, C latches high
//      byte. For a 128K image we only need 17 address bits, so the
//      Port C high byte is zero.
void latchAddress(PsionSSD &ssd, uint32_t addr) {
    ssd.writeFrame(CTRL(0x82));   ssd.writeFrame(DATA(0x02));   // latch mode
    ssd.writeFrame(CTRL(0x81));   ssd.writeFrame(DATA(addr & 0xFF));
    ssd.writeFrame(CTRL(0x93));
    ssd.writeFrame(DATA((addr >> 8)  & 0xFF));
    ssd.writeFrame(DATA((addr >> 16) & 0xFF));
}

#define CHECK(expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#define CHECK_EQ(actual, expected) do { \
    auto _a = (actual); auto _e = (expected); \
    if (_a != _e) { \
        std::fprintf(stderr, "FAIL %s:%d: %s == %s, got 0x%llx, want 0x%llx\n", \
            __FILE__, __LINE__, #actual, #expected, \
            (unsigned long long)_a, (unsigned long long)_e); \
        return 1; \
    } \
} while (0)

int run() {
    // 128K RAM pack (info byte 0x03, mem_width 17, single device).
    std::vector<uint8_t> image(0x20000, 0);

    PsionSSD ssd;
    bool doorAsserted = false;
    ssd.setDoorCb([&](bool s) { if (s) doorAsserted = true; });

    CHECK(ssd.attach(image.data(), image.size()));
    CHECK(ssd.isInserted());
    CHECK_EQ(ssd.imageSize(), image.size());
    CHECK(doorAsserted);

    // 1) Read the info byte via SerialSelect Asic5PackId (control 0x42:
    //    SerialSelect | PackId, with bit 0 CLEAR to match PACK_MODE=0
    //    (the PC6 pin is low for pack-mode devices per MAME's psion_asic5.h
    //    enum pc6_state { PACK_MODE = 0, PERIPHERAL_MODE = 1 }).
    ssd.writeFrame(CTRL(0x42));
    CHECK_EQ(ssd.readFrame(), uint8_t(0x03));   // 128K RAM info byte

    // 2) Latch address 0x12345 then write 0x5A via Port A.
    latchAddress(ssd, 0x12345);
    ssd.writeFrame(CTRL(0x80));   // SerialWrite Port A
    ssd.writeFrame(DATA(0x5A));
    CHECK_EQ(ssd.imageData()[0x12345], uint8_t(0x5A));

    // 3) Re-latch and read it back via Port A SerialRead.
    latchAddress(ssd, 0x12345);
    ssd.writeFrame(CTRL(0xC0));   // SerialRead Port A
    CHECK_EQ(ssd.readFrame(), uint8_t(0x5A));

    // 4) Counter mode: latch base address, then a sequence of Port A
    //    writes auto-advances the address LSB. Verify three sequential
    //    bytes hit consecutive offsets.
    //    First reset Port B to counter mode (default), then issue a
    //    fresh D-then-C with counter mode active, which zeroes
    //    port_b_counter so writes start from base + 0.
    ssd.writeFrame(CTRL(0x82));   ssd.writeFrame(DATA(0x00));   // counter mode
    ssd.writeFrame(CTRL(0x93));
    ssd.writeFrame(DATA(0x00));   // D = 0   (-> port_b_counter reset)
    ssd.writeFrame(DATA(0x01));   // C = 1   -> base = 0x010000
    ssd.writeFrame(CTRL(0x80));
    ssd.writeFrame(DATA(0xAA));   // writes at 0x010000, then ++counter
    ssd.writeFrame(DATA(0xBB));   // writes at 0x010001, then ++counter
    ssd.writeFrame(DATA(0xCC));   // writes at 0x010002
    CHECK_EQ(ssd.imageData()[0x010000], uint8_t(0xAA));
    CHECK_EQ(ssd.imageData()[0x010001], uint8_t(0xBB));
    CHECK_EQ(ssd.imageData()[0x010002], uint8_t(0xCC));

    // 5) Detach clears the info byte; SerialSelect now returns 0.
    doorAsserted = false;
    ssd.detach();
    CHECK(!ssd.isInserted());
    CHECK(doorAsserted);
    ssd.writeFrame(CTRL(0x43));
    CHECK_EQ(ssd.readFrame(), uint8_t(0x00));

    // 6) Bad sizes are rejected.
    CHECK(!ssd.attach(image.data(), 0x10001));      // not power of two
    CHECK(!ssd.attach(image.data(), 0x008000));     // 32K - below minimum
    CHECK(!ssd.attach(image.data(), 0x1000000));    // 16M - above maximum
    CHECK(!ssd.isInserted());

    // 7) Flash header detection: 0xF1A5 magic at offset 0 -> info byte
    //    OR'd with 0x20 (Type 1 Flash bit per the MAME info-byte table
    //    in reference/mame-psion/machine/psion_ssd.cpp). 0xE0 (the
    //    historical value here) is "Hardware write-protected SSD" —
    //    a system-disk type that EPOC16 doesn't mount as a user drive.
    image[0] = 0xA5; image[1] = 0xF1;
    CHECK(ssd.attach(image.data(), image.size()));
    ssd.writeFrame(CTRL(0x42));
    CHECK_EQ(ssd.readFrame(), uint8_t(0x23));       // 0x20 | 0x03

    // 8) Flash packs accept Port A writes into the backing store, the
    //    same as RAM. MAME's psion_ssd_device::writepa_handler writes
    //    unconditionally — the write-protect info bits only gate whether
    //    call_unload() flushes back to the host file, they do NOT block
    //    the live write. EPOC16 relies on this: when you copy a file
    //    onto a Flash SSD it streams the bytes through Port A and reads
    //    them straight back to verify, so a dropped write makes the
    //    verify fail and crashes the device. Latch a data address (well
    //    clear of the F1A5 header) and confirm the byte lands.
    latchAddress(ssd, 0x01000);
    ssd.writeFrame(CTRL(0x80));   // SerialWrite Port A
    ssd.writeFrame(DATA(0x3C));
    CHECK_EQ(ssd.imageData()[0x01000], uint8_t(0x3C));

    latchAddress(ssd, 0x01000);
    ssd.writeFrame(CTRL(0xC0));   // SerialRead Port A — verify read-back
    CHECK_EQ(ssd.readFrame(), uint8_t(0x3C));

    // 9) Explicit pack-type hints override content sniffing. On real
    //    packs the type comes from hardware straps, not the contents:
    //    a FEFS image (with magic) can be presented as a RAM pack, and
    //    EPOC16's behaviour differs per type (see psion_ssd.h Type).
    CHECK(ssd.attach(image.data(), image.size(), PsionSSD::Type::Ram));
    ssd.writeFrame(CTRL(0x42));
    CHECK_EQ(ssd.readFrame(), uint8_t(0x03));       // RAM info despite magic

    // 10) Protected type sets the write-protect bits (D7-D5 = 111) and
    //     drops Port A writes — the hardware write-enable strap is off.
    //     This keeps factory images pristine: EPOC16's slot-scan always
    //     fires one Port A write of 0x00 at address 0 before re-reading
    //     the header, which must NOT zap the 0xF1A5 magic.
    CHECK(ssd.attach(image.data(), image.size(), PsionSSD::Type::Protected));
    ssd.writeFrame(CTRL(0x42));
    CHECK_EQ(ssd.readFrame(), uint8_t(0xE3));       // 0xE0 | 0x03
    latchAddress(ssd, 0x00000);
    ssd.writeFrame(CTRL(0x80));   // SerialWrite Port A (the scan's dummy write)
    ssd.writeFrame(DATA(0x00));
    CHECK_EQ(ssd.imageData()[0], uint8_t(0xA5));    // magic survives

    std::fprintf(stderr, "PASS ssd_smoke\n");
    return 0;
}

} // namespace

int main() { return run(); }
