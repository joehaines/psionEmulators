// NetpadMmcCard protocol test.
//
// Drives the MMC-over-SPI card model exactly the way the netpad ROM's two
// drivers of the board FPGA's SPI port do, and asserts the answers they
// depend on:
//
//   1. Nothing is driven while chip select is high — the 80 initialisation
//      clocks the variant sends before it selects the card must read back
//      as an idle bus.
//   2. The variant's card-init handshake works: CMD0 answers R1 = 0x01
//      (in idle state) and CMD1 answers R1 = 0x00 (ready), each after at
//      least one byte of 0xFF so the driver's own trailing command byte
//      doesn't swallow the response.
//   3. CMD9 returns a CSD whose capacity matches the image, behind the
//      0xFE start token and followed by a correct CRC16.
//   4. CMD17 returns the right 512 bytes for a byte-addressed argument.
//   5. CMD24 writes a block into the image, and reading it back returns
//      what was written.
//   6. CMD18 streams consecutive blocks until CMD12 stops it.
//
// Build: part of harness/build.sh.  Run: tests/unit/mmc_card_test

#include "../../core/netpad_mmc.h"
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

// Clock one byte, discarding the reply.
static void put(NetpadMmcCard &c, uint8_t b) { (void)c.transfer(b); }

// Send a six-byte command frame.  The CRC byte is whatever a real host
// would send; the card doesn't check it (SPI-mode CRC is off until CMD59,
// which no netpad driver sends), and passing a deliberately wrong one here
// documents that.
static void sendCmd(NetpadMmcCard &c, uint8_t index, uint32_t arg,
                    uint8_t crc = 0xFF) {
    put(c, (uint8_t)(0x40 | index));
    put(c, (uint8_t)(arg >> 24));
    put(c, (uint8_t)(arg >> 16));
    put(c, (uint8_t)(arg >> 8));
    put(c, (uint8_t)arg);
    put(c, crc);
}

// Clock 0xFF until the card drives something other than the idle level,
// up to `limit` bytes.  Returns 0xFF if it never answered.
static uint8_t waitResponse(NetpadMmcCard &c, int limit = 16) {
    for (int i = 0; i < limit; i++) {
        const uint8_t b = c.transfer(0xFF);
        if (b != 0xFF) return b;
    }
    return 0xFF;
}

// Clock 0xFF until the card releases the bus, the way a host has to
// before it can issue the next command (a card still shifting a response
// or programming a block would eat the command frame).
static void settle(NetpadMmcCard &c, int limit = 64) {
    for (int i = 0; i < limit; i++) if (c.transfer(0xFF) == 0xFF) return;
}

// A 1 MB image whose every block is stamped with its own LBA, so a
// misdirected read is obvious.
static std::vector<uint8_t> makeImage(uint32_t blocks) {
    std::vector<uint8_t> img((size_t)blocks * NetpadMmcCard::kBlockSize, 0);
    for (uint32_t lba = 0; lba < blocks; lba++) {
        uint8_t *p = img.data() + (size_t)lba * NetpadMmcCard::kBlockSize;
        for (int i = 0; i < 16; i++) p[i] = (uint8_t)(lba + i);
        p[NetpadMmcCard::kBlockSize - 1] = (uint8_t)(lba ^ 0xA5);
    }
    return img;
}

int main() {
    constexpr uint32_t kBlocks = 2048;                 // 1 MB
    constexpr uint32_t kBs = NetpadMmcCard::kBlockSize;
    std::vector<uint8_t> img = makeImage(kBlocks);

    NetpadMmcCard card;
    check(!card.inserted(), "empty card reports no media");
    card.attach(img.data(), img.size());
    check(card.inserted(), "card reports media after attach");
    check(card.imageSize() == img.size(), "image size round-trips");

    std::printf("== deselected bus ==\n");
    for (int i = 0; i < 10; i++)
        check(card.transfer(0xFF) == 0xFF, "deselected card drives nothing");

    std::printf("== CMD0 / CMD1 handshake ==\n");
    card.setChipSelect(true);
    sendCmd(card, 0, 0, 0x95);
    check(waitResponse(card) == 0x01, "CMD0 answers R1 = in-idle-state");
    sendCmd(card, 1, 0);
    check(waitResponse(card) == 0x00, "CMD1 answers R1 = ready");

    // The response must not land on the byte immediately after the frame:
    // medmmc.pdd clocks one trailing 0xFF of its own and throws it away.
    sendCmd(card, 13, 0);
    check(card.transfer(0xFF) == 0xFF, "response is at least one byte late");
    check(card.transfer(0xFF) == 0x00, "CMD13 answers R2 byte 1");
    check(card.transfer(0xFF) == 0x00, "CMD13 answers R2 byte 2");
    settle(card);

    std::printf("== CMD9 (SEND_CSD) ==\n");
    sendCmd(card, 9, 0);
    check(waitResponse(card) == 0x00, "CMD9 answers R1 = ok");
    check(waitResponse(card) == 0xFE, "CMD9 sends the start-block token");
    uint8_t csd[16];
    for (int i = 0; i < 16; i++) csd[i] = card.transfer(0xFF);
    uint16_t crc = (uint16_t)(card.transfer(0xFF) << 8);
    crc |= card.transfer(0xFF);
    check(crc == NetpadMmcCard::crc16(csd, 16), "CSD data CRC16 is correct");
    {
        uint8_t expect[16];
        card.buildCsd(expect);
        check(std::memcmp(csd, expect, 16) == 0, "CSD matches the card's own");
        // capacity = (C_SIZE + 1) << (C_SIZE_MULT + 2) blocks
        const uint32_t cSize = (uint32_t)(((csd[6] & 0x03) << 10) |
                                          (csd[7] << 2) | (csd[8] >> 6));
        const int mult = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);
        const uint64_t cap = (uint64_t)(cSize + 1) << (mult + 2);
        check(cap == kBlocks, "CSD capacity matches the image");
        check(((csd[5] & 0x0F) == 9), "CSD READ_BL_LEN is 512 bytes");
    }

    std::printf("== CMD17 (READ_SINGLE_BLOCK) ==\n");
    for (uint32_t lba : {0u, 1u, 17u, kBlocks - 1}) {
        sendCmd(card, 17, lba * kBs);              // byte address
        check(waitResponse(card) == 0x00, "CMD17 answers R1 = ok");
        check(waitResponse(card) == 0xFE, "CMD17 sends the start-block token");
        std::vector<uint8_t> got(kBs);
        for (uint32_t i = 0; i < kBs; i++) got[i] = card.transfer(0xFF);
        uint16_t c = (uint16_t)(card.transfer(0xFF) << 8);
        c |= card.transfer(0xFF);
        check(std::memcmp(got.data(), img.data() + (size_t)lba * kBs, kBs) == 0,
              "CMD17 returns the right block");
        check(c == NetpadMmcCard::crc16(got.data(), kBs),
              "CMD17 data CRC16 is correct");
        settle(card);
    }

    std::printf("== CMD24 (WRITE_BLOCK) ==\n");
    {
        const uint32_t lba = 42;
        std::vector<uint8_t> payload(kBs);
        for (uint32_t i = 0; i < kBs; i++) payload[i] = (uint8_t)(i * 7 + 3);
        sendCmd(card, 24, lba * kBs);
        check(waitResponse(card) == 0x00, "CMD24 answers R1 = ok");
        put(card, 0xFE);                            // data start token
        for (uint32_t i = 0; i < kBs; i++) put(card, payload[i]);
        const uint16_t pc = NetpadMmcCard::crc16(payload.data(), kBs);
        put(card, (uint8_t)(pc >> 8));
        put(card, (uint8_t)pc);
        const uint8_t resp = waitResponse(card);
        check((resp & 0x1F) == 0x05, "CMD24 accepts the data packet");
        // The card holds MISO low while it programs; poll it out the way
        // medmmc.pdd does before issuing the next command.
        settle(card);

        sendCmd(card, 17, lba * kBs);
        check(waitResponse(card) == 0x00, "read-back answers R1 = ok");
        check(waitResponse(card) == 0xFE, "read-back sends the start token");
        std::vector<uint8_t> got(kBs);
        for (uint32_t i = 0; i < kBs; i++) got[i] = card.transfer(0xFF);
        (void)card.transfer(0xFF); (void)card.transfer(0xFF);
        check(got == payload, "written block reads back byte for byte");
        check(std::memcmp(card.imageData() + (size_t)lba * kBs,
                          payload.data(), kBs) == 0,
              "written block landed in the image the frontend reads back");
        settle(card);
    }

    std::printf("== CMD18 / CMD12 (READ_MULTIPLE_BLOCK) ==\n");
    {
        const uint32_t first = 100;
        sendCmd(card, 18, first * kBs);
        check(waitResponse(card) == 0x00, "CMD18 answers R1 = ok");
        for (uint32_t n = 0; n < 3; n++) {
            check(waitResponse(card) == 0xFE, "CMD18 sends a start token");
            std::vector<uint8_t> got(kBs);
            for (uint32_t i = 0; i < kBs; i++) got[i] = card.transfer(0xFF);
            (void)card.transfer(0xFF); (void)card.transfer(0xFF);
            check(std::memcmp(got.data(), img.data() + (size_t)(first + n) * kBs,
                              kBs) == 0, "CMD18 streams consecutive blocks");
        }
        // CMD12 goes out while the card is already shifting the next
        // block, exactly as on real hardware; the card finishes that
        // block and then answers.  Clock long enough for one block plus
        // the response, then check the bus has gone quiet for good.
        sendCmd(card, 12, 0);
        int quietRun = 0;
        for (int i = 0; i < (int)kBs * 3; i++)
            quietRun = (card.transfer(0xFF) == 0xFF) ? quietRun + 1 : 0;
        check(quietRun >= 64, "the stream stops after CMD12");
    }

    std::printf("== CMD58 (READ_OCR) ==\n");
    {
        sendCmd(card, 58, 0);
        check(waitResponse(card) == 0x00, "CMD58 answers R1 = ok");
        const uint8_t o0 = card.transfer(0xFF);
        (void)card.transfer(0xFF); (void)card.transfer(0xFF);
        (void)card.transfer(0xFF);
        check((o0 & 0x80) != 0, "OCR reports the card powered up");
    }

    std::printf("== CSD capacity across the frontend's size presets ==\n");
    for (uint32_t mb : {4u, 8u, 16u, 32u, 64u, 128u}) {
        const uint32_t blocks = mb * 1024 * 1024 / kBs;
        std::vector<uint8_t> big((size_t)blocks * kBs, 0);
        NetpadMmcCard c2;
        c2.attach(big.data(), big.size());
        uint8_t d[16];
        c2.buildCsd(d);
        const uint32_t cSize = (uint32_t)(((d[6] & 0x03) << 10) |
                                          (d[7] << 2) | (d[8] >> 6));
        const int mult = ((d[9] & 0x03) << 1) | (d[10] >> 7);
        const uint64_t cap = (uint64_t)(cSize + 1) << (mult + 2);
        check(cap == blocks, "CSD capacity matches a preset-sized image");
        check(d[15] == (uint8_t)((NetpadMmcCard::crc7(d, 15) << 1) | 1),
              "CSD CRC7 is correct");
    }

    std::printf("== eject ==\n");
    card.detach();
    check(!card.inserted(), "detach clears card-present");
    card.setChipSelect(true);
    sendCmd(card, 0, 0, 0x95);
    check(waitResponse(card) == 0xFF, "an empty slot never answers");

    if (g_failures == 0) {
        std::printf("mmc_card_test: PASS\n");
        return 0;
    }
    std::printf("mmc_card_test: FAIL (%d checks)\n", g_failures);
    return 1;
}
