// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "pccard.h"
#include <deque>
#include <functional>
#include <map>
#include <vector>

// An 802.11b PC Card with an Intersil PRISM 2.5 (HFA384x) MAC running
// station firmware 1.4.9 — the "Hermes" programming model Linux's orinoco
// driver speaks:
//
//  - 64 bytes of 16-bit I/O registers: a command register with three
//    parameters and three responses, an event status / interrupt enable /
//    acknowledge triple, and two buffer access paths (BAPs), each a select /
//    offset pair and an auto-incrementing data port;
//  - frame buffers (FIDs) the host allocates and transmits, and the MAC
//    fills with received frames and information records;
//  - configuration records (RIDs) the host reads and writes by ACCESS.
//
// The radio side is a single access point the card associates with once
// its port is enabled: frames the host transmits leave as Ethernet frames
// through `tx`, and `receive` hands frames from the network to the host.
class HermesCard final : public PcCard {
public:
	HermesCard();

	// The network behind the access point.
	std::function<void(const uint8_t *frame, size_t len)> tx;
	void receive(const uint8_t *frame, size_t len);
	const uint8_t *mac() const { return mac_; }
	bool associated() const { return linked_; }
	// Name of the access point's network.
	static constexpr const char *kSsid = "PsionNet";
	static constexpr uint64_t kCyclesPerSecond = 200000000;   // the board's guest clock

	uint32_t attrRead(uint32_t off, int size) override;
	void attrWrite(uint32_t off, int size, uint32_t v) override;
	uint32_t ioRead(uint32_t off, int size) override;
	void ioWrite(uint32_t off, int size, uint32_t v) override;
	bool ireq() const override { return (evstat_ & inten_) != 0 && enabled_; }
	void reset() override;
	uint64_t tick(uint64_t now) override;
	uint64_t nextEvent() const override;

private:
	uint8_t mac_[6];
	uint8_t cor_ = 0, ccsr_ = 0;           // configuration option / status registers
	bool enabled_ = false;                 // COR config index set: I/O mode

	// Registers.
	uint16_t param_[3] = {}, status_ = 0, resp_[3] = {};
	uint16_t evstat_ = 0, inten_ = 0, swsupport_[3] = {};
	uint16_t infofid_ = 0, rxfid_ = 0, allocfid_ = 0, txcomplfid_ = 0;
	uint16_t control_ = 0, auxpage_ = 0, auxoffset_ = 0;
	struct Bap { uint16_t select = 0, offset = 0; bool err = false; } bap_[2];

	// Frame buffers, by FID; configuration records (as LTV word arrays:
	// length, type, data...) by RID.
	std::map<uint16_t, std::vector<uint8_t>> fids_;
	std::map<uint16_t, std::vector<uint16_t>> rids_;
	uint16_t nextFid_ = 0x0100;
	std::map<uint16_t, std::vector<uint8_t>> ridIo_;   // what a BAP selecting a RID addresses
	uint16_t allocFid(size_t bytes);
	std::vector<uint8_t> *bapBuffer(int b);

	void command(uint16_t cmd);
	void completeCommand(uint16_t cmd, uint16_t result);
	bool readRid(uint16_t rid, std::vector<uint16_t> &out);
	void setWordRid(uint16_t rid, uint16_t v) { rids_[rid] = { 2, rid, v }; }
	void setRid(uint16_t rid, std::initializer_list<uint16_t> data);
	void setStringRid(uint16_t rid, const char *s);
	void initRids();
	void transmit(uint16_t fid, bool reclaim);

	// Link state and event scheduling, in guest cycles.
	bool portEnabled_ = false, linked_ = false;
	uint64_t now_ = 0;
	uint64_t linkAt_ = UINT64_MAX;         // association completes
	struct Pending { uint64_t at; uint16_t ev; uint16_t fid; };
	std::deque<Pending> pending_;          // events waiting to be raised
	std::deque<std::vector<uint8_t>> rxQueue_;
	uint64_t rxFreeAt_ = 0;                // the medium is busy with the last frame until then
	static uint64_t airTime(size_t bytes); // at 11 Mb/s, with preamble and the ACK
	std::deque<std::vector<uint8_t>> infoQueue_;
	void raise(uint16_t ev, uint64_t delay, uint16_t fid = 0);
	void queueInfo(uint16_t type, std::initializer_list<uint16_t> words);
	void queueInfo(uint16_t type, const std::vector<uint16_t> &words);
	void deliverQueued();
	uint32_t txUnicast_ = 0, txMulticast_ = 0, rxUnicast_ = 0, rxMulticast_ = 0;
};
