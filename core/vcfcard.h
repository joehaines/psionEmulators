// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

// Virtual CompactFlash / PC Card storage.
//
// Holds a raw disk image (typically FAT16-formatted by the frontend) and
// exposes read/write accessors that the device memory bus can route into.
// Keeps card-presence state so the per-device card controller can surface
// card-detect and door signals to EPOC.
//
// Three access "shapes" are offered:
//   - Attribute memory: exposes a minimal PCMCIA Card Information Structure
//     (CIS) identifying this as a fixed-disk storage card, at even byte
//     offsets (the odd bytes are 0xFF as on real hardware). The Card
//     Configuration Register (CCR) window lives at attribute offset 0x200
//     and is writable — EPOC writes the Option register here to switch the
//     card into I/O mode.
//   - Common memory: raw pass-through of the image bytes at byte offsets.
//     Used in memory-mode for FAT sector access when configured that way.
//   - I/O mode: ATA task-file registers at a separate physical window; the
//     host decodes 8 byte offsets (0x1F0..0x1F7) into our ATA state machine
//     (IDENTIFY DEVICE + READ/WRITE SECTORS). The 16-bit Data register is
//     what actually transfers FAT sectors once a command is running.
class ARM710;

class VCFCard {
public:
	VCFCard();

	// Route CF traces through the same cycle-prefixed logger the host uses,
	// so sector-drain / ATA-command messages sit alongside ETNA/ISR lines in
	// the harness log instead of running free on stderr.
	void setOwner(ARM710 *cpu) { owner = cpu; }

	// Faithful-netBook mode.  The netBook's epbus/medata driver needs ATA
	// semantics (post-command BSY transient, command-aborts-transfer, deferred
	// IRQ-ack) that the simpler polled CF drivers on 5mx / Series 7 / Revo /
	// Osaris do NOT expect — enabling them globally makes those devices see a
	// spurious BSY and report the card "corrupt".  So these behaviours are
	// OPT-IN per card instance and only the netBook turns this on.
	void setFaithfulMode(bool on) { faithfulMode_ = on; }
	bool faithfulMode() const { return faithfulMode_; }

	// Returns true once a valid image has been attached.
	bool inserted() const { return _inserted; }

	// Size of the currently attached image, in bytes. Zero if no card.
	size_t imageSize() const { return image.size(); }

	// Replace the in-memory image with the provided bytes and raise
	// card-present. Takes a copy; caller keeps ownership of `bytes`.
	void attach(const uint8_t *bytes, size_t len);

	// Reset the ATA task-file / sector-transfer state to a freshly-powered
	// card, WITHOUT touching the image bytes or card-present flag.  Used at
	// the netBook bootloader→OS handoff: the bootloader's faithful read leaves
	// the card mid-transfer, and the booted OS's medata driver must see a
	// clean card (a fresh IDENTIFY) or its PDD init hangs.
	void resetAtaState();

	// Clear card-present without wiping the image data, so a subsequent
	// attach() is cheap on the frontend (re-upload same bytes).
	void detach();

	// Read/write the raw common-memory byte window.
	uint8_t readByte(uint32_t offset) const;
	uint32_t readWord(uint32_t offset) const;
	void writeByte(uint32_t offset, uint8_t value);
	void writeWord(uint32_t offset, uint32_t value);

	// Attribute memory: CIS tuples at low offsets, CCR at offset 0x200.
	uint8_t readAttributeByte(uint32_t offset) const;
	void writeAttributeByte(uint32_t offset, uint8_t value);

	// ATA task-file register access (I/O window). offset is 0..7 for the
	// primary ATA registers. 16-bit Data reads/writes go through ataRead16/
	// ataWrite16 so FAT sector transfers stay on the 16-bit ATA bus.
	uint8_t ataRead8(uint32_t offset);
	void ataWrite8(uint32_t offset, uint8_t value);
	uint16_t ataRead16();
	void ataWrite16(uint16_t value);

	// True when the card is currently asserting IREQ# — set whenever a
	// command finishes or a new sector's data becomes available, cleared
	// when the host reads the primary Status register (ATA semantics).
	// Gated by a short cycle delay applied on every assertion: real CF
	// cards take microseconds to move from "cmd written" to "IREQ#
	// asserted", and routing the IRQ synchronously on the same cycle as
	// the host's cmd-register write pre-empts the driver's kernel
	// dispatcher before it finishes setting up its post-command state,
	// hanging the CPU in the IRQ vector. Stepped down by tickIrqDelay()
	// from the host's emulator loop.
	bool irqAsserted() const {
		return irqPending && _inserted && irqDelayCycles == 0;
	}
	// True while a multi-sector READ (async/interrupt-driven) is in progress —
	// gates the host's Eiger data-IREQ (sub-source 10) injection so it isn't
	// fired for the polled single-sector / IDENTIFY commands.
	uint32_t sectorsRemaining() const { return ataSectorsRemaining; }
	bool multiSectorReadActive() const {
		return ataMultiSectorRead && _inserted;
	}
	void tickIrqDelay(int cycles) {
		if (irqDelayCycles > cycles) irqDelayCycles -= cycles;
		else irqDelayCycles = 0;
	}
	// IREQ# state before the assert delay is applied, and how much of that
	// delay is left. The host needs both to schedule a wake for the assertion:
	// a guest sleeping on this interrupt has to be woken AT it, not at whatever
	// unrelated event happens to be scheduled next. See
	// SA1100::Emulator::nextSocEventCycle().
	bool irqPendingRaw() const { return irqPending && _inserted; }
	int  irqDelayRemaining() const { return irqDelayCycles; }

	// Diagnostic counters consumed by the host's CF-IRQ re-enable strategy
	// sweep. Every ATA command written to reg 0x7 bumps ataCommandCount;
	// every primary-Status register read bumps ataStatusReadCount (a good
	// "driver is inside its ISR right now" signal because that read is the
	// CF ack); every sector read drains or write flushes bumps
	// sectorBoundaryCount. Public read access is fine — they're just stats.
	uint32_t ataCommandCount = 0;
	uint32_t ataStatusReadCount = 0;
	uint32_t sectorBoundaryCount = 0;

	// Direct access to the raw image so the WASM bridge can stream it back
	// out to the frontend for download without an extra copy.
	const uint8_t *data() const { return image.data(); }
	uint8_t *data() { return image.data(); }

private:
	std::vector<uint8_t> image;
	bool _inserted = false;

	// Card Configuration Registers (CCR). Real CF cards put these at a
	// configurable base in attribute memory; our CIS advertises base 0x200
	// (CIS byte index 0x100 — see CISTPL_CONFIG in vcfcard.cpp).
	//   Option (+0x00): bit 0-5 = config-index, bit 6 = levelIREQ, bit 7
	//     SRESET. EPOC writes a non-zero config-index to enter I/O mode.
	//   Status (+0x02): I/O-is-enabled latches + interrupt-pending flag.
	//   Pin replacement (+0x04), Socket & Copy (+0x06): left as plain
	//     readable/writable storage; EPOC doesn't probe them meaningfully
	//     for a single-function CF card.
	uint8_t ccrOption = 0;
	uint8_t ccrStatus = 0;
	uint8_t ccrPin = 0;
	uint8_t ccrCopy = 0;

	// ATA task-file state. Kept intentionally small — we only ever need
	// IDENTIFY DEVICE (command 0xEC), READ SECTOR(S) (0x20), and WRITE
	// SECTOR(S) (0x30) to mount a FAT volume.
	uint8_t ataError = 0;
	uint8_t ataFeatures = 0;
	uint8_t ataSectorCount = 1;
	uint8_t ataSectorNumber = 0;  // LBA[7:0]
	uint8_t ataCylLow = 0;        // LBA[15:8]
	uint8_t ataCylHigh = 0;       // LBA[23:16]
	uint8_t ataDrive = 0xA0;      // LBA[27:24] + DEV + always-set bits; bit6 = LBA mode
	uint8_t ataStatus = 0x50;     // DRDY | DSC (ready, drive seek complete)

	// Sector transfer buffer. Filled on READ SECTOR command completion;
	// drained by successive Data-register reads. For WRITE SECTOR it's the
	// other way around — writes accumulate here and flush to the image on
	// the last byte.
	uint8_t sectorBuffer[512] = {};
	uint32_t sectorBufferPos = 0;
	uint32_t sectorBufferFill = 0;  // number of valid bytes in the buffer
	uint32_t ataLBA = 0;            // computed on command start
	uint32_t ataSectorsRemaining = 0;
	bool ataWriting = false;        // write command in progress
	// True while a *multi-sector* READ is in progress (sector count != 1).
	// Such reads use medata's async/interrupt path (it enables and blocks on
	// kernel source 59 = Eiger sub-source 10); single-sector reads and
	// IDENTIFY use the synchronous polled path and must NOT get a spurious
	// data IREQ injected, or enumeration is disrupted.  Set on a multi-sector
	// READ command, cleared on any other command or when the read completes.
	bool ataMultiSectorRead = false;
	// IREQ# line state. Asserted by commands that produce data or complete
	// without data; de-asserted when the host reads the Status register
	// (0x7) — matches CF's "reading status clears IRQ" semantics.
	bool irqPending = false;
	// Post-assertion delay: see irqAsserted() for rationale. Reset to
	// kIrqAssertDelay on every rising edge in the .cpp, stepped down by
	// the host each emulator cycle.
	int irqDelayCycles = 0;
	// BSY (Status bit 7) transient.  Real ATA/CF asserts BSY the instant the
	// command register is written and clears it once the card has the data
	// ready (or the command is complete).  EPOC's medata PIO transfer routine
	// (netBook ROM 0x500a81d0) reads Status at entry and BAILS if BSY is
	// already clear — it expects to catch the card busy, then waits for BSY to
	// fall before draining the data register.  Without modelling the transient
	// the multi-sector media read transfers nothing and times out.  We hold
	// BSY for the first few Status reads after a data/complete command, then
	// drop it (revealing DRQ), so the "wait while busy" loop sees the 1->0
	// edge it needs.
	int ataBusyReads = 0;

	void ataStartCommand(uint8_t cmd);
	void ataFillIdentify();
	void ataLoadNextSector();
	void ataFlushSector();

	ARM710 *owner = nullptr;
	bool faithfulMode_ = false;   // netBook-only ATA-semantics opt-in
	void trace(const char *fmt, ...);
};
