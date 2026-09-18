// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "clps7110.h"
#include <cstdlib>

namespace Geofox {

// Geofox One (1997) — the EPOC32 clamshell built by Geofox Ltd, a Psion
// licensee, around the same ARM710 + CL-PS711x platform as the Psion
// Series 5 and the same EPOC Release 1 kernel generation.
//
// What the ROM image establishes about the hardware (roms/Geofox_v1.01
// (146)_eng.bin, TRomHeader version 1.01(146), 8 MB):
//
//   * EPOC R1, not R5. iKernelDataAddress = 0x80100000 — the SuperPage
//     layout the Series 5's v1.01 ROM uses, one major release older than
//     the Osaris / 5mx ROMs' 0x80000000. Its Z: drive carries the R1
//     component set (EKern.exe, EUser.dll, EFile.exe, Ws32.dll, Cone.dll,
//     …) and the variant DLL is VArmPG.dll — "PG" for the Geofox rather
//     than the Series 5's own.
//   * 640 x 320 LCD, 2 bpp greyscale. Read straight out of the first
//     LCDCON the kernel programs, 0x7414EC7F: GSEN=1 / GSMD=0 selects
//     2 bpp, LINELEN (bits 18:13) = 39 gives (39+1)x16 = 640 pixels per
//     line, and VBUFSIZ (bits 12:0) = 0x0C7F sizes the buffer at
//     (0xC7F+1) x 16 = 51,200 bytes — exactly 640 x 320 x 2 bits. Same
//     width as the contemporary Series 5, a third taller, and that is
//     the reason the machine needs its own profile rather than riding
//     the Series 5's 640 x 240: rendered at 240 rows, the bottom
//     quarter of every screen is simply not drawn.
//   * No FRBADDR. The ROM never writes the CL-PS7111's framebuffer-base
//     register, so the frame buffer sits at the CL-PS7110's fixed
//     0xC0000000 — which is why this class derives from CLPS7110::Emulator
//     (the smaller register block, SYSFLG1 chip-ID bit clear, 24-bit
//     SYSCON1) rather than from CLPS7111::Emulator.
//
// Deliberately NOT derived from Series5::Emulator. The two machines share
// a kernel generation but not a binary: the Series 5 subclass carries a
// long tail of workarounds pinned to PCs in the Series 5 v1.01(144) image
// (the HAL-vtable fix, the ADC mid-scale default, the LCD-init hook), and
// those addresses land in unrelated Geofox code — the ROMs diverge from
// offset 0x82 onwards.
//
// STATUS: boots to the EPOC desktop. The kernel comes up, the file
// server mounts its drives, the loader brings in the whole graphics stack
// (Gdi, BitGdi, ScDv, Video.ldd, Lcd.pdd, FbServ, EwSrv, Ws32), the window
// server reads its Z:\System\Data\WsIni.ini and the shell paints the
// 640 x 320 panel: the icon column (Word, Sheet, Data, Agenda), the epoc
// hand, the System pane and the analogue clock. No CPU exceptions are
// taken over a 40 s boot.
//
// The capacitive mouse pad in the keyboard deck is emulated — see the
// mouse-pad section below for the packet the ROM's Exyin.dll reads off
// the SSI bus and for how a host's absolute pointer is turned back into
// the relative motion the pad reports. NOT emulated, and not faked
// either: the Type II PC Card slot (see the profile comment in
// core/device_registry.cpp). Its absence does not stop the machine being
// used — the keyboard and the twelve etched deck keys reach everything.
//
// Four gaps were found and closed on the way here. The one that turned a
// blank panel into a desktop is not in this file at all: ARM exception
// entry must mask IRQs, and our core was only doing that for IRQ/FIQ
// entries. See setAllExceptionIBit() in the constructor below, and
// raiseException() in core/arm710.cpp, for why the Geofox in particular
// cannot survive without it.
//
// The other three are hardware gaps, each worth more boot than the last;
// they are the three blocks below, and they are why the machine got far
// enough for the IRQ-masking bug to be the thing standing in the way.
// Each was measured, not guessed: the progress metric was how far the
// loader's ROM-directory walk got and how many distinct PCs the sampler
// saw, with PSION_RTC_SEED pinned so runs were comparable. A fourth,
// a cold-boot DRAM fill, turned out to be a symptom of the IRQ bug
// rather than hardware and has been removed — see the note where it was.
class Emulator : public CLPS7110::Emulator {
public:
    Emulator() {
        // Mask IRQs on entry to every exception, the way real ARM
        // hardware does. The Geofox's EKern gives SVC and IRQ mode the
        // same 1 KB stack — its mode-stack setup at 0x50019BC8 loads
        // SP_svc and SP_irq from the same pointer (SuperPage+0x388 ->
        // 0x801057EC, minus the one word the executive-call trampoline
        // at 0x5001A090 reserves) — so the two only coexist because a
        // SWI is entered with IRQs off. Without this, the first timer
        // IRQ to land inside a fast-path executive call pushes
        // r0-r3/ip/lr over the live SVC frame; the call then "returns"
        // into a stale handler address, and the wreckage surfaces a
        // few million cycles later as a USER 19 (bad descriptor type)
        // panic, a prefetch abort into EPOC's 0xBB stack poison, and
        // finally a hang inside the 64-bit divide at 0x5004D65C, which
        // is entered mid-loop with a garbage iteration count. The
        // screen stays blank because the shell never gets to run.
        cpu.setAllExceptionIBit(true);
    }

    const char *getDeviceName() const override { return "Geofox One"; }
    // No getROMSize() override: the image is 8 MB, which is exactly the
    // base class's ROM array. (The Series 5 overrides it because its
    // image is only 6 MB of that array.)

    // 640 x 320, 2 bpp greyscale — see the LCDCON decode above.
    int getLCDWidth()  const override { return 640; }
    int getLCDHeight() const override { return 320; }

    // The Geofox has no touchscreen. It points with a capacitive pad in
    // the keyboard deck that reports RELATIVE motion, the way a mouse
    // does — the ROM's own help text calls it "the mouse pad" and says
    // it "detects the proximity of your finger", with a tap for a click
    // and a tap in the pad's top-right corner for the menu bar. The
    // mouse-pad section further down is what models it.
    //
    // The dimensions below are therefore the LCD's, and they are not a pen
    // area: the pad has no coordinate space of its own to report, so the
    // frontend uses these as the space the on-screen pointer lives in and
    // updateTouchInput() below turns a position in it back into pad
    // motion.
    int getDigitiserWidth()  const override { return 640; }
    int getDigitiserHeight() const override { return 320; }
    int getLCDOffsetX()      const override { return 0; }
    int getLCDOffsetY()      const override { return 0; }

    // ── Capacitive mouse pad ───────────────────────────────────────────
    //
    // WHERE IT LIVES. Z:\System\Libs\Exyin.dll — ROM 0x5007D9A0, 0x694
    // bytes, "xy input" — is the whole driver, and every hardware access
    // it makes goes through four variant entry points:
    //
    //   0x50018958  SYSCON1[17:16] (ADCKSEL) = 01   — SSI clock, once at init
    //   0x500191A0  read INTSR1 & 0x40 (EINT2)      — "is a packet ready?"
    //   0x50018C18  write SYNCIO = 0x6000|a<<8|b    — start an SSI frame
    //   0x50018C28  read SYNCIO, low 16 bits        — collect the reply
    //
    // So the pad is on the SSI bus after all, sharing it with the
    // configuration PROM below. What used to be recorded here — that a
    // whole boot makes exactly 32 SSI transfers, all of them the PROM
    // scan — is precisely what a pad whose EINT2 never rises produces:
    // the driver was loaded and polling the whole time and never had a
    // reason to start a transfer.
    //
    // THE SEQUENCE, from the driver's four callbacks (0x5007DB98,
    // 0x5007DBE8, 0x5007DC1C, 0x5007DC5C, installed as the timer/DFC
    // entries at [state+0x04] / +0x18 / +0x2C / +0x68):
    //
    //   poll   every 25 ms: if INTSR1 & 0x40, write SYNCIO 0x7080,
    //          else re-arm the poll and do nothing
    //   +3 ms  read SYNCIO -> high byte = status, low byte = X delta;
    //          write SYNCIO 0x7080 again
    //   +3 ms  read SYNCIO -> high byte = Y delta (low byte unread);
    //          re-arm the poll and hand the packet to the DFC
    //
    // Two frames of the same control byte, one packet. 0x7080 is the
    // variant's SSI word for (device 0x10, byte 0x80) the same way
    // 0x7AC0..0x7ACF is (device 0x1A, byte 0xC0|i) for the PROM, so the
    // control byte that reaches syncioResponse() is 0x80.
    //
    // THE PACKET is a PS/2 mouse packet in all but delivery — status,
    // X, Y with the sign bits living in the status byte, read out of the
    // driver's decode at 0x5007DC70:
    //
    //   bit 0  0x01  button: EButton1Down / EButton1Up at the pointer
    //   bit 1  0x02  menu: EKeyDown 0x94, the pad's top-right-corner tap
    //   bit 4  0x10  X delta is negative (the byte is read as byte - 256)
    //   bit 5  0x20  Y delta is negative, mouse-style: the driver
    //                SUBTRACTS it, so positive Y is up the screen
    //
    // The driver adds the deltas to the pointer it keeps at [state+0x40]
    // and [state+0x44], clamps to 0..639 / 0..319 (the literals at
    // 0x5007DE68 and 0x5007DE6C), and posts EPointerMove — or, when a
    // button bit changed, the button event instead, which carries the
    // same freshly-updated position.
    //
    // Bit 1 is decoded here for the record but nothing sets it: EKeyDown
    // 0x94 is exactly what the Menu deck key sends through the matrix
    // above, so the frontend reaches the menu bar by pressing that key
    // rather than by needing a second button on the host pointer API.
    //
    // ABSOLUTE IN, RELATIVE OUT. Every other machine in this tree has a
    // digitiser, so the host API is "the user is pointing HERE" in LCD
    // coordinates. The bridge to a relative pad is a closed loop: we keep
    // a shadow of the pointer the driver is holding and hand out the
    // deltas that walk it to where the host is pointing. The shadow is
    // exact rather than approximate because
    //
    //   * the pad is the only thing that moves that pointer, and it only
    //     moves on a packet we built;
    //   * the shadow starts at (320, 160), which is what the driver's own
    //     constructor stores at 0x5007DB38, and the driver cannot have
    //     moved it before reading its first packet from us;
    //   * we replicate the clamp, and we never hand over a delta the
    //     driver would accelerate (see kPadGentleStep).
    void updateTouchInput(int32_t x, int32_t y, bool down) override {
        padPush(clampInt(x, 0, getLCDWidth() - 1),
                clampInt(y, 0, getLCDHeight() - 1), down);
    }

    // Expose RAM to the harness so --save-ram-snapshot works.
    uint8_t *getRamBuffer() override       { return MemoryBlockC0; }
    size_t   getRamSize()   const override { return sizeof(MemoryBlockC0); }

    // ── Keyboard matrix ────────────────────────────────────────────────
    //
    // The Geofox's matrix is 8 columns of 12 keys, and the ROM carries
    // its own scancode table for it at 0x5007CD0C: 96 bytes, indexed by
    // column * 12 + bit, each holding the EPOC scancode that position
    // sends. kMatrix below IS that table, read straight out of the image
    // — which is why the letter keys land where a Geofox photo says they
    // should, and why the fifteen machine-specific codes in columns 5-7
    // (0xA2, 0xA3, 0xA8, 0xAA-0xAF, 0xB0-0xB5) are the ones its etched
    // application keys send rather than a guess.
    //
    // A set bit means the key is DOWN. The driver's change detector at
    // 0x5007C8C4 walks (old & ~new) to decrement the held-key counters at
    // [r6+0x44] / [r6+0x48] and (new & ~old) to increment them, so the
    // 0 -> 1 edge is the press.
    //
    // How the matrix reaches the kernel, read out of the ROM's own scan
    // routine at 0x5007C51C: for each of eight columns it writes
    // SYSCON1[3:0] = 8 + column (the CL-PS711x KBDSCAN "drive column n"
    // encoding, via the variant helper at 0x500188F8 case 3), then reads
    // twelve row bits back — the low eight from port A (0x50018CC0) and
    // the top four from port B's LOW nibble (0x50018D08, masked with 0xF
    // by its caller). Between scans it leaves KBDSCAN at 0, driving every
    // column at once, and polls that for "is anything down at all"
    // (0x5007C7AC) before bothering with a full scan.
    //
    // So the Geofox is an ordinary CL-PS711x keyboard after all, just a
    // wider one than the base class assumes: eight columns rather than
    // seven, twelve rows rather than seven plus four inverted modifier
    // bits in port B's high nibble. Both overrides below exist for that
    // difference alone.
    uint8_t composePortA() const override {
        return (uint8_t)(scannedRows() & 0xFF);
    }
    uint8_t composePortB() const override {
        return (uint8_t)((scannedRows() >> 8) & 0x0F);
    }

    void setKeyboardKey(EpocKey key, bool value) override {
        for (int col = 0; col < kColumns; col++) {
            for (int bit = 0; bit < kKeysPerColumn; bit++) {
                if (kMatrix[col][bit] != (uint8_t)key) continue;
                if (value) kbdColumns[col] |=  (uint16_t)(1u << bit);
                else       kbdColumns[col] &= (uint16_t)~(1u << bit);
                return;   // first match wins; Space repeats across four
                          // positions of column 0 (one wide key bar)
            }
        }
    }

protected:
    // ── Configuration EEPROM on the SSI bus ────────────────────────────
    //
    // Once, early in boot, the variant driver reads a 32-byte block over
    // the SSI: the routine at ROM 0x5007EE8C loops sixteen times, writing
    // SYNCIO 0x7AC0..0x7ACF (that is 0x1A in the high byte, 0xC0 | i in
    // the low) and storing each 16-bit reply as a little-endian halfword
    // into a 32-byte buffer. Nothing re-reads it afterwards; it is the
    // machine's settings PROM, the Geofox's equivalent of the identity
    // PROM the Series 5 reads through ETNA.
    //
    // The block is validated at 0x5007EF38 — XOR every one of the 32
    // bytes together and the result must be 0x42, or the routine returns
    // -2 — and its fields are read by a row of accessors just after it:
    //
    //   [0]        byte                       (0x5007EF28)
    //   [0..3]     word, bits 17:8 signed     (0x5007EF6C)
    //   [4..5]     signed 16                  (0x5007EFF8)
    //   [6..7]     signed 16                  (0x5007F014)
    //   [8..9]     signed 16, clamped 0..15   (0x5007EF84)
    //   [10..11]   signed 16, clamped 0..15   (0x5007EFB0)
    //   [12..13]   signed 16                  (0x5007F064)
    //   [14..15]   signed 16                  (0x5007EFDC)
    //   [16..17]   signed 16                  (0x5007F080)
    //   [18..19]   signed 16                  (0x5007F09C)
    //   [20..23]   word                       (0x5007F030)
    //   [24..27]   word                       (0x5007F054)
    //
    // Nothing reads bytes 28..31, which is where the checksum byte goes.
    //
    // WHY THIS MATTERS: with the SSI unanswered every byte read back as
    // zero, the checksum came to 0x00, the driver returned its error, and
    // the boot stopped with EFile up but no GUI. An emulated machine has
    // no PROM to read, so it gets a blank-but-valid one: all fields zero,
    // which every accessor above either uses directly or clamps into
    // range, and 0x42 in an unread byte to make the checksum come out.
    // Verified: with the block answered, the driver takes its success
    // path at 0x5007E3E0 and goes on to read the fields, where before it
    // branched to the error path at 0x5007E454.
    static constexpr int kEepromBytes = 32;
    static constexpr uint8_t kEepromChecksum = 0x42;

    MaybeU32 syncioResponse(uint8_t controlByte) override {
        if (controlByte == kPadControlByte) return padFrame();
        if (controlByte < 0xC0 || controlByte > 0xCF) return {};
        int word = controlByte - 0xC0;         // frame i carries bytes 2i, 2i+1
        uint8_t lo = eepromByte(word * 2);
        uint8_t hi = eepromByte(word * 2 + 1);
        return (uint32_t)(lo | (hi << 8));
    }

    // The PROM image, built rather than stored: every byte is zero except
    // the one carrying the checksum, so the XOR over the block is 0x42.
    static uint8_t eepromByte(int index) {
        return index == kEepromBytes - 1 ? kEepromChecksum : 0x00;
    }

    // ── nCS2 power-status chip ─────────────────────────────────────────
    //
    // The Geofox's power driver reads a single status byte from physical
    // 0x30000000 (virtual 0x7FF00000) roughly once a second and decodes it
    // into a four-field structure — the code at ROM 0x5007EA20 does:
    //
    //     ldrb r2, [r0]                  ; the byte below
    //     tst  r2, #6      → field 0 = 1 when bits 1-2 are BOTH clear
    //     and  r3, r2, #0x18             ; main-battery level, bits 4:3
    //         0 → 7,  8 → 4,  0x10 → 3,  0x18 → 1
    //     and  r3, r2, #1  → field 2 = bit 0
    //     and  r3, r2, #0x60             ; backup-battery level, bits 6:5
    //         0x60 → 3, 0x40 → 2, 0x20 → 1, 0 → 0
    //
    // Nothing else in the image touches the window and the ROM never
    // writes it. Before it was mapped at all the read took a bus-error
    // data abort every second (a recurring PageOtherBusError at
    // pc=0x5007EA2C, FAR=0x7FF00000), which is not what the real driver
    // sees.
    //
    // The VALUE matters enormously, and 0x06 was arrived at by sweeping
    // all 256 of them. Bits 1 and 2 have to be SET: they are what makes
    // field 0 read 0, and the ROM's power strings ("Replace main
    // batteries", "Main batteries too low for PC Card") say what field 0
    // being 1 means. With them clear the boot stops dead at the media
    // drivers — 23 distinct PCs sampled in 25 s, nothing past Medcrm.pdd
    // in the loader's directory walk. With them set it runs on through the
    // window server. The rest of the byte says the main battery is at its
    // healthiest level (bits 4:3 = 0 → 7) and clears everything else, and
    // 0x06 measured at or above every other value in the sweep.
    //
    // That is the same thing the Series 5 profile does for its battery ADC
    // channels: hand the guest a healthy constant rather than model a part
    // that isn't there.
    static constexpr uint8_t kPowerStatusByte = 0x06;

    // PSION_GF_POWER_BYTE=<n> overrides the byte, for sweeping what the
    // power driver does with each bit pattern.
    static uint8_t powerStatusByte() {
        static int over = -2;
        if (over == -2) {
            const char *e = std::getenv("PSION_GF_POWER_BYTE");
            over = e ? (int)strtoul(e, nullptr, 0) : -1;
        }
        return over >= 0 ? (uint8_t)over : kPowerStatusByte;
    }

    MaybeU32 readRegion3(uint32_t physAddr, ValueSize valueSize) const override {
        // The chip decodes a single byte and mirrors it across the whole
        // window, so the address doesn't select anything and a wider read
        // sees the same value zero-extended. The driver only ever does an
        // LDRB, so nothing observes the upper bytes either way.
        (void)physAddr; (void)valueSize;
        return (uint32_t)powerStatusByte();
    }
    // The ROM never writes the window; accept and drop, so a stray write
    // doesn't abort the machine.
    bool writeRegion3(uint32_t value, uint32_t physAddr, ValueSize valueSize) override {
        (void)value; (void)physAddr; (void)valueSize;
        return true;
    }

    // Two 4 MB DRAM banks at 0xC0000000 and 0xD0000000, backed by the
    // lower and upper halves of MemoryBlockC0 — the CL-PS711x arrangement
    // the Series 5 also has, rather than one 8 MB bank mirrored across
    // both regions (which would collide the two).
    //
    // Only bank 0 is ever used: after three simulated minutes the ROM has
    // its page tables at physical 0xC0020000 and its kernel data around
    // 0xC03E0000, and region 0xD is untouched. Bank 1 is modelled anyway
    // because an unmapped region 0xD faults rather than reading back,
    // which is not what a second DRAM bank does. Presenting 8 MB as one
    // linear bank at 0xC0000000 instead was tried and is much worse — the
    // boot collapses inside the first second, 37 distinct PCs against 497
    // — so 4 MB per bank is what this ROM expects.
    uint32_t getRamMask() const override { return 0x3FFFFFu; }
    bool aliasDRegionToRam() const override { return true; }
    uint32_t getRegionDRamOffset() const override { return 0x400000u; }

    // No cold-boot DRAM fill here, deliberately. While the IRQ-masking
    // bug above was still in place, what uninitialised DRAM read back as
    // changed the boot a lot (zeros sampled 376 distinct PCs, alternating
    // patterns 922), and this class carried a 0xAA fill because of it.
    // With the bug fixed, 0x00, 0x55, 0xAA and 0xFF all reach the same
    // desktop with the same pixel variance and no exceptions, so the fill
    // was a symptom, not hardware, and it is gone. PSION_RAM_FILL is still
    // there for anyone who wants to sweep patterns again.

    // The base class's debug-log hooks are pinned to PCs in the Osaris
    // v1.02 ROM; on this image they land in unrelated functions and
    // invent "KERNEL MMU SECTION" lines that read as real findings.
    bool hasOsarisDebugHooks() const override { return false; }

private:
    // The twelve row bits the kernel sees for whatever KBDSCAN currently
    // selects: one column when bit 3 is set, every column OR'd together
    // when it is 0 (which is what the "anything down?" poll reads), and
    // nothing for the remaining codes, which drive no column.
    uint16_t scannedRows() const {
        if (kScan & 8) return kbdColumns[kScan & 7];
        if (kScan == 0) {
            uint16_t all = 0;
            for (int c = 0; c < kColumns; c++) all |= kbdColumns[c];
            return all;
        }
        return 0;
    }

    static constexpr int kColumns = 8;
    static constexpr int kKeysPerColumn = 12;
    // The ROM's own matrix-to-scancode table, copied from 0x5007CD0C.
    static constexpr uint8_t kMatrix[kColumns][kKeysPerColumn] = {
        { 0x04, 0x18, 0x16, 0x05, 0x05, 0x05, 0x05, 0x0e, 0x11, 0x0f, 0x7b, 0x10 },
        { 0x12, 0x5a, 0x58, 0x43, 0x56, 0x42, 0x4e, 0x4d, 0x79, 0x7a, 0x7d, 0x7e },
        { 0x02, 0x41, 0x53, 0x44, 0x46, 0x47, 0x48, 0x4a, 0x4b, 0x4c, 0x50, 0x80 },
        { 0x7f, 0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4f, 0x82, 0x83 },
        { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x87, 0x13 },
        { 0x97, 0xb0, 0xb4, 0x94, 0xab, 0xaa, 0xac, 0x7c, 0x8a, 0x8b, 0x86, 0x03 },
        { 0xa8, 0xaf, 0xb3, 0xb1, 0x83, 0x93, 0x92, 0x8c, 0x8d, 0x8e, 0x85, 0x81 },
        { 0xad, 0xae, 0xb2, 0xb5, 0xa2, 0xa3, 0x89, 0x8f, 0x90, 0x91, 0x84, 0x01 },
    };
    // One bit per key, set while the key is held.
    uint16_t kbdColumns[kColumns] = {};

    // ── Mouse-pad state ────────────────────────────────────────────────
    //
    // See the long note at updateTouchInput() for the packet and for why
    // the shadow pointer below can be trusted.
    static constexpr uint8_t kPadControlByte = 0x80;
    // The driver's own starting pointer, stored at 0x5007DB38 — which is
    // also the centre of the 640 x 320 panel, though the ROM writes the
    // two literals rather than deriving them.
    static constexpr int32_t kPadStartX = 320, kPadStartY = 160;
    // A delta travels as one signed byte plus a sign bit, so 127 is as far
    // as the pointer can move in a single packet.
    static constexpr int32_t kPadMaxStep = 127;
    // Acceleration, at 0x5007DD20: the driver multiplies the delta by a
    // growing factor once |delta| reaches a threshold on some axis AND the
    // PREVIOUS packet's delta reached it on that axis too. The threshold
    // comes from the Control Panel's mouse-speed setting, read from
    // 0x40000218. That setting is the Screen dialog's Slow / Medium /
    // Fast, and those three are all of it: the decode at 0x5007DCC0
    // handles 1 and 2 and treats everything else as "no acceleration",
    // so the thresholds are 12 (Medium), 8 (Fast) and none (Slow) and
    // there is no fourth value to be caught out by. A delta below 8 is
    // therefore never accelerated whatever the user has chosen, and
    // neither is a large one that follows a small one. Alternating a
    // full step with a gentle one therefore keeps the shadow exact, at an
    // average of 67 px per packet
    // (~2700 px/s at the 25 ms poll), which crosses the panel in a
    // quarter of a second: fast enough to feel direct, and never a guess.
    static constexpr int32_t kPadAccelFloor  = 8;
    static constexpr int32_t kPadGentleStep  = kPadAccelFloor - 1;

    // Where the host is pointing: a queue of "walk to (x, y), then hold
    // the button like this", not a single target. The queue exists
    // because the host is faster than the pad — a mouse reports at 120 Hz
    // and a touchscreen can put a whole tap between two 25 ms packets —
    // and it is what keeps the two facts a click carries, its position
    // and its edges, from being lost to that difference:
    //
    //   * a move that only supersedes an earlier move overwrites it, so
    //     an idle-but-twitching mouse never backs the queue up;
    //   * a goal that CHANGES the button is never overwritten, so a press
    //     stays where the user pressed even if the finger has moved on by
    //     the time the pad gets to it — which is what makes a drag select
    //     from where it started rather than from halfway along;
    //   * a tap whose press and release both land between two packets
    //     still produces both, instead of cancelling out.
    struct PadGoal { int32_t x, y; bool button; bool changesButton; };
    static constexpr int kPadGoalSlots = 16;
    PadGoal padGoals[kPadGoalSlots] = {};
    int padGoalHead = 0, padGoalCount = 0;
    // The button state once the queue has drained — what a newly pushed
    // goal is a change relative to.
    bool padQueuedButton = false;

    // The shadow of the driver's pointer, and the packet in flight.
    int32_t padX = kPadStartX, padY = kPadStartY;
    bool padButton = false;
    bool padBigX = false, padBigY = false;  // last packet's delta was accelerable
    bool padSecondHalf = false;             // next SSI frame carries Y
    uint8_t padStatus = 0, padDeltaX = 0, padDeltaY = 0;

    static int32_t clampInt(int32_t v, int32_t lo, int32_t hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    // EINT2 is the pad's "packet ready" line, and the only thing the
    // driver's 25 ms poll looks at. It is a status bit with no EOI: it
    // stands while we have something to say and drops when we don't.
    void padRefreshIrq() {
        if (padGoalCount > 0) pendingInterrupts |=  (1u << CLPS7111::EINT2);
        else                  pendingInterrupts &= ~(1u << CLPS7111::EINT2);
    }

    void padPush(int32_t x, int32_t y, bool button) {
        if (padGoalCount > 0) {
            PadGoal &last = padGoals[(padGoalHead + padGoalCount - 1) % kPadGoalSlots];
            if (button == padQueuedButton && !last.changesButton) {
                last.x = x; last.y = y; padRefreshIrq(); return;
            }
        } else if (x == padX && y == padY && button == padButton) {
            return;   // already there, nothing to report
        }
        if (padGoalCount == kPadGoalSlots) {
            // Only reachable while nothing is draining the queue — before
            // the driver has loaded, or with the lid shut. Drop the oldest
            // so a machine that comes back finds recent input, not a
            // quarter-second of replayed history.
            padGoalHead = (padGoalHead + 1) % kPadGoalSlots;
            padGoalCount--;
        }
        padGoals[(padGoalHead + padGoalCount) % kPadGoalSlots] =
            { x, y, button, button != padQueuedButton };
        padGoalCount++;
        padQueuedButton = button;
        padRefreshIrq();
    }

    // One step toward the goal, held below the acceleration threshold
    // whenever the previous packet was at or above it.
    static int32_t padStep(int32_t want, bool previousWasBig) {
        int32_t limit = previousWasBig ? kPadGentleStep : kPadMaxStep;
        return clampInt(want, -limit, limit);
    }

    // Build the packet the two SSI frames will carry, and move the shadow
    // exactly as the driver is about to move its pointer.
    void padBuildPacket() {
        int32_t stepX = 0, stepY = 0;
        if (padGoalCount > 0) {
            const PadGoal &goal = padGoals[padGoalHead];
            stepX = padStep(goal.x - padX, padBigX);
            stepY = padStep(goal.y - padY, padBigY);
            padX = clampInt(padX + stepX, 0, getLCDWidth()  - 1);
            padY = clampInt(padY + stepY, 0, getLCDHeight() - 1);
            // The button only changes once the pointer has arrived, so a
            // tap at a fresh position presses where the user aimed rather
            // than somewhere along the way.
            if (padX == goal.x && padY == goal.y) {
                padButton = goal.button;
                padGoalHead = (padGoalHead + 1) % kPadGoalSlots;
                padGoalCount--;
            }
        }
        // Y goes out inverted: the driver subtracts what it reads.
        int32_t reportX = stepX, reportY = -stepY;
        padStatus = (uint8_t)((padButton ? 0x01 : 0x00)
                              | (reportX < 0 ? 0x10 : 0x00)
                              | (reportY < 0 ? 0x20 : 0x00));
        padDeltaX = (uint8_t)(reportX & 0xFF);
        padDeltaY = (uint8_t)(reportY & 0xFF);
        padBigX = (stepX <= -kPadAccelFloor || stepX >= kPadAccelFloor);
        padBigY = (stepY <= -kPadAccelFloor || stepY >= kPadAccelFloor);
        if (std::getenv("PSION_GF_MOUSE_TRACE")) {
            log("mouse pad: step (%d,%d) -> pointer (%d,%d) status %02x  queue %d",
                stepX, stepY, padX, padY, padStatus, padGoalCount);
        }
    }

    // The two halves of one packet, both answered as the same control
    // byte. The phase cannot drift: the driver reads exactly twice per
    // transfer it starts, and it only starts one when EINT2 is up.
    uint32_t padFrame() {
        if (!padSecondHalf) {
            padBuildPacket();
            padSecondHalf = true;
            return (uint32_t)((padStatus << 8) | padDeltaX);
        }
        padSecondHalf = false;
        padRefreshIrq();   // still queued? the next 25 ms poll picks it up
        return (uint32_t)(padDeltaY << 8);
    }
};

}
