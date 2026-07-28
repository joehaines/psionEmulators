// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "netpad_mmc.h"

#include <algorithm>
#include <cstring>

namespace {

// ── MMC command set, as far as the netpad ROM drives it ──────────────
enum : uint8_t {
    kCmdGoIdleState        = 0,   // CMD0  — reset, enter SPI mode
    kCmdSendOpCond         = 1,   // CMD1  — leave idle; poll until R1 == 0
    kCmdAllSendCid         = 2,
    kCmdSendCsd            = 9,   // CMD9  — 16-byte CSD as a data packet
    kCmdSendCid            = 10,  // CMD10 — 16-byte CID as a data packet
    kCmdStopTransmission   = 12,  // CMD12 — end a multiple-block read
    kCmdSendStatus         = 13,  // CMD13 — R2 (two status bytes)
    kCmdSetBlockLen        = 16,
    kCmdReadSingleBlock    = 17,
    kCmdReadMultipleBlock  = 18,
    kCmdWriteBlock         = 24,
    kCmdWriteMultipleBlock = 25,
    kCmdProgramCsd         = 27,
    kCmdTagSectorStart     = 32,
    kCmdTagSectorEnd       = 33,
    kCmdUntagSector        = 34,
    kCmdTagEraseGroupStart = 35,
    kCmdTagEraseGroupEnd   = 36,
    kCmdUntagEraseGroup    = 37,
    kCmdErase              = 38,
    kCmdReadOcr            = 58,  // CMD58 — R3 (R1 + 32-bit OCR)
    kCmdCrcOnOff           = 59,
};

// R1 status bits.  Only "in idle state" and "illegal command" are ever
// set by a card this simple.
constexpr uint8_t kR1Ok            = 0x00;
constexpr uint8_t kR1IdleState     = 0x01;
constexpr uint8_t kR1IllegalCmd    = 0x04;

constexpr uint8_t kTokenStartBlock = 0xFE;  // single block / CMD9 / CMD10
constexpr uint8_t kTokenStartMulti = 0xFC;  // one block of a CMD25 write
constexpr uint8_t kTokenStopMulti  = 0xFD;  // end of a CMD25 write
constexpr uint8_t kDataResponseOk  = 0x05;  // xxx0_0101 — data accepted

// Bus level with nothing driving it.  A deselected card leaves MISO to
// the host's pull-up, and a selected card drives it high whenever it has
// nothing to say.
constexpr uint8_t kIdle = 0xFF;

// Response latency (Ncr).  A real card answers 1-8 bytes after the last
// command byte, and both ROM drivers depend on it being at least two:
// medmmc's command writer clocks one trailing 0xFF of its own after the
// six command bytes and throws the result away, then looks for the R1 in
// the bytes that follow.  Answering on the very next byte would put the
// R1 in the byte it discards.
constexpr int kResponseLatency = 1;

} // namespace

// ── CRCs ─────────────────────────────────────────────────────────────
// CRC7 (x^7 + x^3 + 1) over the command/register bytes, CRC16-CCITT
// (x^16 + x^12 + x^5 + 1) over data blocks.  Neither ROM driver checks
// them — SPI-mode CRC checking is off until CMD59 enables it and nothing
// sends CMD59 — but a card that emitted junk CRCs would be a trap for
// any future host, so we compute them properly.

uint8_t NetpadMmcCard::crc7(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc <<= 1;
            if ((b ^ crc) & 0x80) crc ^= 0x09;
            b <<= 1;
        }
    }
    return (uint8_t)(crc & 0x7F);
}

uint16_t NetpadMmcCard::crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++)
            crc = (uint16_t)((crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1));
    }
    return crc;
}

// ── Card registers ───────────────────────────────────────────────────

// CID: a plausible identity for an emulated card.  EPOC shows none of it
// (there is no "card info" page for the MMC slot), so the only field
// that has to be right is the CRC7 in the last byte.
void NetpadMmcCard::buildCid(uint8_t out[16]) const {
    std::memset(out, 0, 16);
    out[0] = 0x00;                       // MID — unassigned manufacturer
    out[1] = 'P'; out[2] = 'S';          // OID
    std::memcpy(out + 3, "NETPAD", 6);   // PNM — product name
    out[9]  = 0x10;                      // PRV 1.0
    out[10] = 0x00; out[11] = 0x00;      // PSN
    out[12] = 0x00; out[13] = 0x01;
    out[14] = 0x1A;                      // MDT — 2001, month 10
    out[15] = (uint8_t)((crc7(out, 15) << 1) | 1);
}

// CSD, MMC v1.1 layout.  Capacity is (C_SIZE + 1) << (C_SIZE_MULT + 2)
// blocks of 2^READ_BL_LEN bytes, so we pick READ_BL_LEN = 9 (512 B) and
// grow C_SIZE_MULT until C_SIZE fits its 12 bits — which covers every
// card size the frontend can build (the field tops out at 4 GB, well
// past the 2 GB FAT16 ceiling).
void NetpadMmcCard::buildCsd(uint8_t out[16]) const {
    std::memset(out, 0, 16);
    const uint32_t blocks = blockCount() ? blockCount() : 1;

    int mult = 0;                        // C_SIZE_MULT
    uint32_t cSize = 0;
    for (mult = 0; mult <= 7; mult++) {
        // blocks = (cSize + 1) * 2^(mult + 2)
        const uint32_t unit = 1u << (mult + 2);
        cSize = blocks / unit;
        if (cSize == 0) { cSize = 1; break; }
        cSize -= 1;
        if (cSize < 4096) break;
    }
    if (mult > 7) { mult = 7; cSize = 4095; }

    out[0] = (1 << 6) | (2 << 2);        // CSD_STRUCTURE 1.1, SPEC_VERS 2.x
    out[1] = 0x0F;                       // TAAC  — 1 ms
    out[2] = 0x00;                       // NSAC
    out[3] = 0x32;                       // TRAN_SPEED — 25 MHz
    out[4] = 0x5F;                       // CCC[11:4] — classes 0,2,4,5,6,7
    out[5] = 0x59;                       // CCC[3:0] | READ_BL_LEN = 9
    out[6] = (uint8_t)(0x80 |            // READ_BL_PARTIAL
                       ((cSize >> 10) & 0x03));
    out[7] = (uint8_t)((cSize >> 2) & 0xFF);
    // Byte 8: C_SIZE[1:0] | VDD_R_CURR_MIN(3) | VDD_R_CURR_MAX(3).
    out[8] = (uint8_t)(((cSize & 0x03) << 6) | (1 << 3) | 5);
    // Byte 9: VDD_W_CURR_MIN(3) | VDD_W_CURR_MAX(3) | C_SIZE_MULT[2:1].
    // The current fields have to leave the bottom two bits clear or they
    // corrupt the multiplier and the host reads back the wrong capacity.
    out[9] = (uint8_t)((1 << 5) | (5 << 2) | ((mult >> 1) & 0x03));
    // Byte 10: C_SIZE_MULT[0] | ERASE_GRP_SIZE(5) | ERASE_GRP_MULT[4:3].
    out[10] = (uint8_t)(((mult & 1) << 7) | (31 << 2));
    // Byte 11: ERASE_GRP_MULT[2:0] | WP_GRP_SIZE(5).
    out[11] = 0x1F;
    // Byte 12: WP_GRP_ENABLE | DEFAULT_ECC(2) | R2W_FACTOR(3) |
    //          WRITE_BL_LEN[3:2].  R2W_FACTOR 2 = write takes 4x a read.
    out[12] = (uint8_t)((2 << 2) | 0x02);
    out[13] = 0x40;                      // WRITE_BL_LEN[1:0] = 01 -> 9
    out[14] = 0x00;                      // FILE_FORMAT etc.
    out[15] = (uint8_t)((crc7(out, 15) << 1) | 1);
}

// ── Card presence ────────────────────────────────────────────────────

void NetpadMmcCard::attach(const uint8_t *bytes, size_t len) {
    if (!bytes || len < kBlockSize) return;
    image_.assign(bytes, bytes + len);
    inserted_ = true;
    // A freshly inserted card is in idle state until the host's CMD0 /
    // CMD1 handshake brings it up, exactly as after a power cycle.
    phase_ = Phase::Idle;
    cmdLen_ = 0;
    out_.clear();
    outPos_ = 0;
    idleState_ = true;
    blockLen_ = kBlockSize;
    readMultiple_ = writeMultiple_ = false;
    stopPending_ = false;
    writePos_ = 0;
}

bool NetpadMmcCard::updateImageInPlace(const uint8_t *bytes, size_t len) {
    if (!inserted_ || !bytes || len != image_.size()) return false;
    std::memcpy(image_.data(), bytes, len);
    return true;
}

void NetpadMmcCard::detach() {
    inserted_ = false;
    selected_ = false;
    phase_ = Phase::Idle;
    cmdLen_ = 0;
    out_.clear();
    outPos_ = 0;
    readMultiple_ = writeMultiple_ = false;
    stopPending_ = false;
    writePos_ = 0;
}

void NetpadMmcCard::setChipSelect(bool selected) {
    if (selected == selected_) return;
    selected_ = selected;
    if (!selected) {
        // Deselecting abandons whatever was in flight.  Multiple-block
        // transfers are the one thing that survives on real hardware, but
        // neither ROM driver deselects mid-transfer, so dropping them
        // keeps the model honest about what it has been shown to do.
        phase_ = Phase::Idle;
        cmdLen_ = 0;
        out_.clear();
        outPos_ = 0;
        readMultiple_ = writeMultiple_ = false;
        stopPending_ = false;
        writePos_ = 0;
    }
}

// ── The SPI byte engine ──────────────────────────────────────────────

void NetpadMmcCard::beginResponse(std::vector<uint8_t> &&bytes) {
    out_ = std::move(bytes);
    outPos_ = 0;
    phase_ = Phase::Response;
}

void NetpadMmcCard::queueRegisterBlock(const uint8_t reg[16]) {
    std::vector<uint8_t> r;
    r.reserve(kResponseLatency + 1 + 1 + 16 + 2);
    for (int i = 0; i < kResponseLatency; i++) r.push_back(kIdle);
    r.push_back(kR1Ok);
    r.push_back(kTokenStartBlock);
    r.insert(r.end(), reg, reg + 16);
    const uint16_t crc = crc16(reg, 16);
    r.push_back((uint8_t)(crc >> 8));
    r.push_back((uint8_t)crc);
    beginResponse(std::move(r));
}

void NetpadMmcCard::queueDataBlock(uint32_t lba, bool withStatus) {
    std::vector<uint8_t> r;
    r.reserve(4 + blockLen_ + 2);
    for (int i = 0; i < kResponseLatency; i++) r.push_back(kIdle);
    if (withStatus) r.push_back(kR1Ok);
    r.push_back(kTokenStartBlock);
    const size_t off = (size_t)lba * blockLen_;
    const uint8_t *src = nullptr;
    std::vector<uint8_t> zeros;
    if (off + blockLen_ <= image_.size()) {
        src = image_.data() + off;
    } else {
        // Past the end of the image: read as zeroes rather than failing,
        // so a mis-sized partition table can't wedge the driver.
        zeros.assign(blockLen_, 0);
        src = zeros.data();
    }
    r.insert(r.end(), src, src + blockLen_);
    const uint16_t crc = crc16(src, blockLen_);
    r.push_back((uint8_t)(crc >> 8));
    r.push_back((uint8_t)crc);
    beginResponse(std::move(r));
}

// Collect command bytes out of the MOSI stream while a block is being
// shifted out, and note a CMD12 (STOP_TRANSMISSION).  Uses the same shift
// buffer as the idle-state command hunt, which is free during a response.
bool NetpadMmcCard::sniffStopCommand(uint8_t mosi) {
    if (cmdLen_ == 0) {
        if ((mosi & 0xC0) != 0x40) return false;
        cmd_[cmdLen_++] = mosi;
        return false;
    }
    cmd_[cmdLen_++] = mosi;
    if (cmdLen_ < 6) return false;
    cmdLen_ = 0;
    if ((cmd_[0] & 0x3F) != kCmdStopTransmission) return false;
    readMultiple_ = false;
    stopPending_ = true;
    return true;
}

void NetpadMmcCard::execute() {
    const uint8_t index = (uint8_t)(cmd_[0] & 0x3F);
    const uint32_t arg = ((uint32_t)cmd_[1] << 24) | ((uint32_t)cmd_[2] << 16) |
                         ((uint32_t)cmd_[3] << 8) | cmd_[4];
    auto respondR1 = [&](uint8_t r1) {
        std::vector<uint8_t> r;
        for (int i = 0; i < kResponseLatency; i++) r.push_back(kIdle);
        r.push_back(r1);
        beginResponse(std::move(r));
    };
    const uint8_t status = idleState_ ? kR1IdleState : kR1Ok;

    switch (index) {
    case kCmdGoIdleState:
        idleState_ = true;
        readMultiple_ = writeMultiple_ = false;
        respondR1(kR1IdleState);
        return;
    case kCmdSendOpCond:
        // Initialisation completes immediately: the variant's state
        // machine polls CMD1 until R1 reads 0, and there is nothing to
        // wait for under emulation.
        idleState_ = false;
        respondR1(kR1Ok);
        return;
    case kCmdSendStatus: {                       // R2 — two status bytes
        std::vector<uint8_t> r;
        for (int i = 0; i < kResponseLatency; i++) r.push_back(kIdle);
        r.push_back(status);
        r.push_back(0x00);
        beginResponse(std::move(r));
        return;
    }
    case kCmdSendCsd: {
        uint8_t csd[16];
        buildCsd(csd);
        queueRegisterBlock(csd);
        return;
    }
    case kCmdAllSendCid:
    case kCmdSendCid: {
        uint8_t cid[16];
        buildCid(cid);
        queueRegisterBlock(cid);
        return;
    }
    case kCmdSetBlockLen:
        // Only whole 512-byte blocks are ever asked for, and a partial
        // length would break the image mapping, so anything else is
        // rejected the way a card with READ_BL_PARTIAL off would.
        if (arg == kBlockSize) { blockLen_ = kBlockSize; respondR1(status); }
        else                    respondR1((uint8_t)(status | 0x40));  // param error
        return;
    case kCmdReadSingleBlock:
        readMultiple_ = false;
        queueDataBlock(addressToBlock(arg), true);
        return;
    case kCmdReadMultipleBlock:
        readMultiple_ = true;
        nextBlock_ = addressToBlock(arg);
        queueDataBlock(nextBlock_++, true);
        return;
    case kCmdStopTransmission:
        readMultiple_ = false;
        stopPending_ = false;
        respondR1(status);
        return;
    case kCmdWriteBlock:
    case kCmdWriteMultipleBlock: {
        writeMultiple_ = (index == kCmdWriteMultipleBlock);
        writeBlock_ = addressToBlock(arg);
        nextBlock_ = writeBlock_;
        std::vector<uint8_t> r;
        for (int i = 0; i < kResponseLatency; i++) r.push_back(kIdle);
        r.push_back(status);
        out_ = std::move(r);
        outPos_ = 0;
        // The host clocks idle bytes until it has the R1, then sends the
        // data token; WriteWait covers both, since Response bytes are
        // drained first (see transfer()).
        phase_ = Phase::WriteWait;
        return;
    }
    // Erase tagging.  EPOC's format path tags a range and issues CMD38
    // before writing the new boot sector and FATs, all of which it then
    // overwrites explicitly — so acknowledging without touching the
    // image is both harmless and safer than guessing an erase pattern.
    case kCmdTagSectorStart:
    case kCmdTagSectorEnd:
    case kCmdUntagSector:
    case kCmdTagEraseGroupStart:
    case kCmdTagEraseGroupEnd:
    case kCmdUntagEraseGroup:
    case kCmdErase:
    case kCmdProgramCsd:
    case kCmdCrcOnOff:
        respondR1(status);
        return;
    case kCmdReadOcr: {                          // R3 — R1 + 32-bit OCR
        std::vector<uint8_t> r;
        for (int i = 0; i < kResponseLatency; i++) r.push_back(kIdle);
        r.push_back(status);
        r.push_back(0x80);                       // powered up
        r.push_back(0xFF);                       // 2.7-3.6 V window
        r.push_back(0x80);
        r.push_back(0x00);
        beginResponse(std::move(r));
        return;
    }
    default:
        respondR1((uint8_t)(status | kR1IllegalCmd));
        return;
    }
}

uint8_t NetpadMmcCard::transfer(uint8_t mosi) {
    // Nothing on the bus: the host's pull-up wins.  The 80 clocks the
    // spec requires before the first command are sent with chip select
    // high and land here.
    if (!inserted_ || !selected_) return kIdle;

    switch (phase_) {
    case Phase::WriteData: {
        writeBuf_[writePos_++] = mosi;
        if (writePos_ < blockLen_ + 2) return kIdle;   // block + CRC16
        const size_t off = (size_t)writeBlock_ * blockLen_;
        if (off + blockLen_ <= image_.size())
            std::memcpy(image_.data() + off, writeBuf_, blockLen_);
        writePos_ = 0;
        std::vector<uint8_t> r;
        for (int i = 0; i < kResponseLatency; i++) r.push_back(kIdle);
        r.push_back(kDataResponseOk);
        r.push_back(0x00);                             // one byte of "busy"
        out_ = std::move(r);
        outPos_ = 0;
        // A multiple-block write keeps taking blocks until the stop
        // token; a single-block write is done once the card is idle.
        phase_ = writeMultiple_ ? Phase::WriteWait : Phase::WriteBusy;
        if (writeMultiple_) writeBlock_ = ++nextBlock_;
        return kIdle;
    }
    case Phase::WriteWait:
        // Drain any queued response bytes first, then watch for the data
        // token.  Anything else (0xFF fill) just clocks through.
        if (outPos_ < out_.size()) return out_[outPos_++];
        if (mosi == kTokenStartBlock || mosi == kTokenStartMulti) {
            writePos_ = 0;
            phase_ = Phase::WriteData;
            return kIdle;
        }
        if (mosi == kTokenStopMulti) {
            writeMultiple_ = false;
            phase_ = Phase::Idle;
            cmdLen_ = 0;
            return kIdle;
        }
        return kIdle;
    case Phase::WriteBusy:
        if (outPos_ < out_.size()) return out_[outPos_++];
        phase_ = Phase::Idle;
        cmdLen_ = 0;
        return kIdle;
    case Phase::Response:
        // A multiple-block read only ends when the host sends CMD12, and
        // it sends it while the card is still shifting a block out — so
        // the command has to be picked out of the MOSI stream rather than
        // waited for.
        if (readMultiple_) sniffStopCommand(mosi);
        if (outPos_ < out_.size()) {
            const uint8_t b = out_[outPos_++];
            if (outPos_ == out_.size()) {
                out_.clear();
                outPos_ = 0;
                if (readMultiple_) {
                    queueDataBlock(nextBlock_++, false);
                } else if (stopPending_) {
                    // The block that was in flight when CMD12 arrived has
                    // finished; now answer the command itself.
                    stopPending_ = false;
                    std::vector<uint8_t> r;
                    for (int i = 0; i < kResponseLatency; i++) r.push_back(kIdle);
                    r.push_back(idleState_ ? kR1IdleState : kR1Ok);
                    out_ = std::move(r);
                    outPos_ = 0;
                } else {
                    phase_ = Phase::Idle;
                    cmdLen_ = 0;
                }
            }
            return b;
        }
        phase_ = Phase::Idle;
        cmdLen_ = 0;
        break;                                   // fall through to the hunt
    case Phase::Idle:
    case Phase::Command:
        break;
    }

    // Command hunt / collection.  A command byte has bits 7:6 = 01;
    // anything else while idle is the host's clock fill.
    if (cmdLen_ == 0) {
        if ((mosi & 0xC0) != 0x40) return kIdle;
        cmd_[cmdLen_++] = mosi;
        phase_ = Phase::Command;
        return kIdle;
    }
    cmd_[cmdLen_++] = mosi;
    if (cmdLen_ < 6) return kIdle;
    cmdLen_ = 0;
    execute();
    return kIdle;
}
