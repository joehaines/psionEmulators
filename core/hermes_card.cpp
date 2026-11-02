// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "hermes_card.h"
#include <algorithm>
#include <cstring>

namespace {

// Register offsets (drivers/net/wireless/hermes.h, 16-bit spacing).
enum : uint32_t {
	R_CMD = 0x00, R_PARAM0 = 0x02, R_PARAM1 = 0x04, R_PARAM2 = 0x06, R_STATUS = 0x08,
	R_RESP0 = 0x0a, R_RESP1 = 0x0c, R_RESP2 = 0x0e, R_INFOFID = 0x10, R_CONTROL = 0x14,
	R_SELECT0 = 0x18, R_SELECT1 = 0x1a, R_OFFSET0 = 0x1c, R_OFFSET1 = 0x1e,
	R_RXFID = 0x20, R_ALLOCFID = 0x22, R_TXCOMPLFID = 0x24,
	R_SWSUPPORT0 = 0x28, R_SWSUPPORT1 = 0x2a, R_SWSUPPORT2 = 0x2c,
	R_EVSTAT = 0x30, R_INTEN = 0x32, R_EVACK = 0x34, R_DATA0 = 0x36, R_DATA1 = 0x38,
	R_AUXPAGE = 0x3a, R_AUXOFFSET = 0x3c, R_AUXDATA = 0x3e,
};

enum : uint16_t {
	EV_RX = 0x0001, EV_TX = 0x0002, EV_TXEXC = 0x0004, EV_ALLOC = 0x0008, EV_CMD = 0x0010,
	EV_INFO = 0x0080, EV_INFDROP = 0x2000, EV_TICK = 0x8000,
};

enum : uint16_t {
	CMD_INIT = 0x00, CMD_ENABLE = 0x01, CMD_DISABLE = 0x02, CMD_ALLOC = 0x0a, CMD_TX = 0x0b,
	CMD_INQUIRE = 0x11, CMD_ACCESS = 0x21,
};

enum : uint16_t {
	INQ_TALLIES = 0xf100, INQ_LINKSTATUS = 0xf200,
	RID_CNFPORTTYPE = 0xfc00, RID_CNFOWNMACADDR = 0xfc01, RID_CNFDESIREDSSID = 0xfc02,
	RID_CNFOWNCHANNEL = 0xfc03, RID_CNFOWNSSID = 0xfc04, RID_CNFSYSTEMSCALE = 0xfc06,
	RID_CNFMAXDATALEN = 0xfc07, RID_CNFMAXSLEEPDURATION = 0xfc0c, RID_CNFPMHOLDOVERDURATION = 0xfc0d,
	RID_CNFOWNNAME = 0xfc0e, RID_CNFRTSTHRESHOLD = 0xfc83, RID_CNFFRAGTHRESHOLD = 0xfc82,
	RID_CNFTXRATECONTROL = 0xfc84,
	RID_NICID = 0xfd0b, RID_CHANNELLIST = 0xfd10, RID_STAID = 0xfd20, RID_PRIID = 0xfd02,
	RID_PORTSTATUS = 0xfd40, RID_CURRENTSSID = 0xfd41, RID_CURRENTBSSID = 0xfd42,
	RID_COMMSQUALITY = 0xfd43, RID_CURRENTTXRATE = 0xfd44, RID_CURRENTBEACONINTERVAL = 0xfd45,
	RID_DBMCOMMSQUALITY = 0xfd51, RID_OWNMACADDR = 0xfd86, RID_CURRENTCHANNEL = 0xfdc1,
	RID_SUPPORTEDDATARATES = 0xfdc6,
};

// The access point's BSSID, and the channel it is on.
constexpr uint8_t kBssid[6] = { 0x00, 0x60, 0x1d, 0x50, 0x53, 0x4e };
constexpr uint16_t kChannel = 6;

// Card Information Structure: an Intersil PRISM 2 PC Card as cardmgr's
// config knows it (manfid 0x0156, 0x0002, bound to orinoco_cs), a network
// function, configuration registers at 0x3e0, and one configuration: 64
// bytes of 16-bit I/O with a level interrupt.
std::vector<uint8_t> buildCis() {
	std::vector<uint8_t> c;
	auto tuple = [&](uint8_t code, std::initializer_list<uint8_t> body) {
		c.push_back(code); c.push_back((uint8_t)body.size()); c.insert(c.end(), body.begin(), body.end());
	};
	tuple(0x01, { 0x00, 0x00, 0xff });                              // CISTPL_DEVICE: no common memory
	std::vector<uint8_t> v = { 0x05, 0x00 };                        // CISTPL_VERS_1, 5.0
	for (const char *str : { "Intersil", "PRISM2 IEEE 802.11 PC-Card", "Version 01.02" }) {
		v.insert(v.end(), str, str + std::strlen(str));
		v.push_back(0);
	}
	v.push_back(0xff);
	c.push_back(0x15); c.push_back((uint8_t)v.size()); c.insert(c.end(), v.begin(), v.end());
	tuple(0x20, { 0x56, 0x01, 0x02, 0x00 });                        // CISTPL_MANFID 0x0156, 0x0002
	tuple(0x21, { 0x06, 0x00 });                                    // CISTPL_FUNCID: network adapter
	tuple(0x1a, { 0x01, 0x01, 0xe0, 0x03, 0x0f });                  // CISTPL_CONFIG: last index 1, base 0x3e0
	tuple(0x1b, { 0xc1, 0x01, 0x18, 0x46, 0x20 });                  // CFTABLE_ENTRY 1: 64 B 16-bit I/O, level IRQ
	tuple(0x14, {});                                                // CISTPL_NO_LINK
	c.push_back(0xff);
	return c;
}
const std::vector<uint8_t> kCis = buildCis();
constexpr uint32_t kCorBase = 0x3e0;

inline uint16_t rd16(const std::vector<uint8_t> &b, size_t o) {
	if (o + 1 < b.size()) return (uint16_t)(b[o] | (b[o + 1] << 8));
	return o < b.size() ? b[o] : 0;              // the odd byte at the end of a frame
}
inline void wr16(std::vector<uint8_t> &b, size_t o, uint16_t v) {
	if (o + 1 >= b.size()) b.resize(o + 2, 0);
	b[o] = (uint8_t)v;
	b[o + 1] = (uint8_t)(v >> 8);
}

} // namespace

HermesCard::HermesCard() {
	// A locally administered address in the Intersil OUI's spirit.
	static const uint8_t kMac[6] = { 0x02, 0x50, 0x53, 0x4c, 0x58, 0x01 };
	std::memcpy(mac_, kMac, 6);
	reset();
}

// ── Attribute memory ────────────────────────────────────────────────────

uint32_t HermesCard::attrRead(uint32_t off, int size) {
	uint32_t v = 0xff;
	if (off & 1) v = 0xff;
	else if (off >= kCorBase && off < kCorBase + 8) {
		switch (off - kCorBase) {
		case 0: v = cor_; break;
		case 2: v = ccsr_; break;
		default: v = 0; break;
		}
	} else if (off / 2 < kCis.size()) v = kCis[off / 2];
	return size == 1 ? v : (v | 0xff00u);
}

void HermesCard::attrWrite(uint32_t off, int size, uint32_t v) {
	(void)size;
	if (off == kCorBase) {
		if (v & 0x80) { reset(); return; }               // SRESET
		cor_ = (uint8_t)(v & 0x7f);
		enabled_ = (cor_ & 0x3f) != 0;
	} else if (off == kCorBase + 2) {
		ccsr_ = (uint8_t)v;
	}
}

// ── Reset and configuration records ────────────────────────────────────

void HermesCard::reset() {
	cor_ = ccsr_ = 0;
	enabled_ = false;
	std::memset(param_, 0, sizeof(param_));
	std::memset(resp_, 0, sizeof(resp_));
	std::memset(swsupport_, 0, sizeof(swsupport_));
	status_ = evstat_ = inten_ = 0;
	infofid_ = rxfid_ = allocfid_ = txcomplfid_ = 0;
	control_ = auxpage_ = auxoffset_ = 0;
	bap_[0] = bap_[1] = Bap();
	fids_.clear();
	ridIo_.clear();
	pending_.clear();
	rxQueue_.clear();
	infoQueue_.clear();
	rxFreeAt_ = 0;
	portEnabled_ = linked_ = false;
	linkAt_ = UINT64_MAX;
	initRids();
}

void HermesCard::setRid(uint16_t rid, std::initializer_list<uint16_t> data) {
	std::vector<uint16_t> r;
	r.push_back((uint16_t)(data.size() + 1));
	r.push_back(rid);
	r.insert(r.end(), data.begin(), data.end());
	rids_[rid] = r;
}

void HermesCard::setStringRid(uint16_t rid, const char *s) {
	// An ID string: a length word, then the bytes, padded to 34 bytes.
	size_t n = std::strlen(s);
	std::vector<uint16_t> r = { 18, rid, (uint16_t)n };
	for (size_t i = 0; i < 32; i += 2) {
		uint8_t a = i < n ? (uint8_t)s[i] : 0, b = i + 1 < n ? (uint8_t)s[i + 1] : 0;
		r.push_back((uint16_t)(a | (b << 8)));
	}
	rids_[rid] = r;
}

void HermesCard::initRids() {
	rids_.clear();
	setRid(RID_CNFOWNMACADDR, { (uint16_t)(mac_[0] | mac_[1] << 8), (uint16_t)(mac_[2] | mac_[3] << 8),
	                            (uint16_t)(mac_[4] | mac_[5] << 8) });
	setRid(RID_OWNMACADDR, { (uint16_t)(mac_[0] | mac_[1] << 8), (uint16_t)(mac_[2] | mac_[3] << 8),
	                         (uint16_t)(mac_[4] | mac_[5] << 8) });
	setWordRid(RID_CNFPORTTYPE, 1);
	setStringRid(RID_CNFDESIREDSSID, "");
	setStringRid(RID_CNFOWNSSID, "");
	setStringRid(RID_CNFOWNNAME, "Prism  I");
	setWordRid(RID_CNFOWNCHANNEL, 3);
	setWordRid(RID_CNFSYSTEMSCALE, 1);
	setWordRid(RID_CNFMAXDATALEN, 2312);
	setWordRid(RID_CNFMAXSLEEPDURATION, 100);
	setWordRid(RID_CNFPMHOLDOVERDURATION, 100);
	setWordRid(RID_CNFRTSTHRESHOLD, 2347);
	setWordRid(RID_CNFFRAGTHRESHOLD, 2346);
	setWordRid(RID_CNFTXRATECONTROL, 15);
	setWordRid(RID_CHANNELLIST, 0x07ff);                            // channels 1-11
	// Station firmware identity: Intersil 1.4.9 (id, variant, major, minor).
	setRid(RID_STAID, { 0x001f, 9, 1, 4 });
	setRid(RID_PRIID, { 0x0015, 1, 1, 1 });
	setRid(RID_NICID, { 0x8013, 1, 1, 0 });                         // PRISM 2.5
	setWordRid(RID_CURRENTBEACONINTERVAL, 100);
	// Supported rates: 1, 2, 5.5 and 11 Mb/s, in 500 kb/s units.
	setRid(RID_SUPPORTEDDATARATES, { 4, 0x0402, 0x160b, 0, 0, 0, 0, 0, 0, 0, 0 });
}

bool HermesCard::readRid(uint16_t rid, std::vector<uint16_t> &out) {
	// Records the MAC keeps current itself.
	switch (rid) {
	case RID_PORTSTATUS:                       // 1 disabled, 2 searching, 4 associated
		out = { 2, rid, (uint16_t)(!portEnabled_ ? 1 : linked_ ? 4 : 2) };
		return true;
	case RID_CURRENTSSID: {
		const char *s = linked_ ? kSsid : "";
		setStringRid(rid, s);
		out = rids_[rid];
		return true;
	}
	case RID_CURRENTBSSID:
		if (linked_) out = { 4, rid, (uint16_t)(kBssid[0] | kBssid[1] << 8), (uint16_t)(kBssid[2] | kBssid[3] << 8),
		                     (uint16_t)(kBssid[4] | kBssid[5] << 8) };
		else out = { 4, rid, 0, 0, 0 };
		return true;
	case RID_COMMSQUALITY:                     // quality, signal, noise (+0x95 = dBm)
		if (linked_) out = { 4, rid, 60, (uint16_t)(0x95 - 42), (uint16_t)(0x95 - 95) };
		else out = { 4, rid, 0, 0, 0 };
		return true;
	case RID_DBMCOMMSQUALITY:
		out = { 4, rid, (uint16_t)(linked_ ? 60 : 0), (uint16_t)(int16_t)-42, (uint16_t)(int16_t)-95 };
		return true;
	case RID_CURRENTTXRATE:                    // Intersil rate bits: 8 = 11 Mb/s
		out = { 2, rid, (uint16_t)(linked_ ? 8 : 0) };
		return true;
	case RID_CURRENTCHANNEL:
		out = { 2, rid, (uint16_t)(portEnabled_ ? kChannel : 0) };
		return true;
	default: {
		auto it = rids_.find(rid);
		if (it == rids_.end()) return false;
		out = it->second;
		return true;
	}
	}
}

// ── Buffers ─────────────────────────────────────────────────────────────

uint16_t HermesCard::allocFid(size_t bytes) {
	for (int guard = 0; guard < 0x1000; guard++) {
		uint16_t f = nextFid_;
		nextFid_ = (uint16_t)(nextFid_ >= 0x0ff0 ? 0x0100 : nextFid_ + 0x10);
		if (!fids_.count(f)) {
			fids_[f].assign(std::max<size_t>(bytes, 2), 0);
			return f;
		}
	}
	return 0;
}

std::vector<uint8_t> *HermesCard::bapBuffer(int b) {
	uint16_t sel = bap_[b].select;
	if (sel >= 0xfc00) return &ridIo_[sel];
	auto it = fids_.find(sel);
	return it == fids_.end() ? nullptr : &it->second;
}

// ── Registers ───────────────────────────────────────────────────────────

uint32_t HermesCard::ioRead(uint32_t off, int size) {
	off &= 0x3e;
	if (size == 4) return ioRead(off, 2) | (ioRead(off + 2, 2) << 16);
	uint16_t v = 0;
	switch (off) {
	case R_CMD: v = 0; break;                                   // never busy
	case R_PARAM0: case R_PARAM1: case R_PARAM2: v = param_[(off - R_PARAM0) / 2]; break;
	case R_STATUS: v = status_; break;
	case R_RESP0: case R_RESP1: case R_RESP2: v = resp_[(off - R_RESP0) / 2]; break;
	case R_INFOFID: v = infofid_; break;
	case R_CONTROL: v = control_; break;
	case R_SELECT0: v = bap_[0].select; break;
	case R_SELECT1: v = bap_[1].select; break;
	case R_OFFSET0: case R_OFFSET1: {
		const Bap &b = bap_[off == R_OFFSET1];
		v = (uint16_t)((b.offset & 0x0ffe) | (b.err ? 0x4000 : 0));
		break;
	}
	case R_RXFID: v = rxfid_; break;
	case R_ALLOCFID: v = allocfid_; break;
	case R_TXCOMPLFID: v = txcomplfid_; break;
	case R_SWSUPPORT0: case R_SWSUPPORT1: case R_SWSUPPORT2: v = swsupport_[(off - R_SWSUPPORT0) / 2]; break;
	case R_EVSTAT: v = evstat_; break;
	case R_INTEN: v = inten_; break;
	case R_DATA0: case R_DATA1: {
		int b = off == R_DATA1;
		std::vector<uint8_t> *buf = bapBuffer(b);
		if (buf) v = rd16(*buf, bap_[b].offset);
		bap_[b].offset = (uint16_t)(bap_[b].offset + 2);
		break;
	}
	case R_AUXPAGE: v = auxpage_; break;
	case R_AUXOFFSET: v = auxoffset_; break;
	default: v = 0; break;
	}
	return size == 1 ? (v & 0xff) : v;
}

void HermesCard::ioWrite(uint32_t off, int size, uint32_t val) {
	off &= 0x3e;
	if (size == 4) { ioWrite(off, 2, val & 0xffff); ioWrite(off + 2, 2, val >> 16); return; }
	uint16_t v = (uint16_t)val;
	switch (off) {
	case R_CMD: command(v); break;
	case R_PARAM0: case R_PARAM1: case R_PARAM2: param_[(off - R_PARAM0) / 2] = v; break;
	case R_CONTROL: control_ = v; break;
	case R_SELECT0: bap_[0].select = v; break;
	case R_SELECT1: bap_[1].select = v; break;
	case R_OFFSET0: case R_OFFSET1: {
		Bap &b = bap_[off == R_OFFSET1];
		b.offset = (uint16_t)(v & 0x0ffe);
		// Seeking into a frame or record that does not exist is an error.
		b.err = !(b.select >= 0xfc00 || fids_.count(b.select));
		break;
	}
	case R_ALLOCFID: allocfid_ = v; break;
	case R_TXCOMPLFID: txcomplfid_ = v; break;
	case R_RXFID: rxfid_ = v; break;
	case R_INFOFID: infofid_ = v; break;
	case R_SWSUPPORT0: case R_SWSUPPORT1: case R_SWSUPPORT2: swsupport_[(off - R_SWSUPPORT0) / 2] = v; break;
	case R_INTEN: inten_ = v; break;
	case R_EVACK: {
		uint16_t acked = evstat_ & v;
		evstat_ &= (uint16_t)~v;
		// The host is done with the frame it was handed.
		if (acked & EV_RX) fids_.erase(rxfid_);
		if (acked & EV_INFO) fids_.erase(infofid_);
		break;
	}
	case R_DATA0: case R_DATA1: {
		int b = off == R_DATA1;
		if (std::vector<uint8_t> *buf = bapBuffer(b)) {
			if (bap_[b].offset + 2u <= 4096u) wr16(*buf, bap_[b].offset, v);
		}
		bap_[b].offset = (uint16_t)(bap_[b].offset + 2);
		break;
	}
	case R_AUXPAGE: auxpage_ = v; break;
	case R_AUXOFFSET: auxoffset_ = v; break;
	default: break;
	}
}

// ── Commands ────────────────────────────────────────────────────────────

void HermesCard::completeCommand(uint16_t cmd, uint16_t result) {
	status_ = (uint16_t)((result << 8) | (cmd & 0x3f));
	evstat_ |= EV_CMD;
}

void HermesCard::command(uint16_t cmd) {
	uint16_t code = cmd & 0x3f;
	resp_[0] = resp_[1] = resp_[2] = 0;
	switch (code) {
	case CMD_INIT: {
		// Firmware re-initialisation: every buffer and record back to its
		// power-on state, the link down.
		uint8_t cor = cor_, ccsr = ccsr_;
		reset();
		cor_ = cor; ccsr_ = ccsr;                  // the socket configuration survives
		enabled_ = (cor_ & 0x3f) != 0;
		completeCommand(cmd, 0);
		break;
	}
	case CMD_ENABLE:
		portEnabled_ = true;
		linked_ = false;
		linkAt_ = now_ + kCyclesPerSecond / 4;    // scanning, authenticating, associating
		completeCommand(cmd, 0);
		break;
	case CMD_DISABLE:
		if (linked_) queueInfo(INQ_LINKSTATUS, { 2 });   // disconnected
		portEnabled_ = linked_ = false;
		linkAt_ = UINT64_MAX;
		completeCommand(cmd, 0);
		break;
	case CMD_ALLOC: {
		uint16_t fid = allocFid(param_[0]);
		completeCommand(cmd, fid ? 0 : 1);
		if (fid) { allocfid_ = fid; evstat_ |= EV_ALLOC; }
		break;
	}
	case CMD_TX:
		completeCommand(cmd, 0);
		transmit(param_[0], (cmd & 0x0100) != 0);
		break;
	case CMD_INQUIRE:
		completeCommand(cmd, 0);
		if (param_[0] == INQ_TALLIES) {
			std::vector<uint16_t> t(23, 0);
			t[0] = (uint16_t)txUnicast_; t[1] = (uint16_t)txMulticast_;
			t[10] = (uint16_t)rxUnicast_; t[11] = (uint16_t)rxMulticast_;
			queueInfo(INQ_TALLIES, t);
		} else if (param_[0] == INQ_LINKSTATUS) {
			queueInfo(INQ_LINKSTATUS, { (uint16_t)(linked_ ? 1 : 0) });
		}
		break;
	case CMD_ACCESS: {
		uint16_t rid = param_[0];
		if (cmd & 0x0100) {
			// Write: the host has put the LTV record in the RID's buffer.
			std::vector<uint8_t> &io = ridIo_[rid];
			uint16_t len = rd16(io, 0);
			if (len == 0 || len > 1024) { completeCommand(cmd, 1); break; }
			std::vector<uint16_t> r;
			for (uint32_t i = 0; i <= len; i++) r.push_back(rd16(io, i * 2));
			r[1] = rid;
			rids_[rid] = r;
			completeCommand(cmd, 0);
		} else {
			std::vector<uint16_t> r;
			if (!readRid(rid, r)) { completeCommand(cmd, 1); break; }
			std::vector<uint8_t> &io = ridIo_[rid];
			io.assign(r.size() * 2, 0);
			for (size_t i = 0; i < r.size(); i++) wr16(io, i * 2, r[i]);
			completeCommand(cmd, 0);
		}
		break;
	}
	default:                                       // NOTIFY, DIAG, DOWNLOAD, TEST, ...
		completeCommand(cmd, 0);
		break;
	}
}

// ── Frames ──────────────────────────────────────────────────────────────

void HermesCard::transmit(uint16_t fid, bool reclaim) {
	auto it = fids_.find(fid);
	if (it == fids_.end()) return;
	std::vector<uint8_t> &b = it->second;
	uint16_t txControl = rd16(b, 12);
	bool ok = linked_ && b.size() >= 60;
	size_t airBytes = 64;
	if (ok) {
		// The 802.3 header the host left at offset 46, then its payload.
		uint16_t len = (uint16_t)((b[58] << 8) | b[59]);
		if (60u + len > b.size() || len > 2304) ok = false;
		else {
			std::vector<uint8_t> frame(b.begin() + 46, b.begin() + 58);   // destination, source
			const uint8_t *p = b.data() + 60;
			static const uint8_t kSnap[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
			if (len >= 8 && std::memcmp(p, kSnap, 6) == 0) {
				frame.insert(frame.end(), p + 6, p + len);                // EtherType and data
			} else {
				frame.push_back((uint8_t)(len >> 8));
				frame.push_back((uint8_t)len);
				frame.insert(frame.end(), p, p + len);
			}
			if (frame[0] & 1) txMulticast_++; else txUnicast_++;
			airBytes = 24 + 8 + frame.size();
			if (tx) tx(frame.data(), frame.size());
		}
	}
	// A frame takes a little air time; then the MAC reports it and, with
	// RECL, hands the buffer back as an allocation.
	uint64_t air = airTime(airBytes);
	if (ok) { if (txControl & 0x0002) raise(EV_TX, air, fid); }
	else { wr16(b, 0, 0x0004); if (txControl & 0x0004) raise(EV_TXEXC, air, fid); }
	if (reclaim) raise(EV_ALLOC, air + 200, fid);
}

void HermesCard::receive(const uint8_t *frame, size_t len) {
	if (!linked_ || len < 14 || len > 1514) return;
	if (rxQueue_.size() >= 64) return;             // the card's buffers are full
	const uint8_t *dst = frame, *src = frame + 6;
	size_t dataLen = len - 14;
	std::vector<uint8_t> f(60 + 8 + dataLen, 0);
	// Receive descriptor: an RFC 1042 frame, a good signal, 11 Mb/s.
	wr16(f, 0, 0x2000);
	f[10] = 0x95 - 95;                             // silence
	f[11] = 0x95 - 42;                             // signal
	f[12] = 110;                                   // rate, in 100 kb/s
	// 802.11 header: a data frame from the distribution system.
	wr16(f, 14, 0x0208);
	std::memcpy(&f[18], dst, 6);
	std::memcpy(&f[24], kBssid, 6);
	std::memcpy(&f[30], src, 6);
	wr16(f, 44, (uint16_t)(8 + dataLen));
	// 802.3 header and the LLC/SNAP encapsulation.
	std::memcpy(&f[46], dst, 6);
	std::memcpy(&f[52], src, 6);
	f[58] = (uint8_t)((8 + dataLen) >> 8);
	f[59] = (uint8_t)(8 + dataLen);
	static const uint8_t kSnap[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
	std::memcpy(&f[60], kSnap, 6);
	f[66] = frame[12];
	f[67] = frame[13];
	std::memcpy(&f[68], frame + 14, dataLen);
	if (dst[0] & 1) rxMulticast_++; else rxUnicast_++;
	rxQueue_.push_back(std::move(f));
}

void HermesCard::queueInfo(uint16_t type, std::initializer_list<uint16_t> words) {
	queueInfo(type, std::vector<uint16_t>(words));
}

void HermesCard::queueInfo(uint16_t type, const std::vector<uint16_t> &words) {
	if (infoQueue_.size() >= 16) { evstat_ |= EV_INFDROP; return; }
	std::vector<uint8_t> f((words.size() + 2) * 2);
	wr16(f, 0, (uint16_t)(words.size() + 1));
	wr16(f, 2, type);
	for (size_t i = 0; i < words.size(); i++) wr16(f, 4 + i * 2, words[i]);
	infoQueue_.push_back(std::move(f));
}

void HermesCard::raise(uint16_t ev, uint64_t delay, uint16_t fid) {
	pending_.push_back({ now_ + std::max<uint64_t>(delay, 1), ev, fid });
}

uint64_t HermesCard::airTime(size_t bytes) {
	// 192 us long preamble, the frame at 11 Mb/s, SIFS and an ACK: ~1.2 ms
	// for a full-sized frame.
	return kCyclesPerSecond * (192 + bytes * 8 / 11 + 10 + 106) / 1000000;
}

void HermesCard::deliverQueued() {
	// One received frame and one information record at a time: the next
	// goes in once the host has acknowledged the last and the radio has had
	// time to receive it.
	if (!(evstat_ & EV_RX) && !rxQueue_.empty() && now_ >= rxFreeAt_) {
		uint16_t fid = allocFid(0);
		if (fid) {
			rxFreeAt_ = now_ + airTime(rxQueue_.front().size());
			fids_[fid] = std::move(rxQueue_.front());
			rxQueue_.pop_front();
			rxfid_ = fid;
			evstat_ |= EV_RX;
		}
	}
	if (!(evstat_ & EV_INFO) && !infoQueue_.empty()) {
		uint16_t fid = allocFid(0);
		if (fid) {
			fids_[fid] = std::move(infoQueue_.front());
			infoQueue_.pop_front();
			infofid_ = fid;
			evstat_ |= EV_INFO;
		}
	}
}

uint64_t HermesCard::tick(uint64_t now) {
	now_ = now;
	if (now >= linkAt_) {
		linkAt_ = UINT64_MAX;
		if (portEnabled_) {
			linked_ = true;
			queueInfo(INQ_LINKSTATUS, { 1 });          // connected
		}
	}
	for (auto it = pending_.begin(); it != pending_.end();) {
		if (now >= it->at) {
			if (it->ev == EV_ALLOC) allocfid_ = it->fid;
			else if (it->ev & (EV_TX | EV_TXEXC)) txcomplfid_ = it->fid;
			evstat_ |= it->ev;
			it = pending_.erase(it);
		} else ++it;
	}
	deliverQueued();
	return nextEvent();
}

uint64_t HermesCard::nextEvent() const {
	uint64_t next = linkAt_;
	for (const Pending &p : pending_) next = std::min(next, p.at);
	if (!rxQueue_.empty() && !(evstat_ & EV_RX)) next = std::min(next, std::max(rxFreeAt_, now_ + 64));
	if (!infoQueue_.empty() && !(evstat_ & EV_INFO)) next = std::min(next, now_ + 64);
	return next;
}
