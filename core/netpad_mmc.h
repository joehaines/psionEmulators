// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// MultiMediaCard in SPI mode — the removable-media card in the Psion
// netpad's slot.
//
// The netpad hangs its card off a small SPI port in the board FPGA (nCS4
// registers 0x100-0x108, modelled in core/sa1100.cpp) rather than a
// parallel MMC host controller: `medmmc.pdd` names its transport
// `Media.MmcSpi`, and the ROM's two drivers of that port — the variant
// (ECust.dll) card-initialisation state machine and the media PDD itself
// — speak the plain MMC SPI protocol byte for byte.  So this class is a
// bit-faithful *card*: it takes one byte of MOSI at a time and returns
// the byte the card would have driven onto MISO in the same frame.
//
// Protocol, as the two ROM drivers exercise it:
//
//   * The host holds chip select low (FPGA control bit 5) around a
//     transaction; with it high the card ignores the clock and the bus
//     floats to 0xFF.  The 80 idle clocks the spec demands at power-up
//     are sent with CS high, so they land on the floating path.
//   * A command is six bytes: 0x40|index, a big-endian 32-bit argument,
//     and a CRC7 byte with bit 0 set.  Only CMD0's CRC is checked by a
//     real card in SPI mode (CRC checking is off until CMD59 turns it
//     on), and neither ROM driver ever enables it, so we accept any.
//   * The card answers after 1-8 idle bytes with an R1 status byte
//     (bit 7 clear), optionally followed by more response bytes (R2 for
//     CMD13, a 32-bit OCR for CMD58) or a data packet: the 0xFE start
//     token, the block, and a 16-bit CRC.
//   * Writes reverse that — the host sends 0xFE (or 0xFC for each block
//     of a multiple-block write, 0xFD to stop), the block and a CRC, and
//     the card answers with a data-response byte and then holds MISO low
//     for as long as it is programming.  Emulated programming is
//     instantaneous, so we report busy for a single byte to keep the
//     driver's "wait while zero" loop honest rather than skipping it.
//
// The image is a raw disk image (the frontend formats it FAT16, the same
// way it does the CompactFlash images), held in memory and handed back
// to the frontend to persist.
class NetpadMmcCard {
public:
    static constexpr uint32_t kBlockSize = 512;

    // True once a non-empty image has been attached.
    bool inserted() const { return inserted_; }
    size_t imageSize() const { return image_.size(); }
    const uint8_t *imageData() const {
        return image_.empty() ? nullptr : image_.data();
    }

    // Replace the image and raise card-present.  Takes a copy.
    void attach(const uint8_t *bytes, size_t len);
    // Replace the bytes of an already-inserted card without disturbing
    // the SPI state machine — the socket stays powered and the mount
    // alive.  Returns false if no card is in, or the size differs.
    bool updateImageInPlace(const uint8_t *bytes, size_t len);
    // Clear card-present.  Keeps the image so re-attaching is cheap.
    void detach();

    // Chip select, driven by the FPGA control register.  A deselect
    // ends whatever transaction was in flight, exactly as it does on
    // real hardware (the card returns to the idle command-hunt state).
    void setChipSelect(bool selected);
    bool chipSelected() const { return selected_; }

    // Clock one byte through the card.  Returns what the card drove on
    // MISO during the same eight clocks.
    uint8_t transfer(uint8_t mosi);

    // Card registers, built from the image geometry (see the .cpp).
    void buildCid(uint8_t out[16]) const;
    void buildCsd(uint8_t out[16]) const;

    // MMC CRCs.  Exposed so the FPGA-port unit test can check the card's
    // data packets the way a real host would.
    static uint8_t  crc7(const uint8_t *data, size_t len);
    static uint16_t crc16(const uint8_t *data, size_t len);

private:
    // What the card is doing between command bytes.
    enum class Phase : uint8_t {
        Idle,          // hunting for a command byte
        Command,       // collecting the remaining 5 command bytes
        Response,      // shifting queued response / data bytes out
        WriteWait,     // waiting for the host's data-start token
        WriteData,     // collecting block bytes + CRC
        WriteBusy,     // programming (one byte of MISO low)
    };

    void   beginResponse(std::vector<uint8_t> &&bytes);
    void   execute();                        // run the collected command
    // token + 512 bytes + CRC.  The first block of a transfer is preceded
    // by the command's R1; the continuation blocks of a multiple-block
    // read are not.
    void   queueDataBlock(uint32_t lba, bool withStatus);
    void   queueRegisterBlock(const uint8_t reg[16]);
    bool   sniffStopCommand(uint8_t mosi);   // CMD12 during a block stream
    uint32_t blockCount() const {
        return (uint32_t)(image_.size() / kBlockSize);
    }
    // Byte address (standard-capacity MMC) -> block index.
    uint32_t addressToBlock(uint32_t arg) const { return arg / blockLen_; }

    std::vector<uint8_t> image_;
    bool inserted_ = false;
    bool selected_ = false;

    Phase   phase_ = Phase::Idle;
    uint8_t cmd_[6] = {};
    uint8_t cmdLen_ = 0;
    bool    idleState_ = true;      // set by CMD0, cleared once CMD1 completes
    uint32_t blockLen_ = kBlockSize;

    std::vector<uint8_t> out_;      // queued MISO bytes
    size_t   outPos_ = 0;

    // Multiple-block transfers keep running until CMD12 / the stop token.
    bool     readMultiple_ = false;
    bool     writeMultiple_ = false;
    bool     stopPending_ = false;   // CMD12 seen mid-stream; R1 still owed
    uint32_t nextBlock_ = 0;

    // Block being written: 512 payload bytes then two CRC bytes.
    uint8_t  writeBuf_[kBlockSize + 2] = {};
    uint32_t writePos_ = 0;
    uint32_t writeBlock_ = 0;
};
