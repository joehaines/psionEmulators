// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "vcfcard.h"
#include "arm710.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void VCFCard::trace(const char *fmt, ...) {
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (owner)
		owner->log("%s", buf);
	else
		std::fprintf(stderr, "%s\n", buf);
}

// Minimal PCMCIA Card Information Structure (CIS) for a fixed-disk storage
// card. Each tuple is a (code, length, ...data) triple. Placed at even byte
// offsets of the attribute memory window; odd bytes read back as 0xFF to
// mimic the 8-bit-on-16-bit bus behaviour of real CF cards.
//
// This CIS advertises:
//   - CISTPL_DEVICE / VERS_1 / MANFID / FUNCID / FUNCE: basic identification
//     as a fixed-disk ATA device.
//   - CISTPL_CONFIG: Card Configuration Register (CCR) base at attribute
//     offset 0x200.
//   - CISTPL_CFTABLE_ENTRY: describes a configuration that uses I/O space
//     at 0x1F0–0x1F7 with 16-bit-capable transfers (ATA primary).
//
// Without CISTPL_CONFIG + CISTPL_CFTABLE_ENTRY, EPOC's CIS parser at
// 0x500569ec stops after CISTPL_END, never learns the CCR location, and
// the drive never mounts.
static const uint8_t kCIS[] = {
	// CISTPL_DEVICE: device-type=null (no common-memory Device ID)
	0x01, 0x03, 0x00, 0x00, 0xFF,
	// CISTPL_VERS_1: compliance=5.0, manufacturer/product/version strings.
	// Body length = 2 + 6 + 7 + 5 + 1 + 1 = 22 = 0x16 bytes.
	0x15, 0x16,
		0x05, 0x00,                        // 2 bytes: PCMCIA 5.0 compliance
		'P','S','I','O','N', 0x00,         // 6 bytes: manufacturer
		'V','I','R','T','C','F', 0x00,     // 7 bytes: product
		'1','.','0','0', 0x00,             // 5 bytes: lot / version
		0x00,                              // 1 byte: trailing info string
		0xFF,                              // 1 byte: end of VERS_1
	// CISTPL_CONFIG: CCR sits at attribute offset 0x200 on real CF cards.
	// Body layout:
	//   TPCC_SZ  = 0x01  -> RASZ-1=1 (2 addr bytes), RMSZ-1=0 (1 mask byte)
	//   TPCC_LAST= 0x01  -> highest CFTABLE_ENTRY index is 1
	//   TPCC_RADR= 0x0200 (LE) -> config register base, in bytes-of-CIS
	//   TPCC_RMSK= 0x0F  -> option|status|pin|copy registers all present
	0x1A, 0x05, 0x01, 0x01, 0x00, 0x02, 0x0F,
	// CISTPL_CFTABLE_ENTRY index 1 (default). Describes ATA primary I/O.
	//   TPCE_INDX= 0xC1: default=1, intface=1, index=1
	//   TPCE_IF  = 0x41: interface type=1 (I/O+memory), READY-active
	//   TPCE_FS  = 0x89: Misc(bit7)+I/O(bit3)+Power VCC only(bits1:0=01)
	//   Power desc (VCC params byte + values):
	//     0x06 = MinV(bit1) + MaxV(bit2) present. No NomV (bit 0): NomV
	//       collapses to NomV±5% in the driver, which from 5V gives
	//       VMin=4750, VMax=5250 — excludes the 5mx socket's 3.3V rail
	//       and IsMachineCompatible() rejects with "Bad Vcc" (see
	//       Symbian pccard/spccard.cpp:980). Advertising MinV/MaxV
	//       directly as 3.3V–5.5V covers both PCMCIA voltage domains
	//       (the range real CF cards operate across).
	//     0xAD, 0x50 = MinV = 3.3V. Encoding: mantissa idx 5 (CisMantisa=25),
	//       exponent 5 (×100 in the µA lookup), extension byte 80 →
	//       PwrTplToMicroAmps = 250+80 = 330; ×100 = 33000 µA → 3300 mV.
	//     0x5D = MaxV = 5.5V. mantissa idx 11 (CisMantisa=55), exp 5 →
	//       550×100 = 55000 µA → 5500 mV.
	//   I/O desc: 0xEA (10 addr lines, 8+16-bit, range descriptor follows),
	//     0x60 (1 range, 2 addr bytes, 1 length byte),
	//     0x00 0x00 (base=0x0000 — contiguous I/O mode; EPOC's PCCARD layer
	//       maps this window to the CLPS7111 region we decode at 0x60000000),
	//     0x0F (length=16 bytes). EPOC's ATA media driver in
	//       eka/drivers/medata/pccd_ata.cpp searches for a config entry with
	//       `iValidChunks==1 && __IS_IO_MEM && iMemLen==0x10`; an 8-byte
	//       window (as for classic ATA primary at 0x1F0–0x1F7) is rejected
	//       with KErrNotSupported. A 16-byte contiguous window matches
	//       CompactFlash's "Contiguous I/O" addressing (Data at offset 0,
	//       Alt-Status/DevCtl at offset 0x0E, DriveAddr at 0x0F).
	//   Misc: 0x00 (single-function, no audio/RWS/power-down, no ext byte)
	// Body length: 1+1+1 + 4 + 5 + 1 = 13 = 0x0D bytes.
	0x1B, 0x0D, 0xC1, 0x41, 0x89, 0x06, 0xAD, 0x50, 0x5D,
	      0xEA, 0x60, 0x00, 0x00, 0x0F, 0x00,
	// CISTPL_FUNCID: function code 4 = Fixed Disk (PC Card Standard).
	// sysinit byte = 0 (no POST init required). Without FUNCID at all the
	// CIS walker at 0x50056b34 loops retrying card identify indefinitely.
	0x21, 0x02, 0x04, 0x00,
	// CISTPL_FUNCE: fixed-disk function extension.
	//   sub-tuple 0x01 = disk device interface; body byte = 0x01 (ATA)
	0x22, 0x02, 0x01, 0x01,
	// CISTPL_DEVICE_GEO (0x1C): disk geometry. Required by the 5mx Pro
	// bootloader's CIS parser at 0x7c10/0x7c6c. The wrapper:
	//   - searches for tuple code 0x1C; without it the loader bails.
	//   - then linearly scans the body for bytes whose high nibble is
	//     0xD. The function is called twice: once needing 1 match, then
	//     again needing 2 matches. Without enough 0xDx bytes the second
	//     call returns "no match" (bit 31 set) and the loader bails out.
	// Body bytes that the scanner actually inspects sit at attribute
	// offsets 0x7c, 0x80, 0x84 (= body[1], body[3], body[5]).
	// body[0] (bus_size) must remain 0x02 — the parser at 0x7c88 checks
	// it explicitly. Place 0xD0 at body[1], body[3], body[5] so the
	// count-2 scan call finds two matches before hitting TPL_END.
	0x1C, 0x06, 0x02, 0xD0, 0x01, 0xD0, 0x01, 0xD0,
	// CISTPL_JEDEC_C (0x18): JEDEC ID for common memory. Required by the
	// 5mx Pro bootloader's compatibility check at 0x7e44, which calls the
	// CIS parser with tuple-code 0x18, then dereferences body bytes at
	// attribute offsets r2 and r2+2 where r2 = (r1_in << 4) and r1_in is
	// the byte counter returned by the prior 0x7c6c scan. The outer loop
	// at 0x7e98-0x7eac accepts the result iff:
	//   byte[r2]   == 0xDF || 0x45  (manufacturer code: 0x45 = SanDisk)
	//   byte[r2+2] == 0x01 || 0x02  (device id within manufacturer)
	// Filling the body uniformly with the pattern (0xDF, 0x01) at every
	// kCIS-body byte position satisfies this for any r1_in value the
	// scanner produces, regardless of how many 0xD0 matches are made.
	// Length 0x10 covers attribute offsets 0..30 (r1_in up to 1 — the
	// 0xD0 scanner inside DEVICE_GEO returns r1=1 for the first match;
	// length is generous to allow for additional iterations).
	0x18, 0x10,
		0xDF, 0x01, 0xDF, 0x01, 0xDF, 0x01, 0xDF, 0x01,
		0xDF, 0x01, 0xDF, 0x01, 0xDF, 0x01, 0xDF, 0x01,
	// CISTPL_NO_LINK: no link to further CIS chain
	0x14, 0x00,
	// CISTPL_END
	0xFF,
};

// Attribute-memory byte offset of the Card Configuration Register window
// (advertised in the CISTPL_CONFIG tuple above). CCR registers land at
// offset+0x00 (Option), +0x02 (Status), +0x04 (Pin), +0x06 (Copy).
static constexpr uint32_t kCCRBase = 0x200;

// Delay applied on every IRQ assertion (~27 microseconds at 36.864 MHz).
// Below any real CF command latency, long enough for the host driver
// to finish its post-cmd-write instruction sequence before the kernel
// IRQ dispatcher pre-empts it.
static constexpr int kIrqAssertDelay = 1000;

VCFCard::VCFCard() {
}

void VCFCard::attach(const uint8_t *bytes, size_t len) {
	image.assign(bytes, bytes + len);
	_inserted = true;
	// Reset ATA/CCR state so a fresh attach doesn't inherit mid-sector
	// transfer state from a previous card.  This includes ataCommandCount:
	// it counts ATA commands for THIS card session, and the SA-1100
	// native-CF insert delivery uses "ataCommandCount > 0" as the
	// driver-responded success signal.  If it carried over from a prior
	// mount, a re-insert after an eject would see it already non-zero and
	// stop the card-detect re-pulse before the driver re-enumerates — the
	// card would never re-mount.  Zeroing it here makes each attach a clean
	// session (the CF_STATS ata_cmds metric is likewise per-session).
	resetAtaState();
}

void VCFCard::resetAtaState() {
	ataCommandCount = 0;
	ccrOption = ccrStatus = ccrPin = ccrCopy = 0;
	ataError = 0;
	ataFeatures = 0;
	ataSectorCount = 1;
	ataSectorNumber = ataCylLow = ataCylHigh = 0;
	ataDrive = 0xA0;
	ataStatus = 0x50;
	sectorBufferPos = sectorBufferFill = 0;
	ataLBA = 0;
	ataSectorsRemaining = 0;
	ataWriting = false;
	ataMultiSectorRead = false;
	ataBusyReads = 0;
	irqPending = false;
	irqDelayCycles = 0;
}

void VCFCard::detach() {
	_inserted = false;
}

uint8_t VCFCard::readByte(uint32_t offset) const {
	if (!_inserted || offset >= image.size())
		return 0xFF;
	return image[offset];
}

uint32_t VCFCard::readWord(uint32_t offset) const {
	if (!_inserted || (offset + 3) >= image.size())
		return 0xFFFFFFFF;
	return (uint32_t)image[offset]
		| ((uint32_t)image[offset + 1] << 8)
		| ((uint32_t)image[offset + 2] << 16)
		| ((uint32_t)image[offset + 3] << 24);
}

void VCFCard::writeByte(uint32_t offset, uint8_t value) {
	if (!_inserted || offset >= image.size()) return;
	image[offset] = value;
}

void VCFCard::writeWord(uint32_t offset, uint32_t value) {
	if (!_inserted || (offset + 3) >= image.size()) return;
	image[offset]     = (uint8_t)(value);
	image[offset + 1] = (uint8_t)(value >> 8);
	image[offset + 2] = (uint8_t)(value >> 16);
	image[offset + 3] = (uint8_t)(value >> 24);
}

uint8_t VCFCard::readAttributeByte(uint32_t offset) const {
	if (!_inserted) return 0xFF;
	// Odd bytes are unused on an 8-bit CIS (the AD0 line is tied low).
	if (offset & 1) return 0xFF;
	// CCR window: four registers at even offsets kCCRBase + 0/2/4/6.
	if (offset >= kCCRBase && offset < kCCRBase + 8) {
		switch (offset - kCCRBase) {
		case 0: return ccrOption;
		case 2: return ccrStatus;
		case 4: return ccrPin;
		case 6: return ccrCopy;
		}
		return 0xFF;
	}
	uint32_t idx = offset >> 1;
	if (idx >= sizeof(kCIS)) return 0xFF;
	return kCIS[idx];
}

void VCFCard::writeAttributeByte(uint32_t offset, uint8_t value) {
	if (!_inserted) return;
	if (offset & 1) return;
	if (offset >= kCCRBase && offset < kCCRBase + 8) {
		switch (offset - kCCRBase) {
		case 0: ccrOption = value; break;  // config index + IREQ + SRESET
		case 2: ccrStatus = value; break;
		case 4: ccrPin    = value; break;
		case 6: ccrCopy   = value; break;
		}
	}
	// CIS bytes below kCCRBase are read-only.
}

// ---- ATA task-file emulation -------------------------------------------

// Assemble the current LBA from the task-file registers. We only support
// LBA mode (bit 6 of Drive reg set); CHS mode predates CF by a decade and
// EPOC uses LBA on every ROM version we care about.
static uint32_t computeLBA(uint8_t secN, uint8_t cylLo, uint8_t cylHi, uint8_t drv) {
	return (uint32_t)secN
	     | ((uint32_t)cylLo << 8)
	     | ((uint32_t)cylHi << 16)
	     | ((uint32_t)(drv & 0x0F) << 24);
}

void VCFCard::ataStartCommand(uint8_t cmd) {
	ataError = 0;
	sectorBufferPos = 0;
	sectorBufferFill = 0;
	ataWriting = false;
	// A new command terminates any transfer still in progress (real ATA
	// semantics: writing the command register aborts an unfinished
	// READ/WRITE SECTORS).  netBook-only: the simpler polled drivers on other
	// devices relied on the prior leave-counter-alone behaviour, so gate it.
	if (faithfulMode_)
		ataSectorsRemaining = 0;
	ataCommandCount++;

	// Trace commands so the harness can see drive / sector-count / LBA at
	// the moment of command issue. Routes through the host's cycle-prefixed
	// logger when an owner is set.  Gated: this fires per ATA command (~1839
	// on a full OS.IMG read) and floods the log when a --log-file is set.
	if (std::getenv("PSION_CF_TRACE"))
	trace("CF ATA cmd=%02x drv=%02x secN=%02x cyl=%02x%02x cnt=%02x",
	      cmd, ataDrive, ataSectorNumber, ataCylHigh, ataCylLow, ataSectorCount);

	// PSION_CF_TRACE_CMD_LR=1 — dump the issuing PC/LR + a few stack words at
	// the moment a command register is written, so the working-mount call
	// chain (which initiates IDENTIFY) can be walked.  Env-gated diagnostic;
	// off by default, no effect on any device's normal path.
	if (owner && std::getenv("PSION_CF_TRACE_CMD_LR")) {
		owner->log("CF-CMD-LR cmd=%02x pc=%08x lr=%08x r0=%08x r1=%08x r2=%08x sp=%08x",
		           cmd, owner->getRealPC(), owner->getGPR(14),
		           owner->getGPR(0), owner->getGPR(1), owner->getGPR(2),
		           owner->getGPR(13));
		// One-shot stack walk at the first IDENTIFY: dump ROM-range return
		// addresses (0x50xxxxxx) found on the stack so the mount-initiation
		// call chain can be reconstructed (PSION_CF_TRACE_CMD_LR diagnostic).
		static bool walked = false;
		if (cmd == 0xEC && !walked) {
			walked = true;
			uint32_t sp = owner->getGPR(13);
			char buf[1024]; int n = 0;
			n += snprintf(buf + n, sizeof(buf) - n, "CF-CMD-LR stackwalk sp=%08x:", sp);
			for (uint32_t a = sp; a < sp + 0x200 && n < (int)sizeof(buf) - 16; a += 4) {
				uint32_t v = owner->readVirtualDebug(a, ARM710::V32).value_or(0);
				if (v >= 0x50000000u && v < 0x51000000u)
					n += snprintf(buf + n, sizeof(buf) - n, " %08x", v);
			}
			owner->log("%s", buf);
		}
		// PSION_CF_TRACE_CMD_LR: on READ SECTORS, walk the USR-banked stack for
		// F32-server/FSY return addresses (0x5006/0x5007/0x5008xxxx) so the
		// directory-read path that serves a cached mount (no CheckMount) can be
		// reconstructed.  Capped so a multi-sector read doesn't flood.
		if (cmd == 0x20 || cmd == 0x21) {
			static int rw = 0;
			if (rw < 24) { rw++;
				uint32_t usp = owner->getBankedSP(5);
				uint32_t ulr = owner->getBankedLR(5);
				char buf[900]; int n = 0;
				n += snprintf(buf + n, sizeof(buf) - n,
				              "CF-CMD-LR READ uLR=%08x uSP=%08x f32stk:", ulr, usp);
				for (uint32_t w = 0; w < 0x800u && usp >= 0x1000u && n < (int)sizeof(buf) - 20; w += 4) {
					uint32_t v = owner->readVirtualDebug(usp + w, ARM710::V32).value_or(0);
					if (v >= 0x50060000u && v < 0x50090000u)
						n += snprintf(buf + n, sizeof(buf) - n, " +%03x=%08x", w, v);
				}
				owner->log("%s", buf);
			}
		}
	}

	// Real ATA/CF asserts BSY the instant the command register is written.
	// Hold it for the next Status read(s) so the netBook medata "wait while
	// busy" transfer routine (0x500a81d0) catches the busy->ready edge it
	// requires; see ataBusyReads in vcfcard.h.  netBook-only: other devices'
	// CF drivers don't expect BSY here and report the card "corrupt".
	ataBusyReads = faithfulMode_ ? 1 : 0;

	// Default: this command is not a multi-sector read.  Set true only in the
	// READ SECTORS case below when the sector count is not 1.
	ataMultiSectorRead = false;

	switch (cmd) {
	case 0xEC: // IDENTIFY DEVICE
		ataFillIdentify();
		sectorBufferFill = 512;
		ataStatus = 0x58; // DRDY | DSC | DRQ (data ready to transfer)
		irqPending = true;
		irqDelayCycles = kIrqAssertDelay;
		break;
	case 0x20: // READ SECTORS (with retry)
	case 0x21: // READ SECTORS (no retry)
		ataLBA = computeLBA(ataSectorNumber, ataCylLow, ataCylHigh, ataDrive);
		ataSectorsRemaining = ataSectorCount ? ataSectorCount : 256;
		// cnt != 1 → multi-sector read → medata's async/interrupt path.
		ataMultiSectorRead = (ataSectorsRemaining != 1);
		ataLoadNextSector();
		break;
	case 0x30: // WRITE SECTORS (with retry)
	case 0x31: // WRITE SECTORS (no retry)
		ataLBA = computeLBA(ataSectorNumber, ataCylLow, ataCylHigh, ataDrive);
		ataSectorsRemaining = ataSectorCount ? ataSectorCount : 256;
		ataWriting = true;
		sectorBufferFill = 512;
		sectorBufferPos = 0;
		ataStatus = 0x58; // ready for host to push data
		// NO IRQ here.  On real ATA/CF a WRITE SECTORS command does NOT raise
		// IREQ# — the device just asserts DRQ ("send me the data") and the host
		// pushes the sector; IREQ# is asserted only AFTER the sector data has
		// been received and written (per-sector completion), which
		// ataFlushSector() does.  Asserting IREQ# here is a spurious
		// "command-complete" interrupt before any data has been transferred.
		// EPOC's pccd_ata write path issues the command, waits for not-busy,
		// reads Status to "clear any pending interrupt", THEN enables the card
		// IRQ and pushes the data — so a stale post-command IREQ that survives
		// that ack (e.g. under faithfulMode's deferred-ack timing) fires the
		// instant interrupts are enabled, before the data is sent, and the
		// driver aborts the write → "Disk corrupt" on copy/format.  Leaving the
		// IRQ to ataFlushSector keeps the single, correct completion interrupt.
		break;
	case 0x91: // INITIALIZE DEVICE PARAMETERS (no-op for LBA)
	case 0xE0: // STANDBY IMMEDIATE
	case 0xE1: // IDLE IMMEDIATE
	case 0xE7: // FLUSH CACHE
	case 0xEF: // SET FEATURES
	case 0xC6: // SET MULTIPLE MODE
		ataStatus = 0x50; // DRDY | DSC (done, no data)
		irqPending = true;
		irqDelayCycles = kIrqAssertDelay;
		break;
	default:
		ataError = 0x04; // ABRT
		ataStatus = 0x51; // DRDY | ERR
		irqPending = true;
		irqDelayCycles = kIrqAssertDelay;
		break;
	}
}

// Populate sectorBuffer with a 512-byte IDENTIFY DEVICE response. The
// protocol's data is a 256-word (little-endian) block. Only a handful of
// words matter for a FAT mount: the geometry, the LBA-capacity, and the
// feature flags in word 49 / 53 / 88. Strings are byte-swapped per the
// ATA spec.
void VCFCard::ataFillIdentify() {
	uint16_t id[256] = {};
	uint32_t totalSectors = (uint32_t)(image.size() / 512);
	// Minimum sensible CHS geometry: 4 heads, 32 s/t. Total sectors must
	// fit below 2^28 for LBA28, which our 16MB image comfortably does.
	uint16_t heads = 4;
	uint16_t spt = 32;
	uint16_t cyls = (uint16_t)((totalSectors + heads * spt - 1) / (heads * spt));
	if (cyls > 16383) cyls = 16383;

	id[0]  = 0x848A;             // General config: non-magnetic, removable, hard sectored
	id[1]  = cyls;
	id[3]  = heads;
	id[6]  = spt;
	id[22] = 4;                  // obsolete bytes available on READ/WRITE LONG
	id[47] = 0x8001;             // Max sectors per multi-sector cmd = 1
	id[49] = 0x0200;             // LBA supported (bit 9)
	id[51] = 0x0200;             // PIO mode 2
	id[53] = 0x0007;             // Fields in words 54-58, 64-70, 88 are valid
	id[54] = cyls;
	id[55] = heads;
	id[56] = spt;
	id[57] = (uint16_t)(totalSectors);
	id[58] = (uint16_t)(totalSectors >> 16);
	id[59] = 0x0101;             // Multi-sector setting valid, 1 sector
	id[60] = (uint16_t)(totalSectors);
	id[61] = (uint16_t)(totalSectors >> 16);
	id[64] = 0x0003;             // PIO modes 3,4 supported
	id[80] = 0x007E;             // ATA-1..6 supported
	id[82] = 0x4000;             // NOP supported
	id[83] = 0x4000;             // Reserved bit
	id[84] = 0x4000;
	id[85] = 0x4000;
	id[86] = 0x0000;
	id[87] = 0x4000;
	id[88] = 0x0000;             // UDMA unsupported

	auto putString = [&](int wordStart, int nWords, const char *s) {
		size_t n = strlen(s);
		for (int i = 0; i < nWords; i++) {
			char a = (i * 2 < (int)n) ? s[i * 2] : ' ';
			char b = (i * 2 + 1 < (int)n) ? s[i * 2 + 1] : ' ';
			id[wordStart + i] = (uint16_t)((uint8_t)a << 8) | (uint8_t)b;
		}
	};
	putString(10, 10, "PSION0001           ");        // Serial (20 chars)
	putString(23, 4,  "1.00");                         // Firmware rev (8 chars)
	putString(27, 20, "PSION VIRTCF                            "); // Model (40 chars)

	for (int i = 0; i < 256; i++) {
		sectorBuffer[i * 2]     = (uint8_t)(id[i]);
		sectorBuffer[i * 2 + 1] = (uint8_t)(id[i] >> 8);
	}
}

void VCFCard::ataLoadNextSector() {
	uint64_t base = (uint64_t)ataLBA * 512ULL;
	if (base + 512 > image.size()) {
		ataError = 0x10; // IDNF (ID not found)
		ataStatus = 0x51; // DRDY | ERR
		sectorBufferFill = 0;
		ataSectorsRemaining = 0;
		irqPending = true;
		irqDelayCycles = kIrqAssertDelay;
		return;
	}
	memcpy(sectorBuffer, image.data() + base, 512);
	if (std::getenv("PSION_CF_TRACE")) {
		trace("CF loadSector LBA=%u (offset=%llu) bytes[0..15]=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		      ataLBA, (unsigned long long)base,
		      sectorBuffer[0], sectorBuffer[1], sectorBuffer[2], sectorBuffer[3],
		      sectorBuffer[4], sectorBuffer[5], sectorBuffer[6], sectorBuffer[7],
		      sectorBuffer[8], sectorBuffer[9], sectorBuffer[10], sectorBuffer[11],
		      sectorBuffer[12], sectorBuffer[13], sectorBuffer[14], sectorBuffer[15]);
	}
	sectorBufferFill = 512;
	sectorBufferPos = 0;
	ataStatus = 0x58; // DRDY | DSC | DRQ
	// Data ready: assert IREQ# so the host doesn't fall back to polling.
	irqPending = true;
	irqDelayCycles = kIrqAssertDelay;
}

void VCFCard::ataFlushSector() {
	uint64_t base = (uint64_t)ataLBA * 512ULL;
	if (base + 512 > image.size()) {
		ataError = 0x10; // IDNF
		ataStatus = 0x51;
		irqPending = true;
		irqDelayCycles = kIrqAssertDelay;
		return;
	}
	memcpy(image.data() + base, sectorBuffer, 512);
	ataLBA++;
	ataSectorsRemaining--;
	if (ataSectorsRemaining == 0) {
		ataStatus = 0x50; // DRDY | DSC, command complete
		sectorBufferFill = 0;
		ataWriting = false;
	} else {
		sectorBufferPos = 0;
		sectorBufferFill = 512;
		ataStatus = 0x58; // ready for next sector's worth of data
	}
	// Sector flushed (or write complete / error): host needs to be told.
	irqPending = true;
	irqDelayCycles = kIrqAssertDelay;
}

uint8_t VCFCard::ataRead8(uint32_t offset) {
	if (!_inserted) return 0xFF;
	// CF "Contiguous I/O" layout (16 bytes). See eka/drivers/medata/ata.h.
	switch (offset & 0xF) {
	case 0x0: {
		// Data register: 16-bit on the bus, but EPOC's Osaris driver
		// drains it via byte-by-byte ldrb (verified at Osaris
		// 0x5000892c). Return one byte per call, advance the sector
		// buffer by 1, and emit the same sector-drain bookkeeping that
		// ataRead16() does once we cross a 16-bit boundary.
		if (sectorBufferPos >= sectorBufferFill) return 0xFF;
		uint8_t b = sectorBuffer[sectorBufferPos++];
		if (sectorBufferPos >= sectorBufferFill) {
			if (!ataWriting) {
				sectorBoundaryCount++;
				if (std::getenv("PSION_CF_TRACE"))
				trace("CF sector drained (byte mode): lba=%u remaining=%u",
				      ataLBA, ataSectorsRemaining);
				if (ataSectorsRemaining > 1) {
					ataSectorsRemaining--;
					ataLBA++;
					ataLoadNextSector();
				} else {
					ataSectorsRemaining = 0;
					ataStatus = 0x50;
					sectorBufferFill = 0;
					// netBook only: assert a command-complete IRQ so medata
					// finalises the request after the last sector of a
					// multi-sector read.  Other devices' drivers didn't get
					// this IRQ before and don't expect it.
					if (faithfulMode_) {
						irqPending = true;
						irqDelayCycles = kIrqAssertDelay;
					}
				}
			}
		}
		return b;
	}
	case 0x1: return ataError;
	case 0x2: return ataSectorCount;
	case 0x3: return ataSectorNumber;
	case 0x4: return ataCylLow;
	case 0x5: return ataCylHigh;
	case 0x6: return ataDrive;
	case 0x7: {
		// Reading the primary Status register clears IREQ# on real CF —
		// but only once IREQ has actually asserted.  Drivers poll the
		// Status register right after issuing a command (e.g. medata's
		// BSY poll at 0x500a8acc) while the card is still "busy"
		// (irqDelayCycles > 0); clearing the pending IREQ there would drop
		// the data-ready interrupt the driver then waits for, hanging the
		// read.  Only ack (clear) when the IREQ is genuinely asserted.
		// While the post-command BSY transient is active, report BSY set
		// (bit 7) and DON'T expose DRQ yet — medata's transfer routine waits
		// for this busy->ready edge before draining the data register.
		ataStatusReadCount++;
		if (ataBusyReads > 0) {
			ataBusyReads--;
			return (uint8_t)(0x80 | (ataStatus & ~0x08)); // BSY, DRQ masked
		}
		uint8_t s = ataStatus;
		// netBook (faithful) defers the IRQ-ack until the IREQ has actually
		// asserted (irqDelayCycles==0) so a busy-poll right after the command
		// doesn't drop the data-ready interrupt.  Other devices keep the
		// original unconditional ack-on-Status-read semantics.
		if (faithfulMode_) {
			if (irqDelayCycles == 0) irqPending = false;
		} else {
			irqPending = false;
		}
		return s;
	}
	case 0xD: return ataError;  // duplicate error reg
	case 0xE: // alt-status mirrors status (incl. the BSY transient) without IRQ ack
		return ataBusyReads > 0 ? (uint8_t)(0x80 | (ataStatus & ~0x08)) : ataStatus;
	case 0xF: return 0x01;      // drive address: drive 0 selected, head 0
	}
	return 0xFF;
}

void VCFCard::ataWrite8(uint32_t offset, uint8_t value) {
	if (!_inserted) return;
	switch (offset & 0xF) {
	case 0x0:
		// Data register: 16-bit on the bus, but EPOC's Osaris driver
		// fills it via byte-by-byte strb (mirrors the byte-mode drain
		// path on read at 0x5000892c). Accept one byte per call into
		// the sector buffer and flush when full — same accounting as
		// ataWrite16().
		if (!ataWriting || sectorBufferPos >= 512) return;
		sectorBuffer[sectorBufferPos++] = value;
		if (sectorBufferPos >= sectorBufferFill)
			ataFlushSector();
		return;
	case 0x1: ataFeatures = value; break;
	case 0x2: ataSectorCount = value; break;
	case 0x3: ataSectorNumber = value; break;
	case 0x4: ataCylLow = value; break;
	case 0x5: ataCylHigh = value; break;
	case 0x6: ataDrive = value; break;
	case 0x7: ataStartCommand(value); break;
	case 0xD: ataFeatures = value; break; // duplicate features reg
	case 0xE: break;                      // device control (nIEN / SRST): no-op
	}
}

uint16_t VCFCard::ataRead16() {
	if (!_inserted || sectorBufferPos >= sectorBufferFill) return 0xFFFF;
	uint16_t v = (uint16_t)sectorBuffer[sectorBufferPos]
	           | ((uint16_t)sectorBuffer[sectorBufferPos + 1] << 8);
	sectorBufferPos += 2;
	if (sectorBufferPos >= sectorBufferFill) {
		if (!ataWriting) {
			sectorBoundaryCount++;
			if (std::getenv("PSION_CF_TRACE"))
			trace("CF sector drained: lba=%u remaining=%u status->%s",
			      ataLBA, ataSectorsRemaining,
			      ataSectorsRemaining > 1 ? "next sector" : "complete");
			if (ataSectorsRemaining > 1) {
				ataSectorsRemaining--;
				ataLBA++;
				ataLoadNextSector();
			} else {
				ataSectorsRemaining = 0;
				ataStatus = 0x50; // command complete
				sectorBufferFill = 0;
			}
		}
	}
	return v;
}

void VCFCard::ataWrite16(uint16_t value) {
	if (!_inserted || !ataWriting) return;
	if (sectorBufferPos + 1 >= 512) return;
	sectorBuffer[sectorBufferPos]     = (uint8_t)value;
	sectorBuffer[sectorBufferPos + 1] = (uint8_t)(value >> 8);
	sectorBufferPos += 2;
	if (sectorBufferPos >= sectorBufferFill)
		ataFlushSector();
}
