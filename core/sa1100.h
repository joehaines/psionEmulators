// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "emubase.h"
#include "sa1100_cpu.h"
#include "eiger.h"
#include "sa1100_defs.h"
#include "vcfcard.h"
#include "netpad_mmc.h"
#include "audio_codec.h"
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <set>
#include <vector>

// Intel StrongARM SA-1100 SoC emulator — CPU plus on-chip peripherals.
// Used by the Psion Series 7 and netBook which integrate a 16 MB boot
// ROM/flash at 0x00000000 and 32 MB SDRAM at 0xC0000000.
//
// This implementation focuses on getting the stock EPOC R5 boot ROM far
// enough through initial bring-up (reset vector → MMU enable → kernel
// launch) that the LCD controller is programmed and the framebuffer is
// rendered. On-chip peripherals are modelled at the MMIO register level
// with enough fidelity for the kernel's boot-time probes to return
// sensible values. Real hardware features we deliberately don't model:
//
//   * DMA transfers — the LCD pulls its framebuffer directly out of
//     DRAM based on DBAR1, no separate DMA engine involvement required.
//   * USB host / audio codec — the boot ROM doesn't touch these.
//   * PCMCIA socket emulation — the netBook has a CF bay, but the ROM
//     probes for it lazily and empty slot is a valid state.
//   * Thermal / power management side effects.
//
// The CPU is modeled by the existing ARM710 decoder with isTVersion=true
// (enables ARMv4 halfword loads/stores + long multiplies). SA-1100 is
// ARMv4 but the only ARMv4-over-ARMv3 instructions the EPOC R5 ROM uses
// are the ones already handled by the 'T' path.
namespace SA1100 {

class Emulator : public EmuBase {
    friend class ::SA1100Bridge;
public:
    // Composed ARM710 (T-variant enables the ARMv4 halfword / long
    // multiply instructions used by the EPOC R5 kernel on the netBook).
    SA1100Bridge cpu{this, true};
    ARM710 *getArmCpu() override { return &cpu; }
    const ARM710 *getArmCpu() const override { return &cpu; }

    // 16 MB boot ROM/flash (region 0). The Psion netBook's flash is
    // 16 MB; the Series 7 image on disk is the same size.
    static constexpr size_t kRomSize = 0x1000000;
    uint8_t ROM[kRomSize];

    // SDRAM banks. Real netBook / Series 7 has 32 MB total = two 16 MB
    // chips wired to nRAS0 (bank 0, regions 0xC0-0xC7) and nRAS1 (bank
    // 1, regions 0xC8-0xCF). The two banks are physically separate, so
    // a write to 0xC0000000 does NOT alias to 0xC8000000.
    //
    // The netBook EPOC R5 boot path relies on this independence: the
    // stage-1 bootloader copies the kernel image into bank 1 before
    // MMU enable, then the kernel maps va=0x88000000 → phys=0xC8000000
    // and executes from bank 1 while writing kernel data into bank 0
    // at 0xC0xxxxxx. If the two were aliased the kernel's first
    // data-segment write would scribble over its own .text and the
    // CPU would DABT-loop in the abort handler. Series 7 only uses
    // bank 0 in early boot, so it doesn't care about the split.
    static constexpr size_t kBankSize = 0x1000000;            // 16 MB
    static constexpr uint32_t kBankMask = kBankSize - 1;
    // netpad board GPIO input pins that idle high (pull-ups).  Reported
    // high in GPLR while the firmware has the pin configured as an input
    // (GPDR bit clear), and masked back out the moment it drives the pin
    // as an output — so an open-drain line reads high when released and
    // low when the firmware pulls it (see readGpio).
    //   bit 23  — the early bit-banged serial device's "ready" handshake
    //             (busy-waited high before the byte stream, FUN_50004608).
    //   bit 24  — the same device's "transmit complete" handshake
    //             (busy-waited high after the stream, FUN_500046b0).
    //   bits 15/16 — bit-banged open-drain serial/I2C clock / data.  The
    //             primitives (FUN_50005774 / _5794 direction-flip a line
    //             open-drain style; FUN_500057b4 / _57c8 read bits 16 /
    //             15; FUN_5000596c busy-waits bit 15 high) need a
    //             released line to float high or every transaction
    //             wedges the driver.
    // Every one of these is only ever busy-waited *high*.  Pins the
    // firmware waits *low* on are deliberately excluded: bit 0, and
    // bit 17 — a busy/interrupt flag FUN_50005800 spins on WHILE SET
    // (forcing it high makes that wait burn its full timeout on every
    // call).  Bit 14 is only branched on, never waited.
    static constexpr uint32_t kNetpadGpioReadyMask =
        (1u << 24) | (1u << 23) | (1u << 16) | (1u << 15);
    // netpad pen-detect line.  Exyin.dll (the digitiser driver) binds the
    // variant interrupt named "IrqGpioEdge14" and arms GRER bit 14 — and
    // only the rising edge — so the panel's pen signal reaches the SoC
    // inverted: the pin idles low and goes high while the stylus is down.
    static constexpr int kNetpadPenGpio = 14;
    static constexpr size_t kRamSize = 2 * kBankSize;
    static constexpr uint32_t kRamMask = kRamSize - 1;
    uint8_t RAM[kBankSize];        // bank 0 (regions 0xC0-0xC7 alias)
    uint8_t RAM2[kBankSize];       // bank 1 (regions 0xC8-0xCF alias)
    // Banks 2 and 3 (regions 0xD0-0xD7, 0xD8-0xDF) — populated only on
    // 4-bank machines (netpad, 64 MB). On 2-bank machines 0xD0-0xDF stays a
    // read alias of bank 0 (see readPhysical), so these go unused there.
    uint8_t RAM3[kBankSize];       // bank 2 (regions 0xD0-0xD7) — netpad
    uint8_t RAM4[kBankSize];       // bank 3 (regions 0xD8-0xDF) — netpad

    const uint8_t *romPtr()  const { return ROM; }
    const uint8_t *ramPtr()  const { return RAM; }
    const uint8_t *ram2Ptr() const { return RAM2; }
    // Banks 2/3 and the live bank count — needed by the CPU's host-pointer
    // fast path so it resolves regions 0xD0-0xDF to the same host memory as
    // readPhysical/writePhysical (bank 2/3 on 4-bank machines, a bank-0
    // alias on 2-bank ones).  Disagreeing there silently splits a physical
    // address across two buffers.
    const uint8_t *ram3Ptr() const { return RAM3; }
    const uint8_t *ram4Ptr() const { return RAM4; }
    int ramBanks() const { return ramBanks_; }

protected:
    // LCD dimensions reported to the frontend. The netBook has a native
    // 640×480 TFT panel; subclasses may override for other variants.
    virtual int lcdWidth() const  { return 640; }
    virtual int lcdHeight() const { return 480; }
    virtual const char *deviceName() const { return "Psion Series 7"; }

    // CP15 c0 Main ID register presented to the guest. The netBook /
    // Series 7 ship a DEC StrongARM SA-1100 (0x4401A11x); variants built
    // on the Intel SA-1110 (e.g. the netpad, whose boot ROM refuses to
    // run unless (id & 0xfffffff0) == 0x6901b110) override this.
    virtual uint32_t processorId() const { return 0x4401A118; }

    // Number of 16 MB SDRAM banks the machine populates. netBook / Series 7
    // have two (32 MB: bank 0 at 0xC0, bank 1 at 0xC8); the netpad has four
    // (64 MB: adds bank 2 at 0xD0, bank 3 at 0xD8). Two-bank machines keep
    // the historical 0xD0-0xDF = bank-0 read alias.
    virtual int ramBankCount() const { return 2; }

    // Byte offset within the ROM image at which the bootable EPOC ROM
    // starts. netBook / Series 7 boot from offset 0; the netpad image
    // carries a 0xC0000 boot-partition prefix (its own reset stub at
    // offset 0 that redirects to the OS ROM at 0xC0000, which is linked at
    // 0x50000000). Loading from that offset puts the OS reset vector at
    // physical 0 so VA 0x50000000 (ROMBASE) maps to real code.
    virtual size_t romLoadOffset() const { return 0; }

    // Size of the boot-flash address window. netBook / Series 7 decode a
    // 16 MB flash (0x1000000); the netpad has 32 MB. Only affects address
    // masking for region-0 and the 0x50 ROM alias — the physical ROM[]
    // backing store stays 16 MB (the 12.3 MB image fits), and reads above
    // the image return the erased-flash pattern.
    virtual size_t romWindowBytes() const { return kRomSize; }

    // Whether the boot flash is also visible in physical regions
    // 0x50-0x57 (the address EPOC links the ROM at, 0x50000000).  The
    // netpad's second-stage init re-runs the reset stub from VA
    // 0x500c0100 and disables the MMU mid-sequence, so execution must
    // keep fetching ROM at physical 0x50xxxxxx.  netBook / Series 7 reach
    // the ROM only through the MMU (VA 0x50000000 -> PA 0), so they leave
    // this false and physical 0x50-0x5F stays the Eiger companion-ASIC
    // window.  (0x58 is left as Eiger regardless — VA 0x58030000.)
    virtual bool flashAliasedAt0x50() const { return false; }

    // OSCR rate multiplier: how many 3.6864 MHz OS-timer ticks the guest
    // sees per real tick.  1 is the spec-correct rate; the Series 7 /
    // netBook default to 10 because their kernels don't reach WSERV at
    // 1× (see the kOsTimerScale comment below for that open bug).
    //
    // Anything but 1 skews every duration the guest measures off OSCR,
    // and on the netpad that was user-visible in three places: EPOC's
    // key auto-repeat fired mid-tap (one on-screen keyboard press
    // registered as 3-8), the board-codec driver's inter-transfer waits
    // expired before the SSP had clocked its frames (leaving stale words
    // in the RX FIFO — a garbage backup-battery reading and, when it
    // landed during app launch, a half-painted wedged UI), and the clock
    // ran fast.  The netpad kernel boots happily at 1×, so it takes the
    // faithful rate.
    virtual int64_t osTimerScale() const { return 10; }

    // Whether this is the Psion netpad (SA-1110 variant).  The netpad's
    // boot ROM drives several board-specific peripherals the netBook /
    // Series 7 lack — an early bit-banged serial device with a GPIO-23
    // "ready" handshake among them — so a handful of SoC paths need to
    // know they are running the netpad.  Default false; NetpadEmulator
    // overrides it.  Cached into isNetpad_ at reset() (see flashAlias50_).
    virtual bool isNetpad() const { return false; }

    // Current LCD framebuffer base address (populated when the guest
    // writes LCCR0.ENA; exposed so readLCDIntoBuffer can reuse it
    // whether or not the guest has configured the DMA channel yet).
    uint32_t currentLcdBase() const { return dbar1; }
    uint32_t currentLcdControl() const { return lccr0; }
    uint32_t currentLcdControl3() const { return lccr3; }

public:
    Emulator();
    ~Emulator() override = default;

    // EmuBase surface.
    uint8_t *getROMBuffer() override { return ROM; }
    size_t   getROMSize()   override { return sizeof(ROM); }
    void     loadROM(uint8_t *buffer, size_t size) override;
    void     executeUntil(int64_t cycles) override;
    // JIT CPU-stepping seam.  executeUntil's inner CPU batch calls stepCpu()
    // instead of cpu.tick() directly, so there is a single chokepoint where the
    // WASM-codegen JIT can dispatch a compiled block for the current PC (and
    // fall back to the interpreter for unsupported ops).  In the native build —
    // and whenever the JIT kill-switch is off — stepCpu() *is* cpu.tick(), so
    // the boot suite validates this path as byte-identical to the interpreter.
    // See docs/jit-engine-scope.md (W2 wiring).
    uint32_t stepCpu();
    // Cycle of the soonest scheduled SoC event strictly after `fromCycle`
    // (RTC 1 Hz, armed OSMR matches, the Series-7 synthetic 64 Hz tick, and —
    // while audio/touch are active — the audio-tick and Eiger AtoD completion),
    // or INT64_MAX when nothing is scheduled.  Hoisted out of executeUntil's WFI
    // idle path so the same deadline can bound how far the CPU (interpreter
    // batch or a JIT block-chain) may run uninterrupted before it must return to
    // service the SoC.  Read-only: inspects scheduling state, mutates nothing,
    // so it is side-effect-free to call from the hot path.  See
    // docs/jit-engine-scope.md (W2 wiring).
    int64_t nextSocEventCycle(int64_t fromCycle) const;
    int32_t  getClockSpeed() const override { return CLOCK_SPEED; }
    // Expose SDRAM bank 0 (the bank that holds the LCD framebuffer at
    // 0xC0000000) so the harness's --save-ram-snapshot can dump it and
    // we can diff it across sim-time samples to see what's changing.
    uint8_t *getRamBuffer() override       { return RAM; }
    size_t   getRamSize()   const override { return sizeof(RAM); }
    const char *getDeviceName() const override { return deviceName(); }
    int      getDigitiserWidth()  const override { return kSeries7DigitiserWidth; }
    int      getDigitiserHeight() const override { return kSeries7DigitiserHeight; }
    int      getLCDOffsetX() const override { return 0; }
    int      getLCDOffsetY() const override { return 0; }
    int      getLCDWidth()   const override { return lcdWidth(); }
    int      getLCDHeight()  const override { return lcdHeight(); }
    // LCD-controller register snapshots (blank-screen diagnostics).
    uint32_t debugLcdDbar()  const { return dbar1; }
    uint32_t debugLcdDbar2() const { return dbar2; }
    uint32_t debugLcdLccr0() const { return lccr0; }
    uint32_t debugLcdLccr1() const { return lccr1; }
    uint32_t debugLcdLccr2() const { return lccr2; }
    uint32_t debugLcdLccr3() const { return lccr3; }
    // Raw framebuffer-region byte peek (region 0xC0-0xDF -> RAM/RAM2).
    uint8_t  debugPeekRam(uint32_t addr) const {
        uint8_t region = (uint8_t)(addr >> 24);
        if (region < 0xC0 || region > 0xDF) return 0;
        const uint8_t *bank = (region >= 0xC8 && region <= 0xCF) ? RAM2 : RAM;
        size_t a = (size_t)(addr & kBankMask);
        if (a >= kBankSize) return 0;
        return bank[a];
    }
    void     readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const override;
    // Quarter-turns anticlockwise the panel image needs to be shown at.
    // Non-zero only on the netpad, and only while its EPOC build is
    // drawing rotated — see netpadScreenOrientation in sa1100.cpp.
    int      getScreenOrientation() const override;

    // ── Unique id, low half (Eiger serial EEPROM) ─────────────────────
    // The 32-bit little-endian word at EEPROM offset 0x18, which the
    // kernel reads over the ASIC command port (see initEepromImage) and
    // shows as the last two groups of Machine information's "Unique id".
    //
    // initEepromImage's defaults give this word other meanings too — byte
    // 0x18 doubles as the panel orientation and bits 31-30 as a machine
    // type selector — so an arbitrary id overwrites those.  That was
    // measured rather than assumed: a Series 7 and a netpad both cold-boot
    // cleanly with the word set to DEADBEEF (netpad boot-check variance
    // 4732.47, byte-identical to its golden run, orientation still 0), so
    // the whole word is writable and the id the user asks for is the id
    // the machine reports.
    bool     hasMachineId() const override { return true; }
    uint32_t getMachineId() const override;
    // High half of the Unique id EPOC prints, from the ROM rather than the
    // EEPROM.  Read out of the Series 7 v1.05(254) Machine information
    // dialog (which showed 0908-0001-4AFE-BA01).
    // The model UID for this build is 09080001.  It occurs TEN times in
    // that image, not once as on the Windermere ROMs — seven of them in
    // what looks like a repeated module-header field — so every copy is
    // rewritten together.  Doing all ten boots and changes the id; doing a
    // subset black-screens the machine, which is why the offsets are
    // patched as a set and never individually.
    //
    // The netBook OS image (netBook_v1.05(450)_eng.img) carries the same
    // constant the same ten times, at the analogous places in the same
    // module-header field, so it takes the same patch — only the copies
    // live on the CF card rather than in flash, because that is where its
    // OS comes from (see machineIdPrefixCardOffsets_).  The netpad's EPOC
    // R5 image has ELEVEN copies, one more than the field accounts for, and
    // its dialog has never been read to say which value is the model UID —
    // so it alone still reports 0 ("unknown") and the host shows only the
    // half it can actually change.
    static constexpr uint32_t kMachineIdPrefixDefault = 0x09080001u;
    uint32_t getMachineIdPrefix() const override { return machineIdPrefix_; }
    // The netBook reports settable with an empty slot: its copies are on
    // the OS card, which the frontend ejects on every reset, so the id is
    // normally programmed while there is no card to patch and applied to
    // whichever one is attached next.
    bool canSetMachineIdPrefix() const override {
        return !machineIdPrefixOffsets_.empty() || isNetBookBootloader_;
    }
    bool setMachineIdPrefix(uint32_t prefix) override;
    bool     setMachineId(uint32_t id) override;

    // Audio bridge (UCB1200 codec via MCP port).  Speaker DAC samples
    // pushed by the OS via MCDR0 writes drain into audio_; host pulls
    // them with readAudioOutput.  Mic samples from the host go via
    // writeAudioInput and surface to the OS via MCDR0 reads.
    bool   hasAudio() const override { return true; }
    int    getAudioSampleRate() const override { return 8000; }
    size_t readAudioOutput(int16_t *dst, size_t maxSamples) override {
        return audio_.readAudioOutput(dst, maxSamples);
    }
    // Push host mic samples into the codec's RX FIFO.  When the FIFO
    // transitions empty→non-empty AND the codec is currently enabled
    // (MCCR0.MCE=1), fire IRQ_MCP_AUDIO so the kernel's ISR can drain
    // them — same edge-fire pattern Windermere uses with CSINT (see
    // windermere.cpp Emulator::writeAudioInput).  Out-of-line so it
    // can poke icpr / recomputeInterrupts() without dragging extra
    // headers into sa1100.h.
    void   writeAudioInput(const int16_t *src, size_t count) override;
    void   setHostAudioEnabled(bool speaker, bool mic) override {
        audio_.setHostEnabled(speaker, mic);
    }
    // CF card emulation entry point.  Pulses IrqExtMedChgCf (Eiger
    // slot 13, asic[0x12] bit 13 = high byte bit 5) so the kernel
    // sees a "media changed" event.  The actual CF data isn't served
    // yet — this is a probe to see whether the kernel's
    // input-driver-init chain is gated on CF detection.  Gate behind
    // env var so default boot stays clean.
    bool     attachCard(const uint8_t *bytes, size_t size) override;
    bool     updateCardImageInPlace(const uint8_t *bytes, size_t size) override;
    void     detachCard() override;
    // netpad: the slot holds an MMC card on the FPGA's SPI port; every
    // other SA-11x0 machine here has a PC-Card socket.
    bool     isCardInserted() const override {
        return isNetpad_ ? mmcCard.inserted() : cfCard.inserted();
    }
    // CF image accessors used by the WASM bridge (readCFImage path) so
    // the frontend's CFCardDialog can pull the current bytes back and
    // list the files on the card.  The bytes come straight from
    // cfCard's image buffer, which is loaded by attach() in
    // attachCard().  Returning the staged bytes (rather than nullptr,
    // as EmuBase's default did) is essential on the netBook —
    // otherwise the user clicks "Insert CF card containing OS" and
    // the dialog still shows an empty card.
    size_t       getCardImageSize() const override {
        return isNetpad_ ? mmcCard.imageSize() : cfCard.imageSize();
    }
    const uint8_t *getCardImageData() const override {
        return isNetpad_ ? mmcCard.imageData() : cfCard.data();
    }
    // Bootloader-handoff helper: scans a FAT16 CF image for the
    // "EPOCARM ROM" magic prefixing D:\OS.IMG, strips the 256-byte
    // header, re-runs loadROM() with the body and resets the CPU.
    // Used by attachCard() when the running ROM is the netBook v0.11
    // YModem bootloader — see core/sa1100.cpp for the full rationale.
    // Returns true if the image was found and the handoff was
    // performed; false if the magic isn't present.
    bool     netBookLoadOsFromCard(const uint8_t *bytes, size_t size);
    bool     s7CardInserted_ = false;
    bool     s7PendingMedChgPulse_ = false;
    int64_t  s7MedChgPulseAtCycle_ = 0;
    // Hot-swap faithfulness: after a card is (re)inserted into the booted OS,
    // hold off the nbCfMountHook/s7CfMountHook iState=EPBusOn force until this
    // cycle, so the OS's DoorCloseEvent observes the socket still in
    // EPBusCardAbsent and fires its media-change notify (which invalidates the
    // FAT mount so the swapped-in card re-mounts).  0 = no hold (boot/handoff
    // path, where the card is present from the start and must mount eagerly).
    int64_t  cfIStateForceHoldUntil_ = 0;
    // netBook CF hot-swap: armed by attachCard() on a post-boot swap of the
    // booted netBook OS.  While set, nbCfMountHook forces F32-server's
    // CheckMount (0x50065c14) to take the dismount/remount branch the next
    // time it runs for the CF drive (E:), so the cached card1 FAT mount is
    // dropped and card2's MBR/BPB are re-read.  Cleared on first use (one
    // remount per swap — never on every access, which would thrash the boot
    // mount).  Kill switch: PSION_NB_NO_CF_ICHANGED.
    bool     nbCfSwapRemountPending_ = false;
    // The F32-server TDrive object for the CF drive (E:, drive number 4),
    // captured at its CheckMount (0x50065c14) so the swap remount-force fires
    // for E: only — not for the first (C:) CheckMount that would waste the
    // one-shot before E: is accessed.  0 = not yet captured.
    uint32_t nbCfF32Drive_ = 0;
    // netBook CF hot-swap: armed by attachCard() on a post-boot swap.  While
    // set, the executeUntil medint hook clears the CF DPrimaryMediaBase's
    // "driver open" flag (TheMedia[5]+0xc) once, so the next medata caps access
    // re-runs OpenMediaDriver (0x5001d2e0) and re-reads card2's partition table
    // / boot sector instead of reusing card1's cached geometry.  This is the
    // medint re-read gate (independent of socket power / F32 CheckMount).
    // Cleared on first use.  Kill switch: PSION_NB_NO_CF_MEDIACLOSE.
    bool     nbCfMediaClosePending_ = false;
    // netBook CF hot-swap: true between a detachCard() eject and the next
    // attachCard() insert (booted netBook OS).  While set, readAsic reports
    // STATUS_A (0x06) card-detect bits 5,6 CLEARED so the OS's card-detect
    // present-query reads "absent" and runs its door-open / power-down path
    // (instead of seeing stale "still present" bits and skipping it).
    bool     nbCfEjectedLevel_ = false;
    bool     nbCfMcDumped_ = false;
    // netBook CF hot-swap (REAL power-up mode) — faithful socket power-CYCLE.
    // The netBook OS image runs at a -0x100 offset vs the ROM file, so RUNTIME
    // entry points are (armdis file-VA - 0x100):
    //   ChangeState              (DPBusSocket::ChangeState)        0x5005d068
    //   InitiatePowerUpSequence  (DPBusSocket::PowerUp inner body) 0x5005e00c
    // Across a swap the OS never powers the CF socket down (it stays EPBusOn) so
    // the insert never re-powers/re-IDENTIFYs.  Bridge it with the OS's OWN
    // functions via callRomFunctionSync (run from executeUntil under kernCSLock):
    //   eject  -> ChangeState(socket, EPBusCardAbsent=0): drops iState 3->0 and
    //             runs the controller power-manager publish hooks (faithful;
    //             verified to return cleanly).
    //   insert -> InitiatePowerUpSequence(socket): the NON-blocking inner body
    //             PowerUp calls first (the full PowerUp 0x500595c0 has a blocking
    //             kernel-stub tail that never returns under the scheduler-frozen
    //             sync-call).  It enqueues the per-resource power-up DFC and
    //             returns; the live scheduler then drains the timer-driven
    //             sequencer -> iState 0->2->3 -> CIS/IDENTIFY -> medint refresh
    //             -> F32 re-mount.  Override the insert PC via PSION_NB_CF_PWRUP_PC.
    // Kill switch: PSION_NB_NO_CF_REPOWER.
    bool     nbCfPwrDownPending_ = false;
    uint32_t nbCfPwrCycleSocket_ = 0;
    int64_t  nbCfPwrUpAtCycle_   = 0;
    // Series 7 full-OS native CF card-detect (PSION_S7_NATIVE_CF).  The S7
    // OS's Eiger priority encoder (ROM 0x50087b24) reads ASIC[0x2a-0x2b] as
    // its PRIMARY source: `r4 & 0x2000 -> idx 13 -> slot 62 IrqExtMedChgCf`
    // (bound at handler 0x80309ca4).  Unlike the netBook OS (stubbed
    // MediaChange), the S7 OS has a functional driver stack — Medata.pdd
    // loads and slots 58/59/62/63 are bound — so signalling card-detect on
    // the register the encoder actually reads lets the real driver enumerate
    // the socket.  s7CfDetectLevel_ is the level asserted on ASIC[0x2a-0x2b]
    // bit 13 while a card-detect is pending; cleared once the slot-62 handler
    // acks (W1C to 0x2a/0x2b) so the encoder stops re-firing.
    bool     s7CfDetectLevel_ = false;
    int      s7CfDetectFires_ = 0;
    // True while a CF *removal* (eject) is being delivered through the same
    // GPIO10/encoder MedChgCf path as an insert.  An eject is also a
    // media-change, so the driver re-probes the now-empty socket and tears
    // down D:.  The flag lets the delivery loop fire even though
    // cfCard.inserted() is now false, and suppresses the "driver issued an
    // ATA command" early-stop (ataCommandCount is still non-zero from the
    // prior mount and must not be mistaken for a fresh response).  Cleared
    // when the Eiger demux acks idx 13, exactly like the insert level.
    bool     s7CfEjectPending_ = false;
    // NOTE: CF diagnostics (read histogram, dispatch-trace window, encoder
    // log caps) are deliberately kept as function-local statics in
    // sa1100.cpp rather than Emulator members.  The Emulator object is ~50 MB
    // and has a latent, layout-sensitive constructor heap-overflow (benign at
    // the HEAD layout, ASan-confirmed); adding ~1 KB of members shifts the
    // layout enough to make it fire as a process-teardown abort.  Statics
    // keep sizeof(Emulator) identical to the known-clean HEAD.
    // One-shot guard for the host-side MedChgCf-ISR invocation experiment
    // (PSION_S7_NATIVE_CF) — see core/sa1100.cpp and
    // docs/series7-cf-investigation.md.
    bool     s7CfMediaChangeCalled_ = false;
    bool     s7CfForcePowerupDone_ = false;  // PSION_S7_CF_FORCE_POWERUP one-shot
    bool     s7CfForceOpenmdDone_ = false;   // PSION_S7_CF_FORCE_OPENMD one-shot
    bool     s7CfForceReopenDone_ = false;   // PSION_S7_CF_FORCE_REOPEN one-shot
    bool     s7CfForceCreateDone_ = false;   // PSION_S7_CF_FORCE_CREATE one-shot
    bool     s7CfGenuinePwrupDone_ = false;  // genuine socket ChangeState(EPBusOn) one-shot
    // ── CF detach/reattach re-mount (Series 7 + netBook OS) ──────────────────
    // The hot-swap "faithful" media-change chain is blocked at two ends (the
    // F32-server directory cache above and live-scheduler socket re-power below
    // — see docs/netbook-cf-investigation.md).  Rather than drive that chain we
    // refresh the medint layer DIRECTLY on a reattach: clear the CF media's
    // "driver open" flag and sync-call the OS's own OpenMediaDriver, which
    // re-IDENTIFYs the (now swapped-in) card and re-reads its partition / boot
    // sector — so a subsequent F32 access serves the new card's contents.
    //
    // s7CfMediaObj_ is the CF DPrimaryMediaBase, captured at its first mount by
    // watching the medata caps handler's OpenMediaDriver call site (the media
    // whose open triggers an ATA command is the CF).  s7CfRemountPending_ is
    // armed by attachCard() on a post-boot reattach and consumed once in
    // executeUntil after the card settles.  Both devices share the kernel, so
    // the same members serve the netBook OS (different caps/OpenMediaDriver PCs).
    // Kill switch: PSION_CF_NO_REMOUNT.
    uint32_t s7CfMediaObj_ = 0;
    int64_t  s7CfRemountAtCycle_ = 0;   // 0 = no remount armed
    uint32_t s7CfF32Drive_ = 0;         // S7 F32 CF (E:) TDrive, captured at CheckMount
    bool     s7CfReattachRemount_ = false; // one-shot CheckMount media-change force
    // s7CfInPlaceFlush_ is armed by updateCardImageInPlace(): a content update
    // that keeps the live mount/medata driver alive but leaves the mount's
    // cached root-directory sector stale.  The OS re-reads file CONTENT (data
    // clusters) live but serves the directory ENTRY list from a cached copy of
    // the root-dir sector in the F32 heap that a media-change would normally
    // invalidate (which never fires on a swap; see docs).  Consumed in
    // executeUntil: locate that cached sector by content-matching the OLD
    // root-dir bytes and overwrite it with the NEW card's root-dir bytes, so the
    // next E: listing reflects the updated card — no eject/insert, no re-mount,
    // no device-absent "Corrupt".  Kill switch: PSION_CF_NO_INPLACE_FLUSH.
    bool     s7CfInPlaceFlush_ = false;
    // OLD/NEW copies of every DIRECTORY sector that changed across an in-place
    // update (root + sub-directories — the OS caches directory sectors but reads
    // file content live, so these are the only stale sectors).  Each OLD sector
    // anchors the heap search for its cached copy; the matching NEW sector
    // replaces it.  Captured in updateCardImageInPlace(); consumed by the flush
    // in executeUntil.  Parallel vectors (entry i = one 512-byte sector).
    std::vector<std::vector<uint8_t>> s7CfInPlaceOld_;
    std::vector<std::vector<uint8_t>> s7CfInPlaceNew_;
    // Parse a FAT16 BPB (bare or MBR-partitioned) and return the absolute LBA of
    // the first root-directory sector, plus the FAT1 LBA and root-dir sector
    // count via out-params.  Returns 0 if not a recognisable FAT16 image.
    static uint32_t cfRootDirLba(const uint8_t *img, size_t len,
                                 uint32_t *fat1Lba = nullptr,
                                 uint32_t *rootDirSecs = nullptr);
    // Walk a FAT16 image's directory tree (root + all sub-directories) and
    // append every directory sector's absolute LBA to `lbas`.  Bounded against
    // cyclic/corrupt FAT chains.  Used to find the sectors the F32 server caches.
    static void cfCollectDirSectors(const uint8_t *img, size_t len,
                                    std::vector<uint32_t> &lbas);
    // Virtual CompactFlash card.  Lives behind the SA-1100 PCMCIA socket 0
    // windows (physical 0x20000000-0x2FFFFFFF).  attribute memory, common
    // memory and the ATA task-file I/O window all route through this one
    // object; see readPcmcia / writePcmcia in sa1100.cpp.  Driven by the
    // netBook YModem bootloader's filesystem stack to read D:\OS.IMG.
    VCFCard  cfCard;

    // The netpad's removable-media card.  Hangs off the board FPGA's SPI
    // port rather than a PCMCIA socket (see the FPGA block above), so it
    // is a separate model from the CompactFlash one; the netpad has no
    // PC-Card socket wired up and the machines that do have no MMC slot,
    // so the two never coexist on one device.
    NetpadMmcCard mmcCard;

    // Report CF/ATA activity so the harness CF_STATS line reflects the real
    // ATA commands the netBook driver issues (ataCommandCount bumps on every
    // command-register write; sectorBoundaryCount on sector-buffer drains).
    CfStats getCfStats() const override {
        CfStats s;
        s.ataCommands  = cfCard.ataCommandCount;
        s.sectorDrains = cfCard.sectorBoundaryCount;
        return s;
    }

    // While the netBook bootloader is doing its faithful CF read
    // (PSION_NB_NATIVE_CF: the real "Loading from CF card..." path reading
    // the ~14 MB OS.IMG sector-by-sector through the medata ATA driver),
    // ask the frontend to burst extra sim frames per RAF so the load
    // compresses to a few wall-seconds instead of minutes — mirroring the
    // 5mx/5mxPro CF-gap acceleration in Windermere::cfGapActive().  Scoped
    // to the bootloader's own multi-sector reads; the post-handoff OS and
    // every other path see the default (false).
    bool cfGapActive() const override {
        return isNetBookBootloader_ && cfCard.inserted() &&
               cfCard.multiSectorReadActive();
    }

    // netBook CF: PC-Card IREQ delivery state (the card's data-ready/command-
    // complete interrupt is wired to GPIO 1 = SA1100 IRQ 1 = handler_table[1],
    // the epbus card-IRQ object).  cfIrqPrev_ edge-detects irqAsserted();
    // cfIrqLastCyc_ paces tickIrqDelay().
    bool     cfIrqPrev_ = false;
    int64_t  cfIrqLastCyc_ = 0;
    // cfIrqFiredCyc_: cycle at which we last injected the GPIO-1 edge.  GPIO 1
    // is ecust's wake handler, which does NOT W1C the GEDR edge we inject, so
    // the kernel re-dispatches source 1 forever (interrupt storm), pegging the
    // CPU and starving the medata read thread (~0.4 s/sector).  We auto-ack the
    // edge a short window after firing (simulating the W1C the real handler
    // would do) so the storm breaks and the scheduler can run medata.
    int64_t  cfIrqFiredCyc_ = 0;
    // CF chunk-boundary fix (iteration 32 — root cause found via memory-write
    // watchpoint).  0x80306de8 is the kernel interrupt-bind object for source
    // 0x3b=59 (the CF data IREQ): [+0]=source number, [+4]=enabled flag.  After
    // each 8-sector chunk the IRQ path DISABLES source 59 ([+4] 1->0); epbus
    // RE-ENABLES it ([+4] 0->1, via Interrupt::Enable(59)) only when its
    // timer-paced socket step runs (~147k cyc later) — that's the stall.  Once
    // enabled, pending source 59 fires and medata drains the next chunk.  Fix:
    // detect the enable object (the bound obj whose [+0]==59), and when a
    // multi-sector read is stalled with data ready but source 59 sits DISABLED,
    // replicate epbus's own re-enable + deliver source 59 now.  cfBindObj_ is
    // resolved at runtime by watching for the [+0]==0x3b binding.
    uint32_t cfBindObj_ = 0;            // VA of the source-59 bind object
    uint32_t cfBindLastBoundary_ = 0;
    int64_t  cfBindStallCyc_ = 0;
    uint32_t cfBindKickCount_ = 0;
    int64_t  cfBindScanCyc_ = 0;        // last bind-object heap-scan cycle
    int64_t  cfStallDiagCyc_ = 0;       // last stall-state diag log cycle
    int64_t  cfBindLastDrainCyc_ = 0;   // cycle of the last sector drain (diag)
    uint32_t cfBindLastDrainBc_ = 0;    // sector count at last drain (diag)
    // PSION_NB_VTRACE="LO,HI[,N]" control-flow tracer state.  Logs every
    // non-sequential PC transition (call / return / branch / vtable indirect)
    // with prevPC->PC, lr and r0-r3 in the cycle window [LO,HI], up to N lines.
    // Reveals the RAM-vtable-mediated interrupt dispatch (IREQ -> ecust ISR ->
    // medata DFC) that static ROM disassembly can't follow.  Use with
    // PSION_BATCH_TICKS=1 so every instruction's pre-PC is sampled.
    int64_t  nbVtraceLo_ = -2;   // -2 = uninitialised, -1 = disabled
    int64_t  nbVtraceHi_ = 0;
    int64_t  nbVtraceMax_ = 0;
    int64_t  nbVtraceCount_ = 0;
    uint32_t nbVtracePrevPc_ = 0;

    void     setKeyboardKey(EpocKey key, bool value) override;
    void     updateTouchInput(int32_t x, int32_t y, bool down) override;
    // Synthesise a TRawEvent and call Kern::AddEvent (0x50017390 on S7) to
    // inject input events directly into the kernel's event queue.  Bypasses
    // the touch-driver state machine that doesn't deliver events end-to-end.
    // No-op on non-Series-7 ROMs.  See sa1100.cpp for details.
    void     s7InjectRawEvent(uint32_t evType, int32_t paramA, int32_t paramB);
    // PSION_S7_FORCE_WSERV_DISPATCH=1 — F-FORCE-WSERV-DISPATCH experiment.
    // After s7InjectRawEvent() has called Kern::AddEvent and signalled
    // WServ's iReqSem, this routine bypasses the (broken) kernel
    // scheduler by directly restoring WServ's saved CPU context.
    // Default OFF.  Returns true if the dispatch was performed.
    // See sa1100.cpp for the full state-restore rationale.
    bool     s7ForceWservDispatch();

    MaybeU32 readPhysical(uint32_t physAddr, ValueSize valueSize) override;
    bool     writePhysical(uint32_t value, uint32_t physAddr, ValueSize valueSize) override;

private:
    bool configured = false;
    // Cached flashAliasedAt0x50() (the vtable is live by configure() time),
    // read on the physical-read slow path to alias ROM into region 0x50-0x57.
    bool flashAlias50_ = false;
    // Cached isNetpad() (see the virtual for why the SoC paths need it).
    bool isNetpad_ = false;
    // Cached ramBankCount() / romWindowBytes()-1 for the physical-access paths.
    int ramBanks_ = 2;
    uint32_t romWinMask_ = kRomSize - 1;
    void configure();

    // Register-level helpers for each peripheral block. Each returns
    // the 32-bit value the CPU should observe (8-bit reads are synthesised
    // from the low byte of the 32-bit read unless otherwise noted).
    uint32_t readPeripheral(uint32_t physAddr, ValueSize valueSize);
    void     writePeripheral(uint32_t physAddr, uint32_t value, ValueSize valueSize);
    // PSION_TRACE_PERIPH=1 — emit one printf per MMIO access.  Symmetric
    // with Windermere::Emulator::tracePeriph; see implementation in
    // sa1100.cpp.
    void     tracePeriph(const char *op, uint32_t physAddr, int sz, uint32_t value);

    uint32_t readUart(uint32_t base, uint32_t off, ValueSize vs);
    void     writeUart(uint32_t base, uint32_t off, uint32_t value, ValueSize vs);

    uint32_t readOsTimer(uint32_t off, ValueSize vs);
    void     writeOsTimer(uint32_t off, uint32_t value, ValueSize vs);

    uint32_t readRtc(uint32_t off, ValueSize vs);
    void     writeRtc(uint32_t off, uint32_t value, ValueSize vs);

    uint32_t readPwr(uint32_t off, ValueSize vs);
    void     writePwr(uint32_t off, uint32_t value, ValueSize vs);

    uint32_t readGpio(uint32_t off, ValueSize vs);
    void     writeGpio(uint32_t off, uint32_t value, ValueSize vs);

    // netpad nCS4 (physical region 0x40) board register file.  The boot
    // ROM initialises a board-specific serial peripheral here by busy-
    // waiting on its TX-ready status bit and clocking a byte stream out
    // (see readGpio and FUN_500044ac).  We don't model the target device,
    // so reads report the transmitter permanently ready; writes land in
    // npBoard_ so the registers the OS *does* use for something visible
    // (the contrast DAC, see below) can be read back by the emulator.
    uint32_t readNcs4Serial(uint32_t physAddr, ValueSize vs);
    void     writeNcs4Serial(uint32_t physAddr, uint32_t value, ValueSize vs);

    // ── netpad board FPGA: interrupt controller + MMC SPI port ───────
    //
    // Two blocks of the nCS4 register file are modelled (the rest keeps
    // the permissive defaults in readNcs4Serial):
    //
    //   0x10  interrupt status — bit n is the variant interrupt EPOC
    //         names "IrqExternal<n>".  The ASSP finds the lowest pending
    //         line here (ROM 0x500a7004) and acknowledges by writing the
    //         bit back (0x500a6fac).
    //   0x12  interrupt enable mask (0x500a6f74 / 0x500a6f90).
    //   0x1a  board status; bit 6 is the MMC card-detect switch, read
    //         inverted by the variant's socket-0 card-present helper
    //         (0x500a8014: present == !(reg & 0x40)).
    //   0x100 MMC SPI control: bit 0 enable, bit 3 selects 8-bit frames
    //         over 16-bit, bit 5 is the card's chip select.  The variant
    //         asserts bit 5 around each transaction (0x500a8498 /
    //         0x500a84a4) and medmmc.pdd flips bit 3 around the 512-byte
    //         block transfers (ROM 0x50368830 / 0x50368874).
    //   0x102 status: bit 0 busy, bit 3 port ready.
    //   0x104 clock divider — the variant programs 400 kHz for the card
    //         identification phase and steps it up afterwards.
    //   0x106 16-bit data register; 0x108 the 8-bit one.
    static constexpr uint32_t kNpFpgaIrqStatus = 0x10;
    static constexpr uint32_t kNpFpgaIrqEnable = 0x12;
    static constexpr uint32_t kNpFpgaBoardStat = 0x1a;
    static constexpr uint32_t kNpFpgaMmcCardDetect  = 0x40;  // 1 = slot empty
    static constexpr uint32_t kNpFpgaMmcMediaPresent = 0x08; // 1 = card in
    static constexpr uint32_t kNpMmcCtrl    = 0x100;
    static constexpr uint32_t kNpMmcStatus  = 0x102;
    static constexpr uint32_t kNpMmcClkDiv  = 0x104;
    static constexpr uint32_t kNpMmcData16  = 0x106;
    static constexpr uint32_t kNpMmcData8   = 0x108;
    static constexpr uint16_t kNpMmcCtrlEnable     = 0x0001;
    static constexpr uint16_t kNpMmcCtrlByteWide   = 0x0008;
    static constexpr uint16_t kNpMmcCtrlChipSelect = 0x0020;
    static constexpr uint32_t kNpMmcStatusReady    = 0x0008;
    // The FPGA's interrupt output pin on the SoC.  GPIO 10 is the only
    // GPIO besides the power button (1), GPIO 0 and the pen-detect line
    // (14) whose rising edge the netpad's EPOC build ever arms.
    static constexpr int kNetpadFpgaIrqGpio = 10;
    // Variant interrupt line for the card slot's media change.  The
    // variant maps socket 0 (the MMC slot) to "IrqExternal11" and the
    // PC-Card socket to "IrqExternal12" — ROM 0x500a8088.
    static constexpr int kNetpadMmcMediaChangeLine = 11;

    // ── netpad AC'97 audio controller ────────────────────────────────
    //
    // The netpad's sound hardware is an AC'97 codec on a controller in
    // the same board FPGA as the MMC port, five halfword registers at
    // nCS4 0x200 (VA 0x58030200).  Three pieces of the ROM drive it and
    // they agree on the map, which is where every constant below comes
    // from:
    //
    //   * the ASSP's accessors inside EKern.exe — ROM 0x50005118
    //     (control read-modify-write), 0x50005140 (status, masked
    //     0x9f), 0x50005158 (write a codec register), 0x50005180 /
    //     0x50005194 (start a codec-register read / collect it) and
    //     0x500051a0 / 0x500051b0 (write / read a PCM sample);
    //   * \System\Libs\Esdrv.pdd, the "Sound.Ac97" sound PDD, which
    //     calls those accessors — its record path is at ROM 0x502e7ac8
    //     and its play path at 0x502e795c;
    //   * \System\Libs\d_ac97.ldd, the "Codec.Ac97" test driver, which
    //     carries its own copy of the same accessors (ROM 0x5036cd2c
    //     onwards) plus straight-line Play / Record loops at 0x5036c8b8
    //     and 0x5036c938.
    //
    //   0x200  PCM data.  Writing pushes a playback sample into the TX
    //          FIFO; reading pops a recorded one out of the RX FIFO.
    //          16-bit signed, one sample per access.
    //   0x202  codec register index.  Bit 0 selects direction: writing
    //          index|1 starts a read whose result appears at 0x204;
    //          writing an even index commits the value staged at 0x204.
    //   0x204  codec register data (staged before a write, read back
    //          after a read).
    //   0x206  control.  Bit 0 releases the codec's reset, bit 1
    //          enables the AC-link, bit 2 is a receive-FIFO flush the
    //          sound PDD pulses at the start of a recording.
    //   0x208  status, of which the drivers use bit 0 (RX FIFO empty),
    //          bit 4 (TX FIFO full) and bit 7 (codec command busy).
    //
    // Both FIFOs are 16 deep: the play path pre-fills exactly sixteen
    // samples before it enables its interrupt (ROM 0x502e79d8) and the
    // record ISR drains eight per service request (0x502e7a5c).
    static constexpr uint32_t kNpAc97Data   = 0x200;
    static constexpr uint32_t kNpAc97Index  = 0x202;
    static constexpr uint32_t kNpAc97Value  = 0x204;
    static constexpr uint32_t kNpAc97Ctrl   = 0x206;
    static constexpr uint32_t kNpAc97Status = 0x208;
    static constexpr uint16_t kNpAc97IndexRead   = 0x0001;
    static constexpr uint16_t kNpAc97IndexMask   = 0x007E;
    static constexpr uint16_t kNpAc97CtrlReset   = 0x0001;
    static constexpr uint16_t kNpAc97CtrlEnable  = 0x0002;
    static constexpr uint16_t kNpAc97CtrlRxFlush = 0x0004;
    static constexpr uint16_t kNpAc97StatRxEmpty = 0x0001;
    static constexpr uint16_t kNpAc97StatTxFull  = 0x0010;
    static constexpr uint16_t kNpAc97StatBusy    = 0x0080;
    static constexpr uint16_t kNpAc97StatMask    = 0x009F;
    // AC'97 codec registers the ROM's drivers touch by number.
    static constexpr uint8_t kAc97RegPowerdown = 0x26;  // + ready status
    static constexpr uint8_t kAc97RegDacRate   = 0x2C;  // PCM front DAC rate
    static constexpr uint8_t kAc97RegAdcRate   = 0x32;  // PCM L/R ADC rate
    static constexpr uint16_t kAc97PowerdownReady = 0x000F;  // REF/ANL/DAC/ADC
    // The two variant interrupt lines the sound drivers bind, named
    // "IrqExternal3" and "IrqExternal4" in both Esdrv.pdd's and
    // d_ac97.ldd's string tables.  They are the codec's play and record
    // FIFO service requests; which name carries which direction is not
    // decidable from the ROM (both drivers bind both names and neither
    // driver is ever opened during an ordinary session, so no trace
    // pins it down).  It doesn't have to be: the sound PDD runs one
    // direction at a time and arms only the line it needs, so
    // netpadAc97ServiceIrq falls back to whichever of the two is
    // actually enabled and the model is correct under either mapping.
    static constexpr int kNetpadAc97PlayLine   = 3;
    static constexpr int kNetpadAc97RecordLine = 4;

    struct NetpadAc97 {
        static constexpr int kFifoDepth    = 16;
        static constexpr int kRxServiceLvl = 8;   // samples per record IRQ
        static constexpr int kTxServiceLvl = 8;   // room before a play IRQ
        uint16_t codecReg[64] = {};
        uint16_t ctrl        = 0;
        uint16_t staged      = 0;   // value written to 0x204 before an index
        uint16_t readback    = 0;   // value 0x204 returns after a read request
        int16_t  tx[kFifoDepth] = {};
        int16_t  rx[kFifoDepth] = {};
        int      txCount = 0;
        int      rxCount = 0;
        bool     txArmed = false;   // guest has queued playback samples
        bool     rxArmed = false;   // guest has started a recording
        // Cycles banked towards the next sample in each direction, and
        // when the clock was last advanced.  Counting in cycles rather
        // than in ticks is what keeps the codec at its programmed rate:
        // the emulator's audio tick fires a little late whenever the
        // CPU overshoots its deadline, and a per-tick counter loses
        // that overshoot (measurably — a quarter of a recording).
        int64_t  txCycles = 0;
        int64_t  rxCycles = 0;
        int64_t  lastTickCycle = 0;
        int64_t  lastPcmCycle = 0;  // last touch of the PCM data register
    } npAc97_;

    // True while the codec is streaming.  The FIFO clock only has to
    // keep up with the codec's sample rate while a transfer is running,
    // and the machine is idle most of the time, so this is what keeps
    // netpadAc97Tick out of the SoC's wake set the rest of the time.
    // "Streaming" is measured from the last access to the PCM data
    // register (or the receive-FIFO flush that opens a recording)
    // rather than from a driver state bit, because the sound PDD's stop
    // path powers the codec's sections down through its own registers
    // and leaves the controller enabled.
    bool     netpadAc97Streaming() const {
        return netpadAc97LinkEnabled() && npAc97_.lastPcmCycle != 0 &&
               passedCycles - npAc97_.lastPcmCycle < CLOCK_SPEED;
    }

    void     netpadAc97Reset();
    bool     netpadAc97Read(uint32_t reg, uint32_t &out);
    bool     netpadAc97Write(uint32_t reg, uint16_t value);
    void     netpadAc97Tick();
    void     netpadAc97ServiceIrq(bool record);
    uint16_t netpadAc97CodecRead(uint8_t index) const;
    int      netpadAc97Rate(uint8_t rateReg) const;
    bool     netpadAc97LinkEnabled() const {
        return (npAc97_.ctrl & (kNpAc97CtrlReset | kNpAc97CtrlEnable)) ==
               (kNpAc97CtrlReset | kNpAc97CtrlEnable);
    }
    // PSION_NETPAD_AC97_TRACE — log codec register traffic, FIFO
    // starts/stops and service interrupts.
    bool     npAc97Trace_ = false;

    struct NetpadFpga {
        static constexpr int kRxDepth = 8;
        uint16_t rx[kRxDepth] = {};
        uint8_t  rxCount   = 0;
        uint16_t spiCtrl   = 0;
        uint16_t spiClkDiv = 0;
        uint16_t irqStatus = 0;
        uint16_t irqEnable = 0;
    } npFpga_;

    bool     netpadFpgaRead(uint32_t reg, uint32_t &out);
    bool     netpadFpgaWrite(uint32_t reg, uint32_t value);
    void     netpadFpgaRaiseIrq(int line);
    void     recomputeNetpadFpgaIrq();
    uint16_t netpadMmcPopRx();
    void     netpadMmcPushRx(uint16_t v);
    void     netpadMmcTransfer(uint16_t txWord, int bytes);
    bool     netpadAttachMmc(const uint8_t *bytes, size_t size);
    void     netpadDetachMmc();
    // PSION_NETPAD_MMC_TRACE — log every SPI frame and card-detect edge.
    bool     npMmcTrace_ = false;

    // ── netpad screen contrast ───────────────────────────────────────
    // The board's register file is a set of 16-bit locations on nCS4,
    // reached through VA 0x58030000 (the ASSP's WriteBoardReg helper at
    // ROM 0x50005510); only the low byte of each carries data.
    //
    // Register 0x0a is a 5-bit contrast DAC feeding the
    // panel's bias generator: the boot stub parks it at full scale (0x1f)
    // and EPOC then programs the user's setting from Control panel →
    // Screen.  The dialog's level L (as shown in the "Contrast" spinner)
    // maps to 4L − 4 clipped to the DAC's 0..31 range, so its factory
    // default (level 6) is 20 — which is what we treat as "no change" in
    // the render path.  See applyNetpadContrast in sa1100.cpp.
    static constexpr uint32_t kNpContrastReg     = 0x0a;
    static constexpr uint8_t  kNpContrastDefault = 20;
    static constexpr uint8_t  kNpContrastMax     = 31;
    // Softening term in the render-side gain (see applyNetpadContrast).
    static constexpr int      kNpContrastFloor   = 8;
    uint8_t npContrast_ = kNpContrastDefault;
    // Apply the current contrast setting to one decoded palette channel.
    // Identity when the DAC sits at its default, so every device that
    // isn't the netpad — and a netpad the user hasn't touched — renders
    // byte-for-byte as before.
    uint8_t applyNetpadContrast(uint8_t channel) const;

    // ── netpad screen orientation ────────────────────────────────────
    // "Switch orientation" on the netpad's Tools menu turns the machine
    // from a landscape slab into a portrait one, and none of it happens
    // in hardware: LCCR1/LCCR2/DBAR1 are untouched across the switch, so
    // the LCD controller keeps scanning the same 640x240 panel out of the
    // same framebuffer and EPOC's screen driver simply starts drawing the
    // UI rotated inside it.  A host showing the machine the way its user
    // holds it therefore has to rotate the panel image itself, and to do
    // that it has to be told — which means reading the state out of the
    // guest, since nothing on the bus carries it.
    //
    // It lives in the screen driver's draw device, an ScDv.dll object in
    // RAM whose tail is unmistakable:
    //
    //   +0x00  vtable, into ScDv.dll (\System\Libs, ROM 0x502d3ce0)
    //   +0x20  panel width   0x280 = 640
    //   +0x24  panel height  0x0f0 = 240
    //   +0x28  framebuffer VA + 0x200 — past the 256-entry palette the
    //          LCD DMA reads from the head of the buffer
    //   +0x30  orientation: 0 landscape, 1 rotated (Symbian's
    //          TOrientation runs 0..3; this ROM's menu toggles 0 <-> 1)
    //   +0x34  framebuffer VA — the kernel's mapping of the panel, which
    //          the LCD controller scans from PA 0xC0000000
    //
    // so netpadFindDrawDevice locates it by scanning bank 0 for the pixel
    // pointer at +0x28 and confirming the geometry around each hit —
    // exactly one match in the full 16 MB bank — and getScreenOrientation
    // reads the quadrant out of it.  Which way to turn the panel was
    // settled by driving the menu through the harness and rotating the
    // screenshot: anticlockwise is the one that comes out upright
    // (tests/stress/netpad_orientation.sh).
    //
    // The located address is cached between polls, in a file-scope static
    // in sa1100.cpp rather than a member — see the note on the CF
    // diagnostics above for why this object's layout is left alone.
    static constexpr uint32_t kNpFramebufferVa   = 0x58040000;
    static constexpr uint32_t kNpDrawDevWidth    = 0x20;   // object offsets
    static constexpr uint32_t kNpDrawDevHeight   = 0x24;
    static constexpr uint32_t kNpDrawDevFbPixels = 0x28;
    static constexpr uint32_t kNpDrawDevOrient   = 0x30;
    static constexpr uint32_t kNpDrawDevFbBase   = 0x34;
    // Sim cycles between re-scans while the object hasn't been found —
    // it only exists once the screen driver is up, so a machine still in
    // early boot would otherwise re-scan on every poll.
    static constexpr int64_t  kNpDrawDevScanGap  = CLOCK_SPEED;   // ~1 s
    uint32_t netpadFindDrawDevice() const;
    bool     netpadDrawDeviceAt(uint32_t pa) const;
    uint32_t netpadRamWord(uint32_t pa) const;

    // netpad bit-banged I2C slave (SCL = GPIO 15, SDA = GPIO 16).  The
    // boot ROM drives the bus open-drain by flipping GPDR direction bits
    // (input = released → pull-up high, output = driven low; the output
    // latch stays 0).  Without a slave every transaction ends in NACK and
    // the board init retries forever, so we decode the waveform from the
    // GPDR writes and act as a permissive slave: ACK every address, sink
    // written bytes, and shift out a configurable byte for reads.  State
    // is a small FSM stepped on every GPIO register write (see
    // netpadI2cUpdate in sa1100.cpp).
    struct NetpadI2c {
        bool    sclPrev  = true;   // previous SCL bus level (idles high)
        bool    sdaPrev  = true;   // previous SDA bus level (idles high)
        bool    slaveSdaLow = false; // slave is pulling SDA low right now
        uint8_t phase    = 0;      // Phase enum in sa1100.cpp
        uint8_t bitCount = 0;
        uint8_t shift    = 0;      // RX shift register (addr / write data)
        uint8_t addr     = 0;      // last address byte (incl. R/W bit)
        uint8_t txByte   = 0;      // TX shift register (read data)
        // ── board-controller command state (see netpadBoardCtlWrite) ──
        uint8_t  cmd     = 0;      // opcode byte of the transfer in flight
        uint8_t  pending = 0;      // opcode still waiting for its argument
        uint16_t ptr     = 0;      // 16-bit read pointer set by 0x91/0x92
        uint8_t  msg     = 0;      // pack message selected by 0x96
    } npI2c_;
    void    netpadI2cUpdate();
    // One byte written to / read from the board controller at slave 0x5a.
    void    netpadBoardCtlWrite(uint8_t byte);
    uint8_t netpadBoardCtlRead();
    // Backing store for the controller's 16-bit-addressed data window.
    uint8_t netpadBoardCtlData(uint16_t addr) const;
    // Smart-battery payload the controller reports (see
    // netpadBoardCtlData in sa1100.cpp for the derivation of each byte).
    static constexpr uint8_t kNpPackTempC       = 25;   // room temperature
    static constexpr uint8_t kNpPackVolts10mV   = 200;  // 2.00 V raw reading
    static constexpr uint8_t kNpPackGaugeGood   = 1;    // calibrated, ~95%
    static constexpr uint8_t kNpPackGaugeLow    = 18;   // calibrated, near 0%
    static constexpr uint8_t kNpPackDepletedLow = 100;  // PSION_NETPAD_BATT_LOW
    // Pack messages the driver asks for with 0x96 <n> (see
    // netpadBoardCtlData).  Only the manufacturer needs its own payload:
    // every other message the ER5 build issues either reads bytes the
    // status block already defines or fields we leave at zero.
    static constexpr uint8_t kNpMsgManufacturer = 5;    // 3-char maker code
    // The maker code, exactly three characters — the driver reads three
    // bytes into a three-character descriptor and the Shell prints all
    // three, so there is no room for a longer name; a shorter one pads
    // with spaces, which the page draws as trailing blanks.
    static constexpr char    kNpPackMaker[3]    = { 'J', 'H', ' ' };

    // PSION_NETPAD_SSP_TRACE — trace the netpad's SSP traffic (board
    // codec: touchscreen + ADC channels).  See readSsp().
    bool netpadSspTrace() const;

    // ── netpad SSP board codec (ADS7846-class touch / ADC controller) ──
    //
    // The netpad hangs its touchscreen-and-ADC controller off the REAL
    // SA-1110 SSP (PA 0x80070000) rather than a Psion ASIC, and drives it
    // exactly the way the ADS7846/TSC2046 datasheet prescribes: an 8-bit
    // control byte (S | A2 A1 A0 | MODE | SER-DFR | PD1 PD0) followed by
    // enough clocks to shift the 12-bit result back, with the chip select
    // (GPIO 13) held around the whole batch.
    //
    // The variant's sampler (ROM 0x5000a0dc / 0x5000a204) queues four
    // command/dummy word PAIRS into the 8-deep transmit FIFO, waits for
    // SSSR.BSY to drop, then reads all eight receive words back and
    // rebuilds the sample from the last two:
    //     value = ((rx[6] & 7) << 9) | ((rx[7] & 0xff8) >> 3)
    // which is precisely "busy bit + DB11..DB9" in one 12-bit frame and
    // "DB8..DB0 + 3 trailing zeros" in the next.  So we model the slave at
    // BIT level and let SSP framing fall out of it: any DSS / batching the
    // driver picks produces the right answer.
    struct NetpadSsp {
        static constexpr int kFifoDepth = 8;   // SA-1110 SSP FIFOs are 8 deep
        uint16_t tx[kFifoDepth] = {};
        uint16_t rx[kFifoDepth] = {};
        uint8_t  txCount = 0;
        uint8_t  rxCount = 0;
        bool     ror     = false;              // receive overrun (sticky)
        bool     busy    = false;              // a frame is on the wire
        uint16_t frameWord    = 0;             // word being shifted out
        int64_t  frameEndCycle = 0;            // when it finishes
        // ADS7846 serial state.  The slave hunts for a start bit, collects
        // eight control bits, then clocks out a leading busy bit followed
        // by the 12 result bits (MSB first); everything after that is zero.
        uint8_t  cmdShift = 0;
        uint8_t  cmdBits  = 0;                 // 0 = hunting for the start bit
        uint16_t outShift = 0;
        uint8_t  outBits  = 0;
    } npSsp_;

    uint32_t netpadSspRead(uint32_t off);
    void     netpadSspWrite(uint32_t off, uint32_t value);
    void     netpadSspTick();                  // called from the tick loop
    void     netpadSspStartFrame();
    void     netpadSspFinishFrame();
    void     netpadSspFlush();                 // SSE 1->0 clears both FIFOs
    uint8_t  netpadSspFrameBits() const;       // DSS + 1
    int64_t  netpadSspFrameCycles() const;     // bits × SCR-derived bit time
    uint8_t  netpadAdsClockBit(uint8_t txBit); // one SCLK edge on the codec
    uint16_t netpadAdsConvert(uint8_t control) const;
    void     recomputeNetpadSspIrq();
    // Next SSP frame completion, for the deadline batcher / WFI wake.
    int64_t  netpadSspNextEvent() const {
        return (isNetpad_ && npSsp_.busy) ? npSsp_.frameEndCycle : INT64_MAX;
    }

    // netpad sleep/wake.  The netpad OS boots to the EPOC "off" state
    // (PMCR.SF=1 deep sleep with a resume context parked at PSPR); on
    // real hardware the power button (PWER GPIO 1) triggers a sleep-mode
    // RESET, and the ROM stub at PA 0 sees RCSR.SMR, re-inits SDRAM and
    // jumps through the PSPR context.  npWakeAtCycle_ is the one-shot
    // synthetic power-button press armed at the first sleep entry;
    // netpadSleepExitReset() performs the faithful wake-by-reset.
    int64_t npWakeAtCycle_ = 0;
    // While non-zero, the synthetic power button is held down (GPLR bit
    // high); the per-frame hook releases it when this cycle passes.
    int64_t npWakeReleaseAtCycle_ = 0;
    // Deep-sleep external wake timer (board controller -> GPIO 27):
    // armed on sleep entry, fires ~31 ms later.  0 = not armed.
    int64_t npSleepPulseAtCycle_ = 0;
    bool    npAutoWakeDone_ = false;
    void netpadSleepExitReset();
    // Synthetic power-button press (wake edge + the GPIO 14 level the
    // resume stub samples).  See the definition in sa1100.cpp.
    void netpadPressPowerButton(const char *why);

    // netpad key delivery.  The machine has no keyboard hardware, so
    // host key presses are handed to the kernel's own Kern::AddEvent as
    // synthetic TRawEvents.  See netpadInjectKeyEvent in sa1100.cpp.
    void     netpadInjectKeyEvent(uint32_t evType, int32_t scanCode);
    // Which EPOC scan codes the host currently holds down.  A matrix
    // machine absorbs a repeated press of a held key; a stream of
    // synthetic TRawEvents does not, so setKeyboardKey only forwards
    // genuine edges.  256 covers the EStdKey space (max EStdKeyMenu-class
    // codes are < 0x100).
    std::bitset<256> npKeysDown_;
    // PSION_NETPAD_PC_RANGE=<lo>-<hi> — first-visit PC tracer (see stepCpu).
    bool     npPcTrace_ = false;
    // PSION_NETPAD_LR_WATCH=<addr> — indirect-branch callee tracer.
    uint32_t npLrWatch_ = 0;
    uint32_t npPcTraceLo_ = 0, npPcTraceHi_ = 0;
    std::set<uint32_t> npPcSeen_;
    // Page-table walk for a kernel VA -> physical address (0 = unmapped).
    uint32_t resolveKernelPA(uint32_t va);

    uint32_t readIntc(uint32_t off, ValueSize vs);
    void     writeIntc(uint32_t off, uint32_t value, ValueSize vs);

    uint32_t readPpc(uint32_t off, ValueSize vs);
    void     writePpc(uint32_t off, uint32_t value, ValueSize vs);

    uint32_t readDma(uint32_t off, ValueSize vs);
    void     writeDma(uint32_t off, uint32_t value, ValueSize vs);

    uint32_t readPwm(uint32_t off, ValueSize vs);
    void     writePwm(uint32_t off, uint32_t value, ValueSize vs);

    uint32_t readLcd(uint32_t off, ValueSize vs);
    void     writeLcd(uint32_t off, uint32_t value, ValueSize vs);

    uint32_t readMemCfg(uint32_t off, ValueSize vs);
    void     writeMemCfg(uint32_t off, uint32_t value, ValueSize vs);

    // Per-UART state. UART3 is used as the EPOC kernel serial console
    // on the netBook; the others stay idle but have the same layout so
    // we share a single struct.
    struct UartState {
        uint32_t utcr0 = 0;
        uint32_t utcr1 = 0;
        uint32_t utcr2 = 0;
        uint32_t utcr3 = 0;
        // Default UTSR0: TFS (bit 0) set — TX FIFO Service Request.
        // On real SA-1100 silicon, TFS = 1 at cold boot because the
        // empty TX FIFO is below the service threshold.  The EUART PDD
        // probes UTSR0 to detect UART hardware existence; reading 0
        // made it skip ISR binding entirely.
        uint32_t utsr0 = 0x01;
        // Default UTSR1 = 0x04 (TNF only — TX FIFO not full). The legacy
        // value 0x06 also set RNE which made a polling RX loop spin
        // forever because UTDR always returns 0; readUart() now
        // computes RNE dynamically from the rxQueue below, but keep
        // the static base value with TNF only so the dynamic OR works.
        uint32_t utsr1 = 0x04;       // TNF only by default

        // RX FIFO — bytes queued for the guest to read via UTDR.
        // Pushed by injectUartRx() (from harness / YModem state
        // machine), popped by readUart() UTDR.  UTSR1 RNE bit
        // mirrors !empty.
        std::vector<uint8_t> rxQueue;
        // Per-port byte index into rxQueue for the next pop.  Using
        // an index rather than erase-from-front keeps the queue's
        // pointers stable for snapshot/restore.
        size_t   rxQueueHead = 0;

        bool rxEmpty() const { return rxQueueHead >= rxQueue.size(); }
        void rxPush(uint8_t b) { rxQueue.push_back(b); }
        uint8_t rxPop() {
            if (rxEmpty()) return 0;
            return rxQueue[rxQueueHead++];
        }

        // Host bridge state — set by SA1100::Emulator::serialAttachHost
        // when the frontend's Remote Link dialog connects.  When true,
        // UTDR writes go into txQueue instead of being dropped, and the
        // host can drain them via serialReadToHost().  Host-pushed
        // bytes land in rxQueue via serialWriteFromHost (which routes
        // through the existing injectUartRx mechanism so kernel-side
        // RNE/IRQ bookkeeping stays consistent).
        bool                 hostAttached = false;
        std::vector<uint8_t> txQueue;
    };
    UartState uart0, uart1, uart2, uart3, uart4;
    UartState *resolveUart(uint32_t base);

public:
    // Push a byte into a UART's RX FIFO.  port is 0..4 matching the
    // SA-1100 SerN bases; harness uses this to feed YModem packets
    // or test-character streams into the bootloader's serial input.
    // Per-port enable gates (UTCR3 bit 1 = RX enable) aren't
    // enforced here — the guest's driver code is responsible for
    // checking before reading.
    void injectUartRx(int port, const uint8_t *bytes, size_t len);

    // Host serial bridge for the Remote Link / PsiWin file-transfer
    // dialog.  Mirrors Windermere::Emulator's API so the WASM dispatch
    // in wasm/main.cpp can route through a common shape.  uartIndex is
    // 1..3 matching SA-1100's UART1/UART2/UART3 — the frontend's
    // current convention is index 2 (the "cable" port on Windermere);
    // on netBook the docking cable is wired to UART1 but we accept any
    // valid index so the bridge works with whichever UART the EPOC R5
    // RemoteLink service decides to claim.
    bool   serialAttachHost(int uartIndex);
    bool   serialDetachHost(int uartIndex);
    bool   serialIsAttached(int uartIndex) const;
    size_t serialWriteFromHost(int uartIndex, const uint8_t *data, size_t len);
    size_t serialReadToHost(int uartIndex, uint8_t *dst, size_t cap);

    // ── Audio diagnostics ───────────────────────────────────────────
    // Used by harness/netbook-audio-harness.cpp to inspect the codec
    // state without dynamic_cast / friending.  Read-only.
    uint64_t debugCodrReads()  const { return audio_.codrReads;  }
    uint64_t debugCodrWrites() const { return audio_.codrWrites; }
    size_t   debugDacFill()    const { return audio_.dacRingFill(); }
    // Peak-to-peak amplitude of the samples currently queued for the host
    // DAC — used by the harness to prove playback clocks the REAL recorded
    // clip (varying samples) and not silence/a constant.
    int      debugDacPeakToPeak() const {
        const auto &q = audio_.dacQueueRef();
        int lo = 32767, hi = -32768, seen = 0;
        for (int16_t s : q) { if (s == 0) continue; seen++;
            if (s < lo) lo = s; if (s > hi) hi = s; }
        return seen ? (hi - lo) : 0;
    }
    // Estimate the dominant frequency of the DAC queue by zero-crossing rate.
    // Used to prove a recorded/played tone is intact (a clean 440 Hz input
    // round-trips to ~440 Hz; heavy sample loss/corruption skews it).
    int      debugDacFreqHz() const {
        const auto &q = audio_.dacQueueRef();
        int n = (int)q.size();
        int crossings = 0, samples = 0, prevSign = 0;
        for (int i = 0; i < n; i++) {
            int16_t s = q[i];
            int sign = (s > 64) ? 1 : (s < -64) ? -1 : 0;
            if (sign != 0) {
                samples++;
                if (prevSign != 0 && sign != prevSign) crossings++;
                prevSign = sign;
            }
        }
        if (samples < 16) return 0;
        // crossings over `samples` sample-periods (8 kHz): Hz = crossings/2 * 8000/samples
        return (int)((double)crossings * 0.5 * 8000.0 / (double)samples);
    }
    bool     debugNbPlayActive() const { return nbPlayActive_; }
    // Zero-crossing frequency estimate over the recorded netBook clip
    // (playbackClip_ holds the int8 PCM the kernel captured from the SAC RX).
    // A clean 440 Hz input should round-trip to ~440; a 3x-style capture
    // decimation skews it.  Also reports the clip length.
    int      debugClipFreqHz() const {
        int n = (int)playbackClip_.size();
        int crossings = 0, samples = 0, prevSign = 0;
        for (int i = 0; i < n; i++) {
            int s = (int)(int8_t)playbackClip_[i];
            int sign = (s > 4) ? 1 : (s < -4) ? -1 : 0;
            if (sign != 0) { samples++;
                if (prevSign != 0 && sign != prevSign) crossings++;
                prevSign = sign; }
        }
        if (samples < 16) return 0;
        return (int)((double)crossings * 0.5 * 8000.0 / (double)samples);
    }
    size_t   debugClipLen() const { return playbackClip_.size(); }
    uint64_t debugS7SsdrTotal()  const { return dbgS7SsdrTotal_; }
    uint64_t debugS7SsdrSilent() const { return dbgS7SsdrSilent_; }
    size_t   debugAdcFill()    const { return audio_.adcRingFill(); }
    uint32_t debugMccr0()      const { return mccr0_; }
    uint16_t debugUcbAudioCtrl0() const { return ucbAudioCtrl0_; }
    uint16_t debugUcbAudioCtrl1() const { return ucbAudioCtrl1_; }
    uint32_t debugPwm1Ctrl()   const { return pwm_[0].ctrl; }
    uint32_t debugPwm1Pscr()   const { return pwm_[0].pscr; }
    uint32_t debugPwm1Pwdr()   const { return pwm_[0].pwdr; }
    int      debugPwmToneHz()  const { return pwmBuzzerToneHz_; }
    // Recording-path diagnostics for harness Stage D.
    bool     debugEsdrvPcSeen()       const { return esdrvPcSeen_; }
    bool     debugRecordingDfcWaiting() const { return recordingDfcWaiting_; }
    uint32_t debugRecEsdrvChannel() const { return recEsdrvChannel_; }
    uint32_t debugRecordChannelPtr() const { return recordChannelPtr_; }
    uint32_t debugRecDmaEngChan() const { return recDmaEngChan_; }
    uint64_t debugSadrReads() const { return dbgSadrReads_; }
    uint16_t debugSscr0() const { return sscr0_; }
    uint16_t debugSscr1() const { return sscr1_; }
    uint64_t debugSacsrReads() const { return dbgSacsrReads_; }
    uint64_t debugIrqSspFires() const { return dbgIrqSspFires_; }
    uint64_t debugRecDmaIrqBuffers() const { return recDmaIrqBuffers_; }
    uint64_t debugRecDmaEngBuffers() const { return recDmaEngBuffers_; }
    uint64_t debugInjAttempt()  const { return dbgInjAttempt_; }
    uint64_t debugInjBlkMask()  const { return dbgInjBlkMask_; }
    uint64_t debugInjBlkPend()  const { return dbgInjBlkPend_; }
    uint64_t debugInjEval()     const { return dbgInjEval_; }
    uint64_t debugAudioTicks()  const { return dbgAudioTicks_; }
    bool     debugIsSeries7Rom() const { return isSeries7Rom_; }
    bool     debugIsNetBookRom() const { return isNetBookRom_; }

    // Faithful touch routing (real digitiser + Eiger ADC instead of the
    // synthetic-TRawEvent injection).  Default ON for the Series 7 raw ROM;
    // opt out with PSION_S7_LEGACY_TOUCH=1.
    //
    // The full faithful chain now works: pen-down GPIO IRQ → Eiger ADC
    // conversions completed by service-busy state → fast completion (the WFI
    // skip wakes on the scheduled AtoD IRQ, see executeUntil, so a sample
    // burst finishes in ~1.5 ms instead of ~90 ms) → per-channel accumulation
    // → FUN_5008a724 extraction (the stale boot pen-up flag is cleared at
    // pen-down, see updateTouchInput) → ROM calibration FUN_5008b19c → WServ.
    // Verified: taps land on the icon under the pointer across the screen.
    //
    // Both the Series 7 and the netBook are the same SA-1100 + Eiger +
    // UCB1200 digitiser hardware, so both default to the faithful
    // digitiser/ADC touch path — a real tap drives the emulated Eiger/ADC
    // and the ROM's own calibration produces the pixel.  This is what makes
    // the silkscreen contrast/brightness controls work (they live in the
    // strip of digitiser that's wider than the LCD, which the synthetic
    // path can't reach), and with the re-fit ADC constants (kTouchAdc*Span /
    // *Base) a tap lands under the pen on both devices.
    //
    // 2026-06-06: the netBook was previously kept on the synthetic-TRawEvent
    // path by default (it injects screen coords 1:1 — perfect calibration but
    // it bypasses the digitiser entirely, so no silkscreen/contrast touch).
    // Validated end-to-end in the harness that the netBook OS boots to the
    // desktop on the faithful path AND that Sketch taps land on the pen
    // across the panel, so the default is unified onto the faithful path.
    // PSION_S7_LEGACY_TOUCH=1 restores the old synthetic path on both devices
    // if a regression turns up.  (PSION_S7_FAITHFUL_TOUCH is now a no-op kept
    // for back-compat with existing scripts.)
    bool s7FaithfulTouch() const {
        static const bool kLegacy =
            std::getenv("PSION_S7_LEGACY_TOUCH") != nullptr;
        return !kLegacy;
    }

    // True only when the netBook should run the faithful Eiger/ADS7843
    // digitiser path.  ALL netBook-specific touch machinery (SPI completion,
    // STATUS6/IRQ_STATUS register reshaping, the sampler timer, the channel
    // reset) is gated on this so that, by default, the netBook behaves exactly
    // as it did before this work (synthetic-TRawEvent touch) and boots
    // identically to main.  Opt in with PSION_S7_FAITHFUL_TOUCH=1.
    bool nbFaithfulTouch() const { return isNetBookRom_ && s7FaithfulTouch(); }

    // ── Clock-INDEPENDENT recording-capture diagnostics ──────────────
    // Used by scripts/test-rec-timer-netbook.mjs to tell REAL audio
    // capture apart from a frozen/fake recorder.  The displayed
    // "Recorded MM:SS" text is a HomeTime delta (wall-clock), so it is
    // NOT a reliable capture signal — it moves whenever the emulated
    // clock moves, capture or not.  These expose the two signals that
    // only move when something real happens:
    //
    //  * debugCodrReads() (above): the codec FIFO drain counter — it
    //    advances ONLY when real audio bytes are popped out of MCDR0.
    //    Real sustained recording => it climbs at ~8000/s.  Frozen/fake
    //    recording => flat.  THIS is the ground-truth capture metric.
    //
    //  * recPosValAddr_ / [recPosValAddr_+0x28]: the MMF clip position
    //    object captured at the recorder redraw 0x505824b0 while
    //    [engine+0x54]==3, and its recorded-SAMPLE-count field — the
    //    value the byte->µs->TTime chain that builds "Recorded MM:SS"
    //    reads.  On main the RATEFIX hook WRITES this field from elapsed
    //    CYCLES (= clock), so it advances even with zero real capture;
    //    comparing it against codrReads exposes that divergence.
    uint32_t debugRecPosValAddr() const { return recPosValAddr_; }
    bool     debugRecActive()     const { return recordingDfcWaiting_; }
    uint32_t debugIcpr()          const { return icpr; }
    uint32_t debugIcmr()          const { return icmr; }
    // Recorded sample count the MMF clip object holds ([posVal+0x28]).
    // This is the "captured-clip data length" proxy (in 8 kHz samples;
    // µ-law => 1 byte/sample, so samples ≈ captured bytes).  Real
    // recording grows it from real popped bytes; on main RATEFIX writes
    // it from the clock, so it can move with codrReads flat.
    uint32_t debugRecClipSamples() {
        if (!recPosValAddr_) return 0;
        return cpu.readVirtualDebug(recPosValAddr_ + 0x28u,
                   ARM710::V32).value_or(0);
    }
    uint32_t debugRecClipState() {
        if (!recPosValAddr_) return 0xffffffffu;
        return cpu.readVirtualDebug(recPosValAddr_ + 0x1cu,
                   ARM710::V32).value_or(0xffffffffu);
    }
    // Push ~0.5 s of 440 Hz square wave into the DAC queue.  Bypasses
    // MCDR0 / kernel entirely — proves the host audio pipeline works
    // independent of whether the netBook OS is actually opening the
    // codec.  Mirrors Windermere::Emulator::debugInjectTestTone.
    void     debugInjectTestTone();
private:
    UartState *resolveUartByIndex(int uartIndex);
    const UartState *resolveUartByIndex(int uartIndex) const;


    // OS timer state. OSCR is a free-running 3.6864 MHz counter on
    // real silicon; osTimerScale() says how many OSCR ticks we hand the
    // guest per real tick (see the virtual for the per-machine rates).
    //
    // CAVEAT (Series 7 / netBook only): dropping those two to the
    // spec-correct 1× rate causes the kernel boot to stall before
    // reaching WSERV / Shell.  Verified locally after the SMULL
    // sign-extension fix in arm710.cpp — even at 60 sim-seconds (15 G
    // CPU cycles) the kernel never advances past K::Init3.  Hypothesis:
    // a kernel time-elapsed check uses signed 64-bit math that
    // previously got "lucky" with the broken SMULL + 10× OSCR
    // combination, and is now waiting for an event that only fires at
    // perceived real-time intervals.  Further investigation needed.
    //
    // PSION_S7_REAL_OSCR=1 selects the spec-correct 1× rate everywhere
    // for diagnosis once we understand the interaction.
    static constexpr int64_t kOsTimerHz = 3'686'400;
    int64_t kOsTimerScale = 10;
    // Set when PSION_S7_REAL_OSCR / PSION_S7_FAST_OSCR pinned the rate
    // from the environment, so configure() leaves kOsTimerScale alone.
    bool     osTimerScalePinned_ = false;
    uint32_t osmr[4] = {0, 0, 0, 0};
    uint32_t ossr = 0;               // match-event status
    uint32_t oier = 0;               // match-IRQ enable
    uint32_t ower = 0;               // watchdog
    uint32_t osCrBase = 0;           // OSCR zero reference in our cycle clock
    int64_t  osCrBaseCycles = 0;
    // Scheduled match: absolute cycle at which OSMR[i] should fire, or
    // -1 if not currently armed. Populated when the guest programs
    // OSMR[i] from an OSCR value; consumed once per programming by
    // the main tick loop. This sidesteps the "OSCR skips values"
    // problem our sparse-sampled counter has.
    int64_t  matchAtCycle[4] = {-1, -1, -1, -1};
    // Series 7 64 Hz synthetic tick — next cycle to fire OSMR1 IRQ.
    int64_t  s7TickNextCycle_ = 0;
    // T11/M11: sentinel set on each synthetic OSMR1 injection. The
    // natural matchAtCycle[1] fire path checks this and disarms without
    // re-firing; the OSSR W1C ack path uses it to ensure ICPR's
    // IRQ_OST1 actually clears even when ossr bit 1 was set by the
    // synthetic injector rather than by an armMatch fire. Prevents a
    // race where the natural and synthetic paths collide and stack
    // ICPR[IRQ_OST1]. Cleared once consumed (one-shot).
    bool     s7SyntheticOsmr1Active_ = false;
    // Set when a synth-event forced reschedule had to be deferred because
    // the kernel was inside a critical section (kernCSLock != 0).  The main
    // loop re-drives the reschedule once the section is released, so the
    // queued event is delivered without preempting mid-critical-section.
    bool     s7ForcedReschedPending_ = false;
    // Cycle by which a deferred forced reschedule must be honoured even if
    // kernCSLock is still held — a real critical section lasts microseconds,
    // so anything still "locked" past this deadline is a genuine kernCSLock
    // LEAK and we force-unwedge it (the recovery the old csLock-leak patch
    // did eagerly, now done lazily and only when actually needed).
    int64_t  s7ForcedReschedDeadline_ = 0;
    // Cycle through which the audio codec counts as "active" for the synthetic-
    // event force-unwedge gate.  Bumped (passedCycles + ~1 s margin) whenever the
    // codec is enabled for capture/playback.  While active, s7InjectRawEvent must
    // NOT force-clear kernCSLock to drive a reschedule: the record/play teardown
    // runs delicate non-reentrant kernel code, and forcing a reschedule into it
    // corrupts kernel state (the intermittent worker-mode recording crash —
    // 0xA5 use-after-free → DACR=0 fault loop).  During audio we deliver synth
    // events only via the safe natural path (kernCSLock cleared by the kernel
    // itself); the ~1 s margin covers the teardown window after MCE 1->0.
    int64_t  s7CodecActiveUntil_ = 0;
    // Series 7 OSMR1 auto-rearm (pragmatic replacement for the K1 synthetic
    // tick).  Tracks the last cycle the kernel WROTE OSMR1 (= natural arm)
    // and the last cycle the kernel ACKed OSSR bit 1 (= OSMR1 IRQ
    // delivered).  When the kernel acks but doesn't re-arm within ~1 tick
    // interval (i.e. the natural re-arm chain has stalled) we auto-arm
    // OSMR1 on its behalf to the next 64 Hz tick, behaving like a hardware
    // auto-reload counter.  This lets matchAtCycle[1] fire the IRQ
    // normally instead of being bypassed by IRQ injection.
    int64_t  s7Osmr1LastWriteCycle_ = -1;
    int64_t  s7Osmr1LastAckCycle_   = -1;
    uint32_t s7Osmr1AutoArmCount_   = 0;
    // High-res (OSMR2) timer-queue kick after a faithful pen-down.
    // The digitiser's sample timer lives in the kernel's high-res
    // OSMR2 timer queue.  On a fresh tap (after an idle period) the
    // kernel queues the sample timer but never kicks OSMR2 to service
    // it — the deferred-arm that FUN_50006278 normally performs on
    // preemption-unlock doesn't run, so OSMR2 stays disarmed and
    // sampling never starts (touch dead on the 2nd+ tap).  For a short
    // window around each faithful pen-down (whole pen-down + a short
    // post-release tail) we reproduce that kick at the hardware level:
    // once per 64 Hz tick, if OSMR2 is disarmed or armed further out than
    // any plausible sample interval (~100 ms), force an OSMR2 match this
    // cycle.  That runs the kernel's high-res timer DFC, which fires the
    // due sample timer and re-arms OSMR2 correctly.  Self-limiting: once
    // real sampling resumes (OSMR2 armed ~1 tick out, well under the
    // threshold) the kick becomes a no-op.  Disable with
    // PSION_S7_NO_OSMR2_KICK=1.
    int      s7Osmr2KickTicksLeft_  = 0;
    uint32_t s7Osmr2KickCount_      = 0;
    // Periodic auto-arm: when OSMR1 fires naturally OR is acked by the
    // kernel via OSSR bit 1, immediately schedule the next match at the
    // next 64 Hz boundary.  Models OSMR1 as a hardware-style free-running
    // periodic timer so the NTimerQ tick stays at a steady 64 Hz even
    // when the kernel's natural re-arm chain produces jitter.  Counted
    // separately from s7Osmr1AutoArmCount_ (the fallback stall detector).
    // Disable with PSION_S7_NO_OSMR1_PERIODIC=1.
    uint32_t s7Osmr1PeriodicArmCount_ = 0;
    // Counter for the User::After hijack — hijack only the first
    // N calls during boot init, then let later calls wait normally
    // so input drivers can sleep/wake correctly.
    uint32_t s7AfterHijackCount_ = 0;
    bool     s7EikonForceLoadAttempted_ = false;
    bool     s7VideoForceLoadAttempted_ = false;
    // PSION_NB_FORCE_OS_LOAD=1 — one-shot, fires once during boot.
    // Diagnostic hook for the netBook bootloader CF investigation.
    // The bootloader has an OS-image-loader entry point at ROM offset
    // 0xc6064 (virt 0x500c6064) that walks a fallback table and
    // ultimately calls the function at 0xc6db0 which opens D:\OS.IMG,
    // reads it into DRAM, and chains to the loaded image.  In v0.11
    // builds nothing in the bootloader's idle-loop flow seems to reach
    // this entry — the bootloader's IrqExtMedChgCf enable bit is
    // explicitly cleared during init, so MedChgCf alone doesn't drive
    // the path.  Setting this env var force-invokes the entry so we
    // can determine whether the rest of the CF/FS/OS-load stack works
    // when reached, decoupling the "right trigger" question from the
    // "is the data path correct" question.
    bool     nbCfLoadForceAttempted_ = false;
    // Faithful CF boot completion.  In PSION_NB_NATIVE_CF mode the
    // bootloader reads D:\OS.IMG off the CompactFlash card through its
    // own medata/epbus driver (the "Loading from CF card..." progress
    // bar), then parks at its post-read restart, which depends on
    // RAM-resident bootloader state the emulator can't drive (see
    // docs/netbook-cf-bootloader-faithful.md).  Once the read has gone
    // quiet — essentially the whole OS.IMG drained and no fresh ATA
    // command for ~0.5 sim-sec — we complete the boot the way the
    // bootloader's own restart would, handing control to the OS image
    // that was just read from the card.  Tracked here so the per-tick
    // check is cheap and fires exactly once.
    bool     nbFaithfulBootFired_ = false;
    uint32_t nbFaithfulLastAtaCount_ = 0;
    int64_t  nbFaithfulLastAtaCycle_ = 0;
    // PSION_NB_TRACE_LOADER diagnostic: pin the loader's post-read park.
    int64_t  nbLoaderLastCallCyc_ = 0;
    bool     nbLoaderParkDumped_  = false;
    bool     nbMedChgRefire_ = false;
    bool     nbIrqSpFixed_ = false;
    bool     nbBootMainUnblocked_ = false;
    bool     nbHandler32Logged_ = false;
    bool     nbEigerInitForced_ = false;
    bool     nbByteTableFixed_ = false;
    bool     eigerChunkMapped_ = false;
    bool     osLoaderReachedFlag_ = false;
    int      nbPostAckTraceLeft_ = 0;
    bool     nbPccardInstallCalled_ = false;  // Option-C Install attempt
    bool     eigerTableDumped_ = false;
    bool     eigerLateDumped_ = false;
    int64_t  eigerChunkMappedCycle_ = 0;
    int64_t  osHandoffCycle_ = 0;
    bool     nbOsCfDispatchInstalled_ = false;
    bool     nbReschedPokeArmed_ = false;
    int64_t  nbReschedPokeNextCycle_ = 0;
    bool     nbPccardForceAttempted_ = false;
    // One-shot for PSION_NB_FEED_UART_HEX.
    bool     nbUartFeedAttempted_ = false;
    // Counter for PSION_NB_WFI_TRACE entries.
    uint32_t nbWfiTraceCount_ = 0;
    // Counter for the WFI-wake fixup (capped at 1000).
    uint32_t nbWfiWakeCount_ = 0;
    // Counter for PSION_NB_FLUSH_TRACE entries.
    uint32_t nbFlushTraceCount_ = 0;
    // Counter for PSION_NB_FLUSH_SKIP applications.
    uint32_t nbFlushSkipCount_ = 0;
    // Counter for PSION_NB_FLUSH_FIX applications.
    uint32_t nbFlushFixCount_ = 0;
    // Counter for PSION_NB_FN80D8_TRACE entries.
    uint32_t nb80d8TraceCount_ = 0;
    // netBook Eiger sleep mode: set when the kernel writes 1 to
    // ASIC[0xa000] at 0x50006990, cleared on the next IRQ arrival
    // (after which we redirect PC to the wake handler at 0x50006998).
    // While set, the CPU is not ticked — preserving the exact
    // register / stack / save-area state the wake handler expects.
    bool     nbSleepHalted_ = false;
    // Cycle at which the sleep request was issued — for logging only.
    int64_t  nbSleepHaltCycle_ = 0;
    // Count of wake events delivered via the proper sleep model.
    uint32_t nbSleepWakeCount_ = 0;
    // PC to redirect to on wake.  Always busy-wait+4, captured at
    // halt entry so we don't have to re-detect which build we're in
    // (BL v0.11 = 0x50006998, netBook OS v1.05(450) = 0x500068fc).
    uint32_t nbSleepWakePC_ = 0x50006998;
    // (Removed: s7TickPending_ / s7DriveSchedulerOnce_ — used by an
    //  earlier deferred-scheduler driver experiment that didn't pan
    //  out.  See commit log for details.)
    // ── SA-1100 SSP controller (drives UCB1200) ─────────────────────
    //
    // The SSP at 0x80070000 is wired to the UCB1200 codec.  Real
    // silicon executes a 16-bit synchronous transfer: kernel writes
    // a cmd word to SSDR, the SSP shifts it out while shifting the
    // codec's response in, sets RNE/RFS in SSSR, and raises the SSP
    // IRQ (bit 20 in ICPR) when RIE is enabled in SSCR1.  The kernel
    // ISR reads SSDR to retrieve the response and the IRQ clears.
    //
    // We model this with no FIFO and no transfer latency: an SSDR
    // write immediately produces the UCB1200 response and queues it
    // for the next read; if RIE is set, IRQ_SSP fires straight away.
    // That's enough fidelity for the Series 7 touch driver's
    // IRQ-driven sample state machine (the agent investigation
    // identified the missing SSP IRQ as the blocker that wedges
    // touch_drv[+0x24] at terminal idle without delivering events).
    uint16_t sscr0_      = 0;          // control reg 0 (SSE, DSS, SCR)
    uint16_t sscr1_      = 0;          // control reg 1 (RIE, TIE, …)
    uint16_t sspRxData_  = 0;          // queued codec response
    bool     sspRxValid_ = false;      // RX FIFO has a pending response
    bool     sspRor_     = false;      // ROR latch (W1C via SSSR)
    uint32_t readSsp(uint32_t off, ValueSize vs);
    void     writeSsp(uint32_t off, uint32_t value, ValueSize vs);
    void     recomputeSspIrq();
    // (H24: keyState_ removed — was written but never read.  Keyboard
    //  state lives in kbdMatrix_ below + the IRQ status bits in
    //  asicRegs[0x12-0x13].)

    // ── UCB1200 codec model ─────────────────────────────────────────
    //
    // The UCB1200 is the Philips touch / audio / GPIO codec wired to the
    // SA-1100 SSP on Series 7 / netBook.  Three sites in the emulator
    // need to answer "given this 16-bit cmd word, what 16-bit response
    // would the codec produce?":
    //
    //   (a) SSP SSDR read path (readPeripheral case 0x80070000) — the
    //       kernel pushes a cmd word, then reads SSDR for the response.
    //   (b) Eiger ASIC ADC-cmd path (readAsic 0x26/0x27) — the BSP
    //       writes the cmd halfword into the Eiger DMA shadow then
    //       reads it back as the response.
    //   (c) updateTouchInput — primes ASIC[0x2a-0x2d] from touchX/Y.
    //
    // Consolidating these into one helper avoids the three copies
    // drifting (H28 / M28).  The helper also models the UCB1200's
    // 10-pin GPIO block (H27): cmds in the 0x60xx / 0x70xx range read
    // / write GPIO_DATA / GPIO_DIR.  Default state on power-up: all
    // pins are outputs, output value all-1 (backlight on, kbd IRQ
    // enabled, mic on — the lines the Series 7 BSP toggles via the
    // codec GPIOs).  Real hardware powers up with pins as inputs but
    // the BSP reprograms them before anything depends on the reset
    // state, and "all-1 output" gives the visibly-correct boot state
    // (backlight lit) without any extra writes.
    uint16_t ucbGpioData_ = 0x03FFu;  // 10-bit output latch
    uint16_t ucbGpioDir_  = 0x03FFu;  // 1 = output (all pins out)

    // UCB1200 audio + telecom control register shadow.  Per the Philips
    // datasheet, registers 0x08/0x09 (audio control 0/1) and 0x0a/0x0b
    // (telecom control 0/1) hold codec sample-rate divisors, enable
    // bits and gain / mute fields.  The Series 7 / netBook EPOC R5 BSP
    // does write-then-read-back verification when bringing the codec
    // up: if the read back doesn't match what it wrote, it abandons
    // codec init and the device stays silent.  Storing the last-written
    // value and replaying it on read is enough to clear that gate.  Cmd
    // encoding mirrors the existing GPIO block (0x60xx/0x70xx): the top
    // 5 bits encode {R/W, reg-addr}; the low 11 are data.
    uint16_t ucbAudioCtrl0_   = 0;
    uint16_t ucbAudioCtrl1_   = 0;
    uint16_t ucbTelecomCtrl0_ = 0;
    uint16_t ucbTelecomCtrl1_ = 0;
    // UCB1200 ADC-ready GPIO6 interrupt scheduling.  When the codec
    // is configured for recording, fire GPIO6 at 8 kHz to drive the
    // recording DFC's sample-read loop.
    int64_t  ucbAdcNextIrqAt_ = 0;

    // UCB1200 audio codec model.  Receives DAC samples written by the
    // kernel to MCDR0 and produces ADC samples read back from MCDR0
    // for the microphone path.  See clps7111.cpp for the pattern.
    AudioCodecModel audio_;

    // SA-1100 MCP (Multimedia Communications Port) Control Register 0
    // shadow.  Per the Intel SA-1100 manual §11.7, MCCR0 carries the
    // audio + telecom sample-rate divisors (low 13 bits), the codec
    // attached flag (bit 16, MCE) and IRQ enables (bits 17..21).  The
    // netBook OS programs MCE=1 to bring the codec up; with the old
    // "drop writes / read 0" behaviour the kernel saw MCCR0=0 on read-
    // back, decided the MCP didn't latch and bailed.  We now persist
    // MCCR0 and gate MCP-audio IRQs on MCE so a torn-down codec stays
    // silent.
    uint32_t mccr0_ = 0;
    // Diagnostic latch: set when MCCR0 is written, cleared on the
    // first MCDR0 write afterwards.  Drives a one-shot log so we
    // can confirm in the browser console whether the codec is
    // actually being driven (without flooding the trace).
    bool     firstMcdr0WriteSinceMccr0_ = false;
    // One-shot latches for PC-range tracing of audio driver code.
    // Set the first time the kernel executes inside Esdrv.pdd /
    // ESound.ldd address ranges in ROM — confirms whether the
    // drivers actually load and run when the OS boots / an app
    // tries to open audio.
    bool     esdrvPcSeen_      = false;
    // Set when the CPU enters Esdrv.pdd's recording DFC poll loop
    // (PC=0x50298700, body offset +0x14f0).  Gates the 8 kHz Eiger
    // slot 4 interrupt — must NOT fire during boot or the system
    // starves at the splash screen.
    bool     recordingDfcWaiting_ = false;
    uint32_t recordChannelPtr_ = 0;
    uint32_t recBufPos_ = 0;
    uint32_t recBufFlips_ = 0;
    bool     recEsdrvStateSet_ = false;
    uint32_t recEsdrvChannel_ = 0;
    uint32_t recTickCounter_ = 0;
    uint32_t sacLastRxSample_ = 0x2000;
    bool     sacL2Redirected_ = false;
    int      sacr1ResetPolls_ = 0;
    bool     recNtimerFixed_ = false;
    bool     recOpenPatched_ = false;
    uint32_t recOwnerPA24_ = 0;
    uint32_t recNtimerHandle_ = 0;
    uint32_t recOwner_ = 0;
    // CP15 TTB captured from the Sound app's process the first time it sets
    // own+0x1c = 0x00407248. Used to switch MMU context around the recording
    // DFC delivery. 0 = not yet captured.
    uint32_t recAppTtb_ = 0;
    // NThread* of the Sound app's main thread (the one waiting in
    // WaitForAnyRequest for the buffer-full TRequestStatus completion).
    // Captured from *0x80000900 (iCurrentThread) at recording start. Used
    // as the first arg to Kern::RequestComplete to wake the right thread.
    uint32_t recAppNT_ = 0;
    // ── netBook recording-timer fix (EDGE + DMAENG/DMAIRQ + RATEFIX) ──────
    // Default-ON on the netBook ROM (gated on isNetBookRom_); disabled by
    // PSION_REC_OFF=1.  Individual stages can be forced via PSION_REC_EDGE /
    // _DMAENG / _DMAIRQ / _RATEFIX.  See sa1100.cpp for the per-stage logic.
    bool     recEdge_           = false;  // SAC RX edge-driven IRQ_SSP
    bool     recDmaEng_         = false;  // model SA-1111 SAC RX DMA engine
    bool     recDmaIrq_         = false;  // drive the PSL deliver DFC (no stray)
    bool     recRateFix_        = false;  // real-time recorded-sample correction
    bool     recFaithful_       = false;  // kernel-ISR capture (no synthetic drains); default on netBook
    bool     recPlayTx_         = false;  // EXPERIMENTAL playback TX-service (PSION_PLAY_TX)
    bool     recDeliver_        = false;  // EXPERIMENTAL: complete the app's record read (PSION_REC_DELIVER)
    int64_t  recDeliverLastCyc_ = 0;      // cycle of the last forced record-read completion
    uint64_t recDeliverCount_   = 0;      // forced completions issued
    bool     recStoppedOnce_    = false;  // a real foreground record->STOP happened (gates playback-active)
    bool     playActive_        = false;  // faithful playback: codec TX in progress
    int64_t  playLastDacCyc_    = 0;      // cycle of the last MCDR0 DAC write (playback liveness)
    int64_t  playLastIrqCyc_    = 0;      // cycle of the last injected TX-service IRQ_SSP
    bool     recNetBookDefaultApplied_ = false; // one-shot device-gated default-on
    // ── netBook playback bridge ───────────────────────────────────────
    // The EPOC recorder's playback engine arms its client/format objects
    // (owner+0x28 state == 3) but, in this emulated configuration, never
    // drives the SA-1111 SAC TX hardware (no SADR writes, no DMA, no
    // TX-service IRQ_SSP — verified exhaustively).  Recording, by
    // contrast, streams real mic PCM through the codec RX path.  To make
    // playback audible we capture the recorded PCM here and replay it
    // through the real codec DAC output when the recorder enters its play
    // state — bridging only the EPOC-internal clip→driver handoff that
    // this image does not exercise under emulation.  Default-on; the kill
    // switch is PSION_NB_NO_PLAY_BRIDGE.
    std::vector<int8_t> playbackClip_;        // last recording's 8-bit PCM
    bool     recWasCapturing_   = false;      // recordingDfcWaiting_ edge tracker
    bool     playBridgeActive_  = false;      // currently streaming clip to DAC
    size_t   playBridgePos_     = 0;          // next clip sample to emit
    int64_t  playBridgeNextAt_  = 0;          // cycle of the next DAC push
    uint32_t playBridgeLastState_ = 0;        // last observed owner+0x28
    // ── netBook faithful playback (SAC TX → host DAC) ─────────────────
    // The ESound LDD play request (reqId=0 DoRequest with channel-state
    // [LDD+0xcc]==0) is the kernel-observable "PLAY tapped" signal post-
    // STOP (a one-shot burst at play start — the app then idles, its
    // play-open completion never firing on this image).  When it fires we
    // clock the WHOLE recorded clip out through the REAL SAC TX path —
    // feeding pushDacSample (which both routes to the host DAC and fills
    // the virtual TX FIFO that SASR.TBS/TNF report) at the 8 kHz codec
    // rate.  The SAC SADR-write routing + SASR TX status mirror the
    // Series 7 SAC TX model so any kernel-driven SADR write also hits the
    // DAC, but on this image the kernel never writes SADR during play
    // (verified: 0 SADR writes the whole session), so the clip clocking
    // is emulator-driven through the genuine SAC TX FIFO + codec pipeline.
    bool     nbPlayActive_      = false;      // netBook play in progress
    uint32_t nbPlayOwnLast_     = 0;          // last observed recOwner_+0x28 (play-arm rising edge)
    size_t   nbPlayPos_         = 0;          // next clip sample to clock out
    int64_t  nbPlayNextAt_      = 0;          // cycle of the next TX sample
    int64_t  nbPlayLastKernelTxCyc_ = 0;      // cycle of the last kernel SADR write during play
                                              // (audio-tick takes over the clip once the
                                              //  kernel's TX-fill loop stalls after its 1st buffer)
    // Helper: netBook is actively clocking the clip through the SAC TX
    // (legacy emulator-driven hack — see nbPlayActive_).
    bool nbSacAudioTx() const {
        return isNetBookRom_ && nbPlayActive_ && !nbPlayNoTx_;
    }
    // Faithful netBook playback: the SA-1111 SAC is configured for TX (play),
    // by register state alone — the analogue of s7SspAudioTx().  The netBook
    // kernel programs SACR0 (asicRegs[0x7060]) and SACR1 (asicRegs[0x7064])
    // through its SAC config manager (pc 0x5002918c); the steady-state values
    // distinguish record from play:
    //   record:  SACR0=0x1d (ENB), SACR1=0x08 (RX-service int enabled)
    //   play:    SACR0=0x1d (ENB), SACR1=0x00 (RX-service int clear)
    // So TX/play = SAC enabled (SACR0 bit0) AND the record RX-int (SACR1 bit3)
    // clear AND a live host speaker.  Gating the SADR->DAC route on this (not
    // the PC-hook nbPlayActive_) lets the kernel's OWN TX-fill loop drive the
    // codec — fully kernel-driven, like the Series 7.
    bool nbSacTxConfigured() const {
        if (!isNetBookRom_ || !audio_.hostSpeakerEnabled()) return false;
        return nbSacTxMode();
    }
    // SAC being driven for PLAY, by DRIVER STATE — NOT by SAC registers.
    // We cannot distinguish play from record on this OS image from SACR0/SACR1:
    // the record driver clears SACR1 bit3 transiently mid-capture, and the
    // kernel's TX-fill loop oscillates it during the bounds-check spin, so a
    // register test mis-fires in both directions (it hijacks the record SASR
    // and it disengages the play throttle exactly when needed).  The robust
    // signal is the recorder lifecycle: a real foreground record->STOP has
    // happened (recStoppedOnce_, set by the STOP-stall detector and reset when
    // a fresh record arms) and we are not currently capturing
    // (recordingDfcWaiting_).  Under that condition the SAC is being driven for
    // playback, so we report a FAITHFUL SAC TX status (SASR TNF/TBS from the
    // genuine virtual TX FIFO level, like the Series 7's readSsp) and route the
    // kernel's SADR writes through that FIFO.  The TX-fill loop then throttles
    // on the FIFO exactly like real silicon — including play-from-the-clip-end
    // with no data — instead of spinning the kernel into the array-bounds-check
    // freeze.  The record path (and its duty-cycle SASR) is left untouched.
    bool nbSacTxMode() const {
        return isNetBookRom_ && recStoppedOnce_ && !recordingDfcWaiting_;
    }
    bool     nbPlayNoTx_        = false;      // PSION_NB_NO_PLAY_TX kill switch (resolved once)
    bool     nbPlayResolved_    = false;
    // Experimental fully-kernel-driven netBook playback (opt-in:
    // PSION_NB_PLAY_FAITHFUL).  The netBook kernel's SAC TX-fill loop DOES run
    // and write SADR during play (lr=0x50297aec) — overturning the old "0 SADR
    // writes" note (that was a trace-dedup artifact).  But on this OS image the
    // play path has two gaps that make it unusable as-is: (a) the TX source
    // cursor [chan+0x60] is left bound to the record scratch [chan+0x54]
    // (StartRecord at 0x502980fc), so it streams leftover µ-law silence — only
    // ~6 distinct SADR values, not the clip — because the clip→TX deliver
    // (vtable[0x38]=0x50295774) never runs (the play-buffer gate 0x50298578
    // returns 0); and (b) the TX-fill is OSMR-rate-limited to ~2200 writes/s,
    // far below the 8 kHz codec rate.  So the kernel-driven route plays slow
    // scratch.  Default is therefore the emulator-clocked path below, which
    // clocks the REAL recorded clip through the same SAC TX FIFO + codec DAC at
    // a true 8 kHz — higher-quality and faithful in the data path; only the
    // clock trigger is synthesized (the kernel's own play-arm is gap-blocked).
    bool     nbPlayFaithful_    = false;
    // EDGE: codec-drain baseline + handler-install settle window.
    uint64_t recEdgeBaseline_   = 0;
    bool     recEdgeBaselineSet_ = false;
    int64_t  recEdgeSettleAt_   = -1;
    // SAC RX-IRQ pacing: cycle of the last injected IRQ_SSP assertion.  We
    // assert at most once per FIFO-depth refill interval (~2 ms) so the PSL
    // has an inter-IRQ window to run its no-request → MASK path at STOP.
    int64_t  recLastEdgeAssertCyc_ = 0;
    // STOP detection via real-drain stall.  codrReads climbs continuously
    // while the recorder is actually capturing; at STOP the PSL drops into a
    // teardown busy-wait that does NOT read SADR, so codrReads freezes.  If it
    // stalls for ~33 ms after recording was established, the recorder stopped:
    // disarm + clear the injected IRQ so the teardown completes (no freeze).
    uint64_t recLastCodrSeen_   = 0;
    int64_t  recLastCodrAdvCyc_ = 0;
    // After a stop is detected, suppress re-arming the recording detection for
    // a short cooldown — the just-stopped recorder still shows clip state==3
    // (it's mid-teardown), so without this the redraw-detection re-arms
    // immediately and the injected IRQ resumes (the freeze never clears).  The
    // cooldown lets the PSL teardown finish; normal detection resumes after.
    int64_t  recStopCooldownUntil_ = 0;
    // DMAENG/DMAIRQ: captured DMA + SAC PSL channel objects, per-buffer
    // cadence baseline, completion counters, and the host-side codec sample
    // ring fed into the kernel DMA source ring (otherwise silence).
    uint32_t recDmaEngChan_     = 0;
    uint64_t recDmaEngBaseline_ = 0;
    uint64_t recDmaEngBuffers_  = 0;
    uint64_t recDmaIrqBuffers_  = 0;
    std::vector<uint8_t> recSrcFeed_;
    // RATEFIX: cached clip position object + record-arm cycle baseline.
    uint32_t recPosValAddr_     = 0;
    uint64_t recRateStartCyc_   = 0;
    int      recStopCountdown_ = 0;
    int      sacStatusReads_ = 0;
    int      sac7060Reads_ = 0;
    int      sacBatchReads_ = 0;
    // Diagnostics for the recording drain bottleneck (harness Stage D).
    uint64_t dbgSadrReads_ = 0;   // 0x706C read attempts (pop attempts)
    uint64_t dbgSacsrReads_ = 0;  // 0x7074 status reads (handler poll cadence)
    uint64_t dbgS7SsdrTotal_ = 0;  // Series 7 SSDR reads in audio-RX (valid+silent)
    uint64_t dbgS7SsdrSilent_ = 0; // ...of which the ring was empty (silence)
    uint64_t dbgIrqSspFires_ = 0; // EDGE IRQ_SSP assertions
    uint64_t dbgDeliverHookHits_ = 0; // times pc==0x502977f0 (deliver entry) was hit
    uint64_t dbgInjAttempt_ = 0, dbgInjBlkMask_ = 0, dbgInjBlkPend_ = 0, dbgInjEval_ = 0, dbgAudioTicks_ = 0;
    // SAC status register TBS (Transmit Buffer Status) duty cycle.
    // The Esdrv recording driver at PC 0x50298290 spin-polls this bit:
    // reads SACSR (0x7074), tests bit 1 (TBS), writes a sample to
    // SADR (0x706C) when TBS=1.  In our emulation TBS was unconditionally
    // 1, so the driver never exited the spin and starved WSrv (= screen
    // freeze + timer stuck at 0:00).  Cycle TBS off periodically so the
    // driver sees "TX FIFO full" and yields.
    int      sacTbsCounter_ = 0;
    uint32_t recSavedEf4_ = 0;
    uint32_t recSavedEf4ptr_ = 0;
    bool     esoundPcSeen_     = false;
    bool     prevTickInEsdrv_  = false;
    bool     prevTickInEsound_ = false;
    // Returns true when the kernel has enabled the codec (MCCR0 bit 16,
    // MCE).  Used to gate IRQ_MCP_AUDIO firing in the tick loop and the
    // mic-edge path in writeAudioInput.
    bool     mcpAudioEnabled() const {
        // The netBook BSP configures the codec via ASIC UCB1200
        // commands, NOT by writing MCCR0.  So MCCR0.MCE stays 0 even
        // when the codec is fully active.  Check EITHER path:
        //   - MCCR0.MCE (traditional SA-1100 MCP path)
        //   - ucbAudioCtrl0_ non-zero (ASIC UCB1200 path — means
        //     the BSP wrote Audio Control 0 with rate/enable bits)
        return (mccr0_ & (1u << 16)) != 0 || ucbAudioCtrl0_ != 0;
    }
    // Series 7 SSP-streamed microphone recording.  On the Series 7 the
    // UCB1200 codec's ADC samples are streamed into the SA-1100 SSP RX
    // FIFO (NOT MCDR0 — the ROM never reads MCDR0).  The Sound PDD
    // configures the SSP for 14-bit audio framing (SSCR0 DSS=0xD, i.e.
    // low nibble 0xD) with SSE + RIE set, then drains SSDR on IRQ_SSP.
    // The companion touch driver instead drives the UCB1200 over the
    // Eiger ASIC command port, so the SSP is effectively audio-only;
    // gate mic streaming on the 14-bit audio framing + a live host mic
    // so a touch-mode SSP transfer (if any) is never fed mic samples.
    // "Real Series 7" audio gate.  isSeries7Rom_ is set for the whole
    // SA-1100 family INCLUDING the netBook (isNetBookRom_ then also set), so
    // the Series-7 SAC/SSP audio routing must additionally exclude the netBook
    // — the netBook drives its SA-1111 SAC through the original SAC handlers,
    // and routing its 0x7060-0x7077 registers to the SSP model breaks it.
    bool isS7Audio() const { return isSeries7Rom_ && !isNetBookRom_; }
    bool s7SspAudioRx() const {
        return isS7Audio() && audio_.hostMicEnabled()
            && (sscr0_ & 0x80u)            // SSE — SSP enabled
            && (sscr1_ & 0x01u)            // RIE — RX IRQ enabled
            && (sscr0_ & 0x0Fu) == 0x0Du;  // DSS=14-bit → audio framing
    }
    // Series 7 SAC playback (TX).  PlayConfig enables the same 14-bit SAC
    // framing (SSCR0 SSE + DSS=0xD) but leaves RIE (SSCR1 bit 0) CLEAR — the
    // record path sets RIE, the play path does not.  The play TX-fill loop
    // writes SADR and tests SASR bit 1 (TBS, TX FIFO service).  Gate on a live
    // host speaker so an idle configured channel isn't treated as playing.
    bool s7SspAudioTx() const {
        return isS7Audio() && audio_.hostSpeakerEnabled()
            && (sscr0_ & 0x80u)            // SSE — SSP enabled
            && !(sscr1_ & 0x01u)           // RIE clear — not the RX/record path
            && (sscr0_ & 0x0Fu) == 0x0Du;  // DSS=14-bit → audio framing
    }
    // Re-evaluate IRQ_MCP_AUDIO from current codec + MCCR0 state.
    // Mirrors recomputeSspIrq's level-sensitive pattern: asserts the
    // IRQ when MCE=1 AND (TX FIFO needs samples OR RX FIFO has
    // samples); clears otherwise.  Call after any state change that
    // affects either FIFO (MCDR0 read/write, MCCR0 write, mic
    // enqueue, per-tick drain).  Edge-fire-and-leave-stuck IRQs were
    // causing the OS to spin in the audio ISR after MCE was set.
    void     recomputeMcpIrq();

    // Deferred-EKeyUp queue.  On the netBook OS, a key press shorter
    // than ~50 ms in real time causes WServ's CKeyRepeat (After(500
    // ms) timer) to fire spuriously — single right-arrow press scrolls
    // through multiple menu items.  Theory: WServ doesn't fully
    // process the EKeyDown event before EKeyUp arrives, so when it
    // does process them back-to-back, the auto-repeat timer machinery
    // gets confused.  Workaround: defer EKeyUp injection so it lands
    // at least kDeferKeyUpCycles after the matching EKeyDown.  The
    // emulator runs normally between the two, giving WServ time to
    // process Down + schedule + go-to-wait before Up arrives.
    struct DeferredKeyUp { int key; int64_t deliverAtCycle; };
    std::vector<DeferredKeyUp> deferredKeyUps_;
    int64_t lastKeyDownCycle_ = 0;
    static constexpr int64_t kMinKeyHoldCycles =
        (int64_t)CLOCK_SPEED * 100 / 1000;  // 100 ms sim time

    // Pending deferred EButton1Up event (same rationale as DeferredKeyUp:
    // very quick taps cause the OS to see Down + Up in rapid succession,
    // which can cause widgets to open-then-close).  Only one tap can be
    // in flight at a time (single-touch).
    struct DeferredPointerUp { int32_t x; int32_t y; int64_t deliverAtCycle; bool pending; };
    DeferredPointerUp deferredPointerUp_ = {0, 0, 0, false};
    int64_t lastTouchDownCycle_ = 0;

    // Pending deferred EButton1Down (+ its priming EPointerMove).  Used
    // when a new tap arrives while the PREVIOUS tap's Up is still
    // deferred (a fast re-tap on the same spot — Bombs' double-tap-to-
    // expose).  We must deliver the pending Up first, but injecting the
    // new Down back-to-back with that Up makes WServ drop the Down (it
    // hasn't re-subscribed via UserSvr::RequestEvent yet — the very race
    // the Up-deferral above exists to avoid).  So we defer the new Down
    // by the same hold so WServ processes the Up in between; OPL apps
    // that block in GETEVENT waiting for the second tap then see a clean
    // Down/Up pair instead of wedging.  Single-touch: only one in flight.
    struct DeferredPointerDown { int32_t x; int32_t y; int64_t deliverAtCycle; bool pending; };
    DeferredPointerDown deferredPointerDown_ = {0, 0, 0, false};

    // Touch ADC scaling.  The faithful Series 7 touch path feeds these ADC
    // counts to the ROM's own digitiser calibration (FUN_5008b19c +
    // FUN_50088fec/9008 offsets), which then maps ADC → screen pixel.  The
    // composite (frontend pixel → ADC → ROM → drawn pixel) is linear per
    // axis, so the span (gain) + base (offset) of each axis are fit so that
    // a tap lands exactly under the pen.
    //
    // 2026-06-06: re-fit against Sketch (open Sketch, tap known points, read
    // back where the ink lands).  The previous single shared span (3703) +
    // bases (X=50, Y=197) left the faithful path slightly out: ink landed
    // up to ~20px LEFT of the pen (worse toward the right edge) and up to
    // ~19px BELOW the pen (worse toward the foot of the screen).  X needs a
    // touch MORE gain, Y a touch LESS, so the spans are now per-axis rather
    // than one shared constant.  Measured residual after the re-fit is within
    // a pixel or two across the whole panel.  (The netBook's synthetic-
    // TRawEvent path injects screen coords 1:1 and never exercises this — see
    // s7FaithfulTouch / updateTouchInput.)
    static constexpr double kTouchAdcXSpan = 3790.0;  // was the shared 3703
    static constexpr double kTouchAdcYSpan = 3527.0;  // was the shared 3703
    static constexpr int    kTouchAdcXBase = 82;       // was 50
    // Y digitiser ADC increases DOWN the screen (matches the Eiger panel +
    // the ROM calibration FUN_5008b19c, which the faithful touch path feeds):
    //   yAdc = kTouchAdcYBase + touchY * (kTouchAdcYSpan / digitiserHeight)
    static constexpr int    kTouchAdcYBase = 212;      // was 197
    // The Psion Series 7 / netBook digitiser is WIDER than the LCD —
    // it covers a 783-pixel-wide active area while the LCD itself is
    // only 640 pixels.  The extra ~143 pixels live as silkscreen-only
    // touch zones on either side of the LCD (printed icons for
    // brightness, contrast, on/off, etc).  The kernel maps incoming
    // ADC readings through this 783-pixel range, so when the user
    // touches the LCD at LCD-X=0 the ADC must read as if it were at
    // digitiser-X=~72 (the LCD's inset from the digitiser's left
    // edge), NOT at digitiser-X=0 (which is the leftmost silkscreen
    // icon).  Without the centering inset every LCD touch reads as
    // if it landed on the wrong silkscreen icon and the OS routes it
    // to silkscreen handlers instead of foreground apps.
    static constexpr int    kSeries7DigitiserWidth  = 783;
    static constexpr int    kSeries7DigitiserHeight = 480;
    // Pen-up sentinel values used by the UCB1200 / Eiger ADC shadow
    // registers.  The high-byte sentinel (0x82) is what real hardware
    // returns on pen-up; the BSP's PenDown handler bit-15 tests the
    // X word, so the actual low byte doesn't matter — but keeping the
    // real-hardware value avoids surprising third-party drivers that
    // happen to compare the whole halfword.
    static constexpr uint16_t kTouchPenUpX = 0x8000u;
    static constexpr uint16_t kTouchPenUpY = 0x0082u;

    // Battery ADC value reported by the UCB1200's ADCMux MAIN/BACKUP
    // channels.  Series 7 / netBook ship a rechargeable Li-ion at
    // ~4.1 V nominal; that maps via the BSP's ~6 V divider to a 12-bit
    // ADC reading of ~4000 (97.7% of full scale).  We pin this so the
    // System file browser always renders a "full / charging" icon —
    // the alternative would model AC-adapter presence and real
    // charge-level, which isn't worth the complexity in an emulator
    // where the device is conceptually always plugged in.
    static constexpr uint16_t kBatteryFullAdc = 4000;

    // ── netpad board-codec ADC levels ────────────────────────────────
    // The netpad's touch panel is read differentially, so the X/Y counts
    // are a straight linear function of where the pen is: the plate is
    // driven rail-to-rail and the wiper voltage is ratiometric.  Both
    // axes count DOWN across the panel (the plate's driven end is at the
    // left / top edge), and the panel is a little larger than the visible
    // 640x240, so the counts at the LCD's own corners sit inside the
    // 12-bit rails.
    //
    // The values below are fitted against the digitiser calibration the
    // netpad ROM boots with: Exyin.dll's own screen coordinates were read
    // back (TRawEvent EButton1Down) for taps across the panel, giving a
    // straight line per axis, which these constants invert.  A tap now
    // lands within a pixel of the pen everywhere on the panel; if the ROM
    // calibration is ever re-run on-device, EPOC re-fits its own end and
    // these stay the raw-hardware end of the chain.
    static constexpr double kNpTouchXAtLeft   = 3860.0;   // ADC at x = 0
    static constexpr double kNpTouchXAtRight  =  319.0;   // ADC at x = width
    static constexpr double kNpTouchYAtTop    = 3564.0;   // ADC at y = 0
    static constexpr double kNpTouchYAtBottom =  634.0;   // ADC at y = height
    // The plate is wider than the visible 640 columns, and the overhang
    // on the right carries the five silkscreen keys printed on the case
    // (top to bottom: Menu, brightness, Zoom, on-screen keyboard,
    // Extras).  Extrapolating the X fit above past x = 640, the band the
    // booted ROM answers on is x = 649..674, split into five equal bands
    // down the panel height — measured by tapping the netpad desktop
    // through the native harness and watching which taps the OS acted
    // on.  netpadAdsConvert therefore must NOT clamp touchX to the
    // panel; the frontend's silkscreen tap zones aim at the centre of
    // each band.
    // Pressure channels.  Rtouch = Rx·(X/4096)·(Z2/Z1 − 1), so a firm
    // contact is "Z1 large, Z2 barely above it" and a lifted pen is
    // Z1 = 0 (open circuit = infinite resistance).
    static constexpr uint16_t kNpTouchZ1Down = 900;
    static constexpr uint16_t kNpTouchZ2Down = 1150;
    // Supply monitor channels (single-ended, 12-bit).  The variant's own
    // channel table (ROM 0x500ab5c0 names / 0x500ab628 control bytes)
    // says which codec input carries which cell, and the board wires
    // them the opposite way round to the chip's pin names: the MAIN
    // battery is on AUX (control 0xe4) and the BACKUP cell is on VBAT
    // (0xa7 warm-up / 0xa4 final, polled every ~30 s).  Both report a
    // healthy charge — without a reading at all the power driver sees
    // zero volts and EPOC puts up its "backup battery critical" warning.
    // Overridable with PSION_NETPAD_ADC_MAIN / _BACKUP for bring-up.
    static constexpr uint16_t kNpAdcMain   = 3300;
    static constexpr uint16_t kNpAdcBackup = 3300;
    // Temperature channels (TEMP0/TEMP1) — room temperature.
    static constexpr uint16_t kNpAdcTemp = 2048;

    // UCB1200 cmd → response.  Stateful (uses ucbGpio*_ for the GPIO
    // block) so it's a member function, not a free function.
    uint16_t ucb1200Response(uint16_t cmd);
    // Touch ADC pair for a (x, y) pen-down sample, or the pen-up
    // sentinels when penDown is false.  Reads `touchX/touchY/penDown`
    // and the digitiser dimensions — pure helper sharing the same
    // scaling for the three sites that need it.
    void     touchAdcXY(uint16_t &xAdc, uint16_t &yAdc) const;

    // Series 7 / netBook keyboard matrix.  The BSP scans an 8-column ×
    // 7-row matrix by writing a column-drive value (8..15) to ASIC[0x08]
    // low nibble and reading row state back from the high byte of the
    // same halfword (ASIC[0x09]).  We store the 7-bit row state per
    // column here; the netBSD epockbd driver layout we follow assigns
    // KC(n) = (column << 3) + row_bit_index + 1 — see the keymap in
    // setKeyboardKey() for the EpocKey → (col, row) translation table.
    uint8_t  kbdMatrix_[8] = {0};
    int      kbdScanColumn_ = 0;        // last single-column selection (0..7)
    // Column-mask drive: each bit drives that column.  The BSP's
    // observed write values to ASIC[0x08] are {00, 02, 08, 0f, 37, ff}
    // (per eiger.cpp:32) — the existing single-column decode (low
    // nibble == 8+col) handles 0x08..0x0f, and this mask captures the
    // multi-column scan modes (0x02, 0x37, 0xff) so a column-OR read
    // of ASIC[0x09] returns rows from all driven columns.  Falls back
    // to kbdScanColumn_ when only one column is selected.  0 = no
    // column driven (scanner idle).
    uint8_t  kbdScanColumnMask_ = 0;
    uint32_t readOscr() const;
    void     writeOscr(uint32_t value);
    void     armMatch(int idx, uint32_t target);
    // Schedule OSMR1 to fire at the next 64 Hz boundary relative to the
    // current OSCR.  Called from the natural fire path and OSSR-ack path
    // to model OSMR1 as a hardware-style periodic timer.  No-op when
    // PSION_S7_NO_OSMR1_PERIODIC is set or when conditions aren't met.
    void     armOsmr1PeriodicIfEnabled(const char *reason);

    // RTC seconds counter.
    uint32_t rcnr = 0;
    uint32_t rtar = 0;
    uint32_t rttr = 0;
    uint32_t rtsr = 0;
    int64_t  rtcNextTickCycles = 0;

    // Synthetic-event scheduling for the netBook splash bring-up. See
    // sa1100.cpp executeUntil() for the rationale and timings. The
    // isNetBookRom_ flag gates the netBook-specific MMU L1 patch and
    // GPIO 0 power-button edge: Series 7's BSP sets up its own page
    // tables and doesn't gate splash on the GPIO edge, so applying
    // the netBook fixes there overwrites working mappings and stalls
    // the boot.
    bool isNetBookRom_   = false;
    bool isSeries7Rom_     = false;       // detected via descriptor magic
    // True when the loaded ROM is the 2 MB netBook YModem bootloader
    // (netBook_BL_v011_eng.bin and similar).  Detected via the
    // "Psion YModem Bootloader" signature near the end of the image.
    // Enables the same Eiger-ASIC behaviour as a full Series 7 / netBook
    // OS ROM (EEPROM emulation, CF detect IRQ, keyboard matrix) so the
    // bootloader can read the CF card to load D:\OS.IMG.
    bool isNetBookBootloader_ = false;
    bool s7OierWarnDone_   = false;      // one-shot OIER suppression warning
    bool s7DiagEtestSeen_  = false;      // one-shot: ETest.exe sub_cf94 entered
    bool s7DiagWservSeen_  = false;      // one-shot: WSERV code range entered
    bool s7DiagShellSeen_  = false;      // one-shot: Shell.app code range entered
    bool s7DiagScDvSeen_   = false;      // one-shot: ScDv.dll code range entered
    bool s7DiagEFileSeen_  = false;      // one-shot: EFile.exe code range entered
    bool s7DiagEikonSeen_  = false;      // one-shot: Eikon.dll code range entered
    uint32_t s7TicksFired_ = 0;          // count of synthetic OSMR1 ticks fired
    bool s7InNTimerQTick_  = false;      // true while DFC fire method active (NTimerQ::Tick running)
    // True once we've installed the post-IRQ scheduler-tick thunk via
    // installSeries7PostIrqHook() — see sa1100.cpp for rationale. This
    // mirrors Windermere's installDfcDrainThunk pattern: the kernel
    // expects a platform-variant extension to populate this hook slot,
    // but no such extension code path runs in our boot, so we plant a
    // minimal thunk that returns 1 (= "reschedule pending") on every IRQ.
    bool s7PostIrqHookInstalled_ = false;

    // Counter for the generalised csLock-leak workaround.  Incremented
    // at the FUN_50016d4c vtable dispatch site (0x50016de8) when we
    // pre-decrement csLock, decremented at the caller's return site
    // (0x50017840) where we re-increment csLock so non-blocking calls
    // don't underflow.  See sa1100.cpp executeUntil() csLock-leak block.
    int s7CsLockDecPending_ = 0;

    // Plant the post-IRQ scheduler-tick thunk and wire its address into
    // virt 0x800002f8.  Idempotent; returns true once the thunk is live.
    bool installSeries7PostIrqHook();
    uint32_t s7DispatchLogged_ = 0;      // budget for PSION_S7_TRACE_DISPATCH
    bool     s7VectorsProbed_  = false;  // one-shot guard for PSION_S7_PROBE_VECTORS

    void s7ProbeVectorsOnce();

    // Series 7: clear the OSMR1 "work pending" flag at virt 0x80000154.
    // Replaces the K4 periodic-clear hack with a surgical trigger fired
    // at the OSMR1 IRQ handler entry (PC=0x50005bec).  Returns true if
    // a non-zero value was actually cleared (so counters reflect only
    // "real work drained", not no-op writes).  `src` is logged with
    // PSION_S7_TRACE_DRAIN=1 so the cadence can be verified.
    bool s7ClearReschedFlag(const char *src);
    uint32_t s7DrainTriggerCount_     = 0;
    int64_t  s7DrainTriggerLastCycle_ = -1;

    // F-MULTIAGENT Agent D quick fix — unconditionally SET *0x80000154
    // = 1 at OSMR1 IRQ entry so the post-IRQ rescheduler always sees
    // work pending.  Tests the "scheduler asleep because flag never
    // re-armed after boot" hypothesis.  Env-gated by
    // PSION_S7_FORCE_RESCHED_TICK; mutually exclusive with the
    // drain-trigger (which clears the flag at the same PC).
    bool s7SetReschedFlag(const char *src);
    uint32_t s7ForceSetCount_ = 0;
    // Set all three kernel flags needed for the IRQ-exit fallback path
    // to invoke Reschedule and drain the DFC queue.  See sa1100.cpp for
    // the full rationale (addresses 0x80000154, 0x80000810, 0x80000814).
    bool s7SetDfcPendingFlags(const char *src);
    uint32_t s7DfcPendingSetCount_ = 0;
    // Edge tracker for PSION_S7_FORCE_RESCHED_ALL_IRQ — fix-candidate
    // (1) of the wake-protocol-deadlock investigation (post step 13).
    // Holds the previous (icpr & icmr) value so we only re-fire
    // s7SetReschedFlag() on a TRUE 0->1 edge in any IRQ bit (avoids
    // a level-triggered firehose).  Default 0; reset to current value
    // every call so a "still pending" level doesn't re-trigger.
    uint32_t s7LastIcprUnmasked_ = 0;
    // Per-bit edge counters so the trace can show which IRQ source
    // drove which fraction of the force-resched calls — useful when
    // diagnosing whether OST1 vs OST2 vs touch is doing the work.
    uint64_t s7ForceReschedAllCount_ = 0;
    int64_t  s7ForceReschedAllLastCycle_ = -1;

    uint32_t s7DispatchBitInProgress_ = 0; // bit currently being dispatched
    uint32_t s7InstallLogged_ = 0;       // budget for PSION_S7_TRACE_INSTALL
    // F-MULTIAGENT step 9c+ — budget for PSION_S7_TRACE_EIGER_ENCODER.
    // Logs the priority-encoder return value (r0) and ASIC register state
    // at PC=0x5000616c (the instruction right after the vtable[0x160]
    // call inside the Eiger sub-dispatcher at 0x50006140).  We expect
    // r0=15 on tap (= idx 15 = slot 64 IrqExtPenDown); current bug is
    // r0=2 (= slot 51 IrqExtRxFifo, unbound -> dispatcher early-out).
    uint32_t s7EncoderLogged_ = 0;
    uint32_t s7CfEncoderLogged_ = 0;
    // PC trace window: when non-zero, log every unique PC visited until
    // traceWindowEnd_ is reached.  Set via PSION_S7_PC_TRACE env var.
    int64_t  traceWindowEnd_   = 0;
    uint32_t traceWindowStart_ = 0;    // start cycle of current window
    // Small set of unique PCs seen in current window (use sorted vec)
    std::vector<uint32_t> tracedPCs_;
    bool prevMmuEnabled_ = false;       // CP15 c1 bit 0 transition tracker
    bool gpio0Fired_     = false;       // one-shot power-button edge
    int64_t gpio11NextCycle_ = 0;       // 30 Hz keyboard-matrix heartbeat (netBook)
    // Series 7 touchscreen sample heartbeat: while penDown is asserted,
    // real UCB1200 silicon delivers ADC-complete IRQs at ~100 Hz so the
    // kernel's touch driver accumulates a sample stream and forwards
    // TPointerEvents to the window server.  The PenDown edge alone
    // wakes the ISR but doesn't seed the stream.  Period = 1/100 s.
    int64_t penSampleNextCycle_ = 0;
    // One-shot AtoD pulse scheduled after PenDown ISR has had time to
    // bind the slot-7 handler — see Round 12 finding in
    // docs/series7-input-investigation.md.
    int64_t penAtodFireAtCycle_ = 0;
    // netBook Eiger TIMER0 touchscreen-sampler clock: next cycle to tick
    // TIMER0 while the pen is down (0 = not armed / reset on pen-up).
    int64_t nbTimer0NextCycle_ = 0;

    // Power / reset
    uint32_t pmcr = 0;
    uint32_t pssr = 0;
    // SF (sleep-force) state machine.  Set when guest writes PMCR.SF=1
    // and cleared when a PWER-enabled wake source fires.  Distinct from
    // cpu.wfiRequested because (a) wake is gated by PWER (not ICMR) and
    // (b) we want one-shot entry/exit logging for diagnosing the browser
    // suspend-to-sleep hang.
    bool sleepActive_ = false;
    // PSPR (scratch pad register) has UNPREDICTABLE state out of reset
    // on real SA-1100 silicon — its contents survive sleep but are
    // garbage on cold boot.  We initialise to a recognisable marker
    // On cold power-on, PSPR is UNPREDICTABLE per the SA-1100 manual,
    // but on a freshly-booted device the kernel's PSSR.SSS bit is
    // clear (no prior sleep), so EPOC ignores PSPR.  Initialise to 0
    // here — using a visible-marker value (e.g. 0xDEADBEEFu) caused
    // EPOC's "did the prior session save state here?" probes to see
    // a non-zero word and react as if it were valid saved state,
    // contributing to the "auto-navigate to random sample file"
    // behaviour observed by the user.
    uint32_t pspr = 0;
    uint32_t pwer = 0;
    uint32_t pcfr = 0;
    uint32_t ppcr = 0;
    uint32_t pgsr = 0;
    uint32_t posr = 0x01;            // oscillator ok
    uint32_t rsrr = 0;
    uint32_t rcsr = 0x01;            // cold boot (hardware reset)
    uint32_t tucr = 0;               // test unit control register

    // GPIO. The netBook ROM reads GPLR during boot to size peripherals;
    // giving it a stable 0 is enough for the MMU-setup phase.
    uint32_t gplr = 0;
    uint32_t gpdr = 0;
    uint32_t gpsr_latched = 0;
    uint32_t grer = 0;
    uint32_t gfer = 0;
    uint32_t gedr = 0;
    uint32_t gafr = 0;

    // Interrupt controller
    uint32_t icmr = 0;
    uint32_t iclr = 0;
    uint32_t iccr = 0;
    uint32_t icpr = 0;               // raw pending (updated when tick fires)

    // LCD controller. lccr0 bit 0 is ENA; the guest writes a framebuffer
    // base to DBAR1. Our readLCDIntoBuffer reads from DBAR1 when the
    // guest has turned the LCD on, otherwise it returns all-zero.
    uint32_t lccr0 = 0;
    uint32_t lccr1 = 0;
    uint32_t lccr2 = 0;
    uint32_t lccr3 = 0;
    uint32_t dbar1 = 0;
    uint32_t dcar1 = 0;
    uint32_t dbar2 = 0;
    uint32_t dcar2 = 0;
    uint32_t lcsr = 0;
    // LCD end-of-frame IRQ firing: synthesise a frame-done event at
    // ~60 Hz while LCCR0.ENA is set (M19).  The kernel's LCD driver
    // waits for an IRQ at the end of each frame to advance its
    // double-buffer pointers and signal "VSync" to the window server.
    int64_t  lcdFrameNextCycle_ = 0;

    // Peripheral Pin Controller (PPC) — used on Series 7 for UCB1200-
    // via-SSP mux setup.  M23: storage-only (read returns last write).
    uint32_t ppdr = 0;     // pin direction
    uint32_t ppsr = 0;     // pin state
    uint32_t ppar = 0;     // pin assignment
    uint32_t psdr = 0;     // sleep-mode direction
    uint32_t ppfr = 0;     // pin flag

    // DMA controller (H10) — 6 channels, storage-only.  Per-channel
    // pseudo-state so reads/writes don't fall through to the unmapped
    // logger.  See readDma/writeDma in sa1100.cpp for semantics.
    struct DmaChannel {
        uint32_t ddar = 0;       // device descriptor (which periph + dir)
        uint32_t dsr  = 0;       // status (bit0 DONE_A, bit4 IE, ...)
        uint32_t dbsa = 0;       // buffer start A
        uint32_t dbta = 0;       // buffer transfer count A
        uint32_t dbsb = 0;
        uint32_t dbtb = 0;
        // Deferred DMA completion for audio channels.  When STRT is
        // written on a channel whose DDAR points to the MCP audio
        // port (0x80060xxx), the transfer completes after
        // dbta * cycles-per-sample instead of instantly.  This gives
        // the recording DFC time-based callbacks at the codec sample
        // rate.  -1 = no pending deferred completion.
        int64_t  completeAtCycle = -1;
        uint8_t  pendingBuf = 0; // 1=A, 2=B
    };
    DmaChannel dmaCh_[6];

    // PWM controller (H16) — backlight brightness stub.  Two channels,
    // CTRL/PSCR/PWDR each.  Pure storage, no LCD-side effect.
    struct PwmChannel {
        uint32_t ctrl = 0;
        uint32_t pscr = 0;
        uint32_t pwdr = 0;
    };
    PwmChannel pwm_[2];

    // PWM-to-host-tone bridge state.  On the real netBook PWM1 drives a
    // piezo buzzer used for system beeps (key click, alarm chirp, error
    // ding).  We synthesise a square wave at the PWM frequency and pump
    // it into the codec's DAC queue via emitBuzzerSamples — same plumbing
    // Windermere uses for BZCONT.  pwmBuzzerNextTickAt_ schedules the
    // 64 Hz pump cadence emitBuzzerSamples expects.
    int64_t pwmBuzzerNextTickAt_ = 0;
    int     pwmBuzzerToneHz_     = 0;

    // Coarse gate for the audio-codec tick (tickTxFifoDrain +
    // tickDeferredCsint).  Both functions only do work when their
    // own internal cycle gates fire, but on real hardware the codec
    // sample rate is 8 kHz — so re-checking them per CPU instruction
    // (~30 M times/sec) is pure overhead.  This gate skips the inner
    // calls until passedCycles is at least at the next audio-sample
    // boundary.
    int64_t audioTickNextAt_ = 0;
    // Series 7 SAC RX: free-running cap-refill deadline (500 Hz → 16 samples
    // each → 8 kHz codec rate).  See the audio-tick refill in run().
    int64_t s7RxRefillAt_ = 0;

    // Memory controller
    uint32_t mdcnfg = 0;
    uint32_t mdrefr = 0;
    uint32_t msc0 = 0;
    uint32_t msc1 = 0;
    uint32_t msc2 = 0;
    uint32_t mecr = 0;
    uint32_t smcnfg = 0;
    // DRAM CAS waveform registers (banks 0/1 = 00/01/02, banks 2/3 = 20/21/22).
    // Programmed by the kernel during DRAM bring-up; only stored.
    uint32_t mdcas[6] = {0, 0, 0, 0, 0, 0};

    // Touch / keyboard state placeholder.
    int32_t touchX = 0, touchY = 0;
    bool penDown = false;

    // Peripheral-log rate limits — the netBook ROM scans large regions
    // of MMIO during boot and we don't want to drown the harness log.
    int unknownRegReads = 0;
    int unknownRegWrites = 0;
    int unmappedLogs = 0;
    int asicReadLogs = 0;
    int asicWriteLogs = 0;
    bool    inputTrace_          = false;
    int     inputTraceReadsLeft_ = 0;
    static constexpr int kInputTraceWindow_ = 1500;
    void logUnmapped(uint32_t physAddr, ValueSize vs, uint32_t value = 0);

    // DIAG: cycle at which the kernel first wrote LCCR0.ENA = 0
    // (the start of phase-2 reconfigure). -1 = never. We use this
    // to log only the post-disable companion-ASIC reads, which are
    // the candidates for what the kernel polls before re-enabling.

    // Companion ASIC (Psion in-house custom silicon sitting on nCS1,
    // physical address range 0x10000000-0x1FFFFFFF). No public datasheet
    // survives but the ROM pokes it extensively during boot to talk to
    // the keyboard matrix, backlight, battery monitor and power rails.
    //
    // Registers are treated as 32-bit scratch storage by default — a
    // read returns what was last written there, or a monotonically
    // increasing cycle counter for any register in the first 0x100
    // bytes (the ROM polls that range as a free-running counter for
    // its power-on-reset timing loops). This gets the CPU past the
    // early init loops without getting lost in ASIC minutiae.
    //
    // The scratch is 64 KB: the Series 7 kernel touches multiple ASIC
    // sub-blocks (0xa000, 0xc000, 0xe000, …) that have to read back
    // independently — collapsing to a 4 KB mirror caused the kernel's
    // read-modify-write sequence on 0x5800c018 to clobber state at
    // 0x5800a000 and tripped an assertion in the power manager.
    static constexpr uint32_t kAsicSize = 0x10000;
    static constexpr uint32_t kAsicMask = kAsicSize - 1;
    uint8_t asicRegs[kAsicSize] = {};

    // ASIC14 (the netBook/Series 7 "helper" chip on nCS1) exposes the LCD
    // contrast DAC at offset 0x16.  EPOC drives it from the Screen control
    // panel and the Inc/DecContrast keys, encoding the 1..32 user scale as
    // (value - 1) — measured directly off the b756 ROM.  readLCDIntoBuffer
    // reads asicRegs[kContrastReg] each frame and modulates the panel output.
    static constexpr uint32_t kContrastReg     = 0x16;
    static constexpr int      kDefaultContrast = 20;   // factory default
    static constexpr int      kMinContrast     = 1;
    static constexpr int      kMaxContrast     = 32;

    // (T9) Eiger ASIC class wired into the dispatcher.  Constructed
    // with a backing pointer to asicRegs so its register accessors
    // and the SA-1100 emulator's scratch model stay coherent without
    // explicit sync calls.  Low offsets (0x00..0xFF) route through
    // this class for register-level semantics (status polls, EEPROM);
    // higher offsets continue to hit the asicRegs scratch directly
    // since the Eiger class is only 4 KB and the kernel touches
    // sub-blocks at 0xa000/0xc000/0xe000 that need independent storage.
    std::unique_ptr<Eiger> eiger_;
    // Number of remaining ASIC[0x40] reads that should report bit 1 set
    // ("command in progress"). The BSP's EEPROM-read sequence requires
    // exactly two such busy reads before the line goes ready, so we set
    // this to 2 each time the BSP writes a command byte to ASIC[0x4c].
    int asic40PollsBeforeReady = 0;
    // Cycle (passedCycles) until which ASIC[0x40] bit 1 ("SPI command in
    // progress" / busy) reads SET.  The Eiger command port is a real
    // SA-1111-style SPI engine: writing a command byte to ASIC[0x4c]
    // asserts busy for the duration of the transaction, then it clears —
    // a TIME-based state, independent of how many times (or by whom) the
    // status register is read.  The legacy `asic40PollsBeforeReady`
    // decrementing-read counter modelled this as a fixed number of reads,
    // which conflated unrelated pollers of the SAME register (e.g. the
    // bit-10 event poll at OS 0x500094a0) with the EEPROM transaction's
    // own busy reads: an interleaved unrelated read would drain the budget
    // and make the EEPROM cmd-accept check (bit 1 must be SET right after
    // the command write) spuriously fail, flagging a phantom read error and
    // retrying the same EEPROM word forever (faithful-CF full-read wedge,
    // docs iteration 42).  Driving bit 1 off a cycle window instead makes
    // it deterministic per transaction regardless of read interleaving,
    // matching real hardware.  Set on each ASIC[0x4c] command write.
    int64_t asic40BusyUntilCycle_ = -1;
    // One-shot: release the UCB1200 command mutex once when the busy
    // window above expires (the recording path's SPI-complete handshake).
    bool asic40MutexReleasePending_ = false;

    // Eiger status-register poll-clear counters for offsets 0x06 and
    // 0x0a.  Mirrors asic40PollsBeforeReady but for two more offsets
    // the classifier identified as heavily polled (S7 BSP polls 0x06
    // 1300+ times and 0x0a 184 times reading the same value).
    // Set on a matching write of the "start-operation" value
    // (0x18 -> 0x06, 0x02 -> 0x0a); decremented on each read; clears
    // the relevant bit when it hits zero so the poll loop exits.
    int asic06PollsBeforeClear = 0;
    int asic0aPollsBeforeClear = 0;

    // Series 7 / netBook serial EEPROM image, 128 bytes, byte-addressed
    // as 16-bit words at indices 0x180..0x1bf via the ASIC[0x48] /
    // [0x4c] command port (see FUN_50088c4c in the v1.05(254) decompile).
    // The kernel reads this at boot, XOR-checksums it against the magic
    // 0x42, and uses fields at offsets +0x0a, +0x0c, +0x0e, +0x18,
    // +0x20, +0x21, +0x28, +0x29.. for runtime config (touchscreen
    // calibration, panel orientation, serial number).  Without this
    // emulation the kernel either retries forever (no patch) or
    // accepts garbage (with the legacy CMP-R0,R0 patch) — both break
    // touch input.  We synthesise an all-zero image with a single
    // 0x42 magic byte at offset 0x7f so the XOR checksum passes
    // naturally; all field accessors then return zero, which matches
    // the kernel's "no EEPROM" defaults but via the VALID-EEPROM
    // branch (so other init code that gates on `_DAT_40000314 != 0`
    // still sees the expected state).
    static constexpr size_t kEepromSize = 128;
    static constexpr uint16_t kEepromBaseAddr = 0x180;
    uint8_t  eepromImage_[kEepromSize] = {};

    // Unique-id word (see the public accessors above).
    static constexpr size_t   kEepromMachineId = 0x18;
    // A runtime-programmed ID has to survive initEepromImage(), which
    // rebuilds the whole image from scratch on the internal reset path
    // (netpad wake-by-reset).  Remembered here and re-applied there.
    bool     machineIdOverridden_ = false;
    uint32_t machineIdOverride_ = 0;
    // Writes `word` into the ID bytes and re-folds the image checksum.
    void     applyMachineIdWord(uint32_t word);
    // Model-UID copies in the loaded ROM (see the accessors above): located
    // once in loadROM, rewritten as a set.  Zero when unknown for this
    // build, which is also how the netpad reports "not settable".
    std::vector<size_t> machineIdPrefixOffsets_;
    uint32_t machineIdPrefix_ = 0;
    void     locateMachineIdPrefix();
    // netBook: the flash this machine boots is the 2 MB YModem bootloader,
    // and its OS — the image that actually prints the Unique id — is
    // D:\OS.IMG on the CF card.  Patching ROM[] therefore changes nothing
    // the user ever sees (worse, the bootloader's own copies of the
    // constant sit at offsets the OS overwrites wholesale at the handoff),
    // so the netBook's copies are tracked on the CARD instead: located when
    // a card is attached, rewritten from there, and read by the guest's own
    // medata driver on the faithful boot path.
    std::vector<size_t> machineIdPrefixCardOffsets_;
    void     locateMachineIdPrefixOnCard();
    void     applyMachineIdPrefixToCard();

    uint32_t readAsic(uint32_t offset, ValueSize vs);
    uint32_t readAsicImpl(uint32_t offset, ValueSize vs);
    // Eiger SPI command-busy state (ASIC[0x40] bit 1).  Returns true while a
    // command issued via ASIC[0x4c] is still in its per-transaction cycle
    // window; on the first read after the window expires it fires the one-shot
    // UCB1200 SPI-complete handshake (clears the command mutex at VA
    // 0x8000001C).  Shared by every ASIC[0x40]/[0x41] read path so bit 1 is a
    // single consistent signal.  See docs iteration 42.
    bool     asic40SpiBusy();
    void     writeAsic(uint32_t offset, uint32_t value, ValueSize vs);
    void     initEepromImage();
    void     setEepromDeviceName(const char *name);

    // SA-1100 PCMCIA socket 0 windows (regions 0x20-0x2F).
    // Real silicon decodes the 256 MB socket aperture as:
    //   0x20000000-0x23FFFFFF : I/O space (64 MB)
    //   0x28000000-0x2BFFFFFF : Attribute memory
    //   0x2C000000-0x2FFFFFFF : Common memory
    // Routes to the cfCard VCFCard.  Rate-limited logs are emitted at
    // first contact so the harness output stays useful.
    uint32_t readPcmcia (uint32_t physAddr, ValueSize vs);
    void     writePcmcia(uint32_t physAddr, uint32_t value, ValueSize vs);
    int      pcmciaReadLogs_  = 0;
    int      pcmciaWriteLogs_ = 0;

    // Pending-interrupt helpers.
    void recomputeInterrupts();

    // Per-IRQ-slot delivery counter.  Incremented in recomputeInterrupts
    // for each bit in (icpr & icmr) on the 0->1 edge.  Mirrors
    // windermere.h:132's irqCauseCount[] design.  32 slots match the
    // ICIP/ICMR/ICPR width; symbolic names live in
    // sa1100_defs.h::kIrqNames.
    uint32_t irqCauseCount[32] = {};

    // Per-bit diff of ICMR / pending-IRQ changes.  Emits one log line
    // per changed bit using the kIrqNames table — mirrors the
    // diffInterrupts pattern in windermere.cpp:2996.  Gated on
    // PSION_S7_IRQ_DIFF.  `tag` is "ICMR" or "ICPR" so traces are
    // unambiguous.
    void diffInterrupts(const char *tag, uint32_t oldval, uint32_t newval);

    // Rolling 500-ms dump of per-slot IRQ counters + Eiger register
    // hotspots.  Hooked next to EigerClassify::maybePeriodicDump in
    // the main tick path.  No-op unless PSION_S7_IRQ_COUNTERS is set.
    void maybeRollingDump();

    // Track the previous ICMR so writeIntc can call diffInterrupts on
    // every change.  Initialised to 0 (matches the post-reset state of
    // icmr above).
    uint32_t icmrPrev_ = 0;
    // Last cycle the rolling dump fired.  Compared against passedCycles
    // to fire every ~500 ms of sim time (= 500'000 ticks at 1us cycle).
    int64_t  lastRollingDumpCyc_ = 0;

    // T12: GPIO edge injection helpers.  Respect GRER/GFER (edge-enable
    // masks) just like real silicon, update the live-level register
    // (GPLR), and refresh the ICPR[11] composite when n is in 11..27.
    // `fireGpioRisingEdge` is for rising edges; `fireGpioFallingEdge`
    // for falling edges; `fireGpioEdge` is used by call sites that
    // model a "level latched, then auto-cleared by ISR" pulse and
    // don't care which edge it counts as.
    void fireGpioRisingEdge(int n);
    void fireGpioFallingEdge(int n);
    void fireGpioEdge(int n);

    // T13: recompute ICPR[IRQ_GPIO11_27] from the bitmap of currently-
    // latched GEDR bits 11..27.  Single source of truth replacing the
    // open-coded `icpr |= (1u << IRQ_GPIO11_27)` / `icpr &= ~...`
    // pattern that had appeared in 7+ sites.
    void updateGpio11Composite();

    // Sleep-mode wake helpers.  sleepWakeAsserted() reports whether a
    // PWER-enabled wake source is currently latched (called from the
    // WFI/sleep handler in executeUntil() and from recomputeInterrupts()
    // so any edge that lights a PWER bit immediately cancels sleep).
    // pulsePwerWakeGpios() synthesises an edge on every PWER-enabled
    // GPIO so a browser key/touch can act as the "power button" that
    // real PWER actually maps to.
    bool sleepWakeAsserted() const;
    void pulsePwerWakeGpios();
};

}
