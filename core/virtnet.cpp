// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "virtnet.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

bool traceOn() { static const bool on = std::getenv("PSION_NET_TRACE") != nullptr; return on; }

constexpr uint8_t kRouterMac[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
constexpr uint8_t kBroadcastMac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
constexpr uint16_t kOurMss = 1460;
constexpr uint32_t kLeaseSeconds = 86400;

enum : uint8_t { TCP_FIN = 0x01, TCP_SYN = 0x02, TCP_RST = 0x04, TCP_PSH = 0x08, TCP_ACK = 0x10 };
enum : uint8_t { PROTO_ICMP = 1, PROTO_TCP = 6, PROTO_UDP = 17 };

inline uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
inline uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
inline void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
inline void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
inline bool seqLt(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }
inline bool seqLe(uint32_t a, uint32_t b) { return (int32_t)(a - b) <= 0; }

uint32_t sumWords(const uint8_t *p, size_t len, uint32_t sum = 0) {
	for (size_t i = 0; i + 1 < len; i += 2) sum += (uint32_t)((p[i] << 8) | p[i + 1]);
	if (len & 1) sum += (uint32_t)(p[len - 1] << 8);
	return sum;
}
uint16_t fold(uint32_t sum) {
	while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
	return (uint16_t)~sum;
}
// TCP / UDP checksum over the pseudo-header and the segment.
uint16_t l4Checksum(uint32_t src, uint32_t dst, uint8_t proto, const uint8_t *seg, size_t len) {
	uint8_t ph[12];
	put32(ph, src); put32(ph + 4, dst); ph[8] = 0; ph[9] = proto; put16(ph + 10, (uint16_t)len);
	return fold(sumWords(seg, len, sumWords(ph, 12)));
}

} // namespace

void VirtualNetwork::reset() {
	for (auto &kv : tcp_) if (host_) host_->tcpClose(kv.first);
	tcp_.clear();
	leased_ = false;
	guestMacKnown_ = false;
}

void VirtualNetwork::dropFlows() {
	for (auto &kv : tcp_) {
		Tcp &c = kv.second;
		if (c.state != Tcp::Connecting) sendRst(c.remoteIp, c.remotePort, kGuestIp, c.guestPort, c.sndNxt, c.rcvNxt, true);
		else sendRst(c.remoteIp, c.remotePort, kGuestIp, c.guestPort, 0, c.rcvNxt, true);
		if (host_) host_->tcpClose(kv.first);
	}
	tcp_.clear();
}

// ── Frames in ───────────────────────────────────────────────────────────

void VirtualNetwork::fromGuest(const uint8_t *f, size_t len) {
	if (len < 14) return;
	std::memcpy(guestMac_, f + 6, 6);
	guestMacKnown_ = true;
	uint16_t type = be16(f + 12);
	if (type == 0x0806) handleArp(f + 14, len - 14);
	else if (type == 0x0800) handleIp(f + 14, len - 14);
}

void VirtualNetwork::handleArp(const uint8_t *p, size_t len) {
	if (len < 28 || be16(p) != 1 || be16(p + 2) != 0x0800 || be16(p + 6) != 1) return;
	uint32_t spa = be32(p + 14), tpa = be32(p + 24);
	// Proxy ARP for the whole subnet except the guest's own address (which it
	// probes to check nobody else has it).
	if ((tpa & kNetmask) != (kGuestIp & kNetmask) || tpa == kGuestIp || (tpa & 0xff) == 0 || (tpa & 0xff) == 0xff) return;
	uint8_t r[42];
	std::memcpy(r, p + 8, 6);
	std::memcpy(r + 6, kRouterMac, 6);
	put16(r + 12, 0x0806);
	uint8_t *a = r + 14;
	put16(a, 1); put16(a + 2, 0x0800); a[4] = 6; a[5] = 4; put16(a + 6, 2);
	std::memcpy(a + 8, kRouterMac, 6); put32(a + 14, tpa);
	std::memcpy(a + 18, p + 8, 6); put32(a + 24, spa);
	if (toGuest) toGuest(r, sizeof(r));
}

void VirtualNetwork::handleIp(const uint8_t *p, size_t len) {
	if (len < 20 || (p[0] >> 4) != 4) return;
	size_t ihl = (size_t)(p[0] & 15) * 4;
	size_t total = be16(p + 2);
	if (ihl < 20 || total < ihl || total > len) return;
	if (be16(p + 6) & 0x3fff) return;              // fragments: not reassembled
	uint32_t src = be32(p + 12), dst = be32(p + 16);
	const uint8_t *body = p + ihl;
	size_t blen = total - ihl;
	switch (p[9]) {
	case PROTO_UDP: handleUdp(src, dst, body, blen); break;
	case PROTO_TCP: handleTcp(src, dst, body, blen); break;
	case PROTO_ICMP: handleIcmp(src, dst, body, blen); break;
	default: break;
	}
}

// ── IP out ──────────────────────────────────────────────────────────────

void VirtualNetwork::sendIp(uint32_t src, uint32_t dst, uint8_t proto, const std::vector<uint8_t> &payload, bool broadcast) {
	if (!toGuest || (!guestMacKnown_ && !broadcast)) return;
	std::vector<uint8_t> f(14 + 20 + payload.size());
	std::memcpy(f.data(), broadcast ? kBroadcastMac : guestMac_, 6);
	std::memcpy(f.data() + 6, kRouterMac, 6);
	put16(f.data() + 12, 0x0800);
	uint8_t *ip = f.data() + 14;
	ip[0] = 0x45; ip[1] = 0;
	put16(ip + 2, (uint16_t)(20 + payload.size()));
	put16(ip + 4, ipId_++);
	put16(ip + 6, 0x4000);                         // don't fragment
	ip[8] = 64; ip[9] = proto;
	put16(ip + 10, 0);
	put32(ip + 12, src); put32(ip + 16, dst);
	put16(ip + 10, fold(sumWords(ip, 20)));
	std::memcpy(ip + 20, payload.data(), payload.size());
	toGuest(f.data(), f.size());
}

void VirtualNetwork::sendUdp(uint32_t src, uint16_t sport, uint32_t dst, uint16_t dport, const uint8_t *data, size_t len, bool broadcast) {
	if (len > 1472) len = 1472;
	std::vector<uint8_t> u(8 + len);
	put16(u.data(), sport); put16(u.data() + 2, dport); put16(u.data() + 4, (uint16_t)(8 + len));
	if (len) std::memcpy(u.data() + 8, data, len);
	uint16_t c = l4Checksum(src, dst, PROTO_UDP, u.data(), u.size());
	put16(u.data() + 6, c ? c : 0xffff);
	sendIp(src, dst, PROTO_UDP, u, broadcast);
}

// ── ICMP ────────────────────────────────────────────────────────────────

void VirtualNetwork::handleIcmp(uint32_t src, uint32_t dst, const uint8_t *p, size_t len) {
	if (len < 8 || p[0] != 8) return;              // echo requests only
	if (dst != kRouterIp && dst != kDnsIp) return; // past the router: no ICMP from here
	std::vector<uint8_t> r(p, p + len);
	r[0] = 0;
	put16(r.data() + 2, 0);
	put16(r.data() + 2, fold(sumWords(r.data(), r.size())));
	sendIp(dst, src, PROTO_ICMP, r);
}

// ── UDP, DHCP and DNS ───────────────────────────────────────────────────

void VirtualNetwork::handleUdp(uint32_t src, uint32_t dst, const uint8_t *p, size_t len) {
	if (len < 8) return;
	uint16_t sport = be16(p), dport = be16(p + 2), ulen = be16(p + 4);
	if (ulen < 8 || ulen > len) return;
	const uint8_t *data = p + 8;
	size_t dlen = ulen - 8u;
	(void)src;
	if (dport == 67) { handleDhcp(data, dlen); return; }
	if (dst == 0xffffffffu || (dst & 0xf0000000u) == 0xe0000000u || dst == (kGuestIp | ~kNetmask)) return;
	if (dst == kDnsIp && dport == 53 && !host_) { dnsFail(sport, data, dlen); return; }
	if (host_) host_->udpSend(sport, dst, dport, data, dlen);
}

void VirtualNetwork::udpData(uint32_t id, uint32_t srcIp, uint16_t srcPort, const uint8_t *data, size_t len) {
	sendUdp(srcIp, srcPort, kGuestIp, (uint16_t)id, data, len);
}

void VirtualNetwork::dnsFail(uint16_t guestPort, const uint8_t *q, size_t len) {
	// SERVFAIL for every question: the resolver gives up at once.
	if (len < 12) return;
	std::vector<uint8_t> r(q, q + len);
	r[2] = (uint8_t)(0x80 | (q[2] & 0x79));        // response, opcode, RD
	r[3] = 0x82;                                   // RA, rcode 2
	put16(r.data() + 6, 0); put16(r.data() + 8, 0); put16(r.data() + 10, 0);
	sendUdp(kDnsIp, 53, kGuestIp, guestPort, r.data(), r.size());
}

void VirtualNetwork::handleDhcp(const uint8_t *p, size_t len) {
	if (len < 240 || p[0] != 1 || be32(p + 236) != 0x63825363) return;
	int type = 0;
	for (size_t i = 240; i < len;) {
		uint8_t opt = p[i];
		if (opt == 255) break;
		if (opt == 0) { i++; continue; }
		if (i + 1 >= len) break;
		uint8_t olen = p[i + 1];
		if (opt == 53 && olen >= 1 && i + 2 < len) type = p[i + 2];
		i += 2u + olen;
	}
	int reply;
	switch (type) {
	case 1: reply = 2; break;                      // DISCOVER -> OFFER
	case 3: reply = 5; leased_ = true; break;      // REQUEST -> ACK
	case 8: reply = 5; break;                      // INFORM -> ACK
	case 7: leased_ = false; return;               // RELEASE
	default: return;
	}
	std::vector<uint8_t> r(240, 0);
	r[0] = 2; r[1] = 1; r[2] = 6;
	std::memcpy(&r[4], p + 4, 4);                  // xid
	std::memcpy(&r[10], p + 10, 2);                // flags
	if (type != 8) put32(&r[16], kGuestIp);        // yiaddr
	put32(&r[20], kRouterIp);                      // siaddr
	std::memcpy(&r[28], p + 28, 16);               // chaddr
	put32(&r[236], 0x63825363);
	auto opt = [&](uint8_t code, std::initializer_list<uint8_t> v) {
		r.push_back(code); r.push_back((uint8_t)v.size()); r.insert(r.end(), v.begin(), v.end());
	};
	auto opt32 = [&](uint8_t code, uint32_t v) {
		opt(code, { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v });
	};
	opt(53, { (uint8_t)reply });
	opt32(54, kRouterIp);
	if (type != 8) opt32(51, kLeaseSeconds);
	opt32(1, kNetmask);
	opt32(3, kRouterIp);
	opt32(6, kDnsIp);
	opt(15, { 'l', 'a', 'n' });
	r.push_back(255);
	while (r.size() < 300) r.push_back(0);
	sendUdp(kRouterIp, 67, 0xffffffffu, 68, r.data(), r.size(), true);
}

// ── TCP ─────────────────────────────────────────────────────────────────
//
// Each guest connection ends in a minimal TCP endpoint here: in-order
// receive (anything else is dropped and re-acknowledged), a fixed 64 KB
// receive window, sends limited by the guest's window and MSS, and go-back-N
// retransmission on a timer. The far side of the connection is the host's
// socket, so this end never has to wait on the Internet to acknowledge.

VirtualNetwork::Tcp *VirtualNetwork::findTcp(uint32_t remoteIp, uint16_t remotePort, uint16_t guestPort) {
	for (auto &kv : tcp_) {
		Tcp &c = kv.second;
		if (c.remoteIp == remoteIp && c.remotePort == remotePort && c.guestPort == guestPort) return &c;
	}
	return nullptr;
}

void VirtualNetwork::sendTcp(Tcp &c, uint8_t flags, uint32_t seq, const uint8_t *data, size_t len, bool synOptions) {
	size_t hl = synOptions ? 24 : 20;
	std::vector<uint8_t> s(hl + len);
	put16(&s[0], c.remotePort); put16(&s[2], c.guestPort);
	put32(&s[4], seq);
	put32(&s[8], (flags & TCP_ACK) ? c.rcvNxt : 0);
	s[12] = (uint8_t)((hl / 4) << 4);
	s[13] = flags;
	put16(&s[14], 0xffff);
	if (synOptions) { s[20] = 2; s[21] = 4; put16(&s[22], kOurMss); }
	if (len) std::memcpy(&s[hl], data, len);
	if (traceOn())
		std::fprintf(stderr, "[tcp] to guest %u flags=%02x seq=%u ack=%u len=%zu\n", c.guestPort, flags, seq,
		             (flags & TCP_ACK) ? c.rcvNxt : 0, len);
	put16(&s[16], l4Checksum(c.remoteIp, kGuestIp, PROTO_TCP, s.data(), s.size()));
	sendIp(c.remoteIp, kGuestIp, PROTO_TCP, s);
}

void VirtualNetwork::sendRst(uint32_t src, uint16_t sport, uint32_t dst, uint16_t dport, uint32_t seq, uint32_t ack, bool withAck) {
	std::vector<uint8_t> s(20, 0);
	put16(&s[0], sport); put16(&s[2], dport);
	put32(&s[4], seq); put32(&s[8], withAck ? ack : 0);
	s[12] = 5 << 4;
	s[13] = (uint8_t)(TCP_RST | (withAck ? TCP_ACK : 0));
	put16(&s[16], l4Checksum(src, dst, PROTO_TCP, s.data(), s.size()));
	sendIp(src, dst, PROTO_TCP, s);
}

void VirtualNetwork::finish(uint32_t id) {
	if (host_) host_->tcpClose(id);
	tcp_.erase(id);
}

void VirtualNetwork::handleTcp(uint32_t src, uint32_t dst, const uint8_t *p, size_t len) {
	if (len < 20 || src != kGuestIp) return;
	uint16_t sport = be16(p), dport = be16(p + 2);
	uint32_t seq = be32(p + 4), ack = be32(p + 8);
	size_t hl = (size_t)(p[12] >> 4) * 4;
	uint8_t flags = p[13];
	uint16_t wnd = be16(p + 14);
	if (hl < 20 || hl > len) return;
	const uint8_t *data = p + hl;
	size_t dlen = len - hl;
	if (traceOn())
		std::fprintf(stderr, "[tcp] guest %u->%08x:%u flags=%02x seq=%u ack=%u wnd=%u len=%zu\n",
		             sport, dst, dport, flags, seq, ack, wnd, dlen);

	Tcp *c = findTcp(dst, dport, sport);
	if ((flags & TCP_SYN) && !(flags & TCP_ACK)) {
		if (c && (c->state == Tcp::Connecting || c->state == Tcp::SynSent) && c->rcvNxt == seq + 1) {
			if (c->state == Tcp::SynSent) sendTcp(*c, TCP_SYN | TCP_ACK, c->iss, nullptr, 0, true);
			return;                                // a retransmitted SYN
		}
		if (c) finish(c->id);
		if (!host_ || dst == kDnsIp) {             // nothing out there: refuse
			sendRst(dst, dport, src, sport, 0, seq + 1, true);
			return;
		}
		uint32_t id = nextTcpId_++;
		Tcp n;
		n.id = id; n.guestPort = sport; n.remotePort = dport; n.remoteIp = dst;
		n.rcvNxt = seq + 1;
		n.guestWnd = wnd;
		for (size_t i = 20; i + 1 < hl;) {         // the guest's MSS option
			uint8_t k = p[i];
			if (k == 0) break;
			if (k == 1) { i++; continue; }
			uint8_t l = p[i + 1];
			if (l < 2) break;
			if (k == 2 && l == 4 && i + 3 < hl) n.mss = be16(p + i + 2);
			i += l;
		}
		n.mss = std::clamp<uint16_t>(n.mss, 64, kOurMss);
		tcp_[id] = std::move(n);
		host_->tcpConnect(id, dst, dport);
		return;
	}
	if (!c) {
		if (!(flags & TCP_RST)) {
			if (flags & TCP_ACK) sendRst(dst, dport, src, sport, ack, 0, false);
			else sendRst(dst, dport, src, sport, 0, seq + (uint32_t)dlen + ((flags & TCP_FIN) ? 1 : 0), true);
		}
		return;
	}
	if (flags & TCP_RST) { finish(c->id); return; }
	if (c->state == Tcp::Connecting) return;       // still waiting on the host

	if (flags & TCP_ACK) {
		if (c->state == Tcp::SynSent) {
			if (ack != c->iss + 1) { sendRst(dst, dport, src, sport, ack, 0, false); return; }
			c->state = Tcp::Established;
			c->sndUna = c->sndNxt = c->iss + 1;
			c->rtoAt = UINT64_MAX;
			c->retries = 0;
		} else if (seqLt(c->sndUna, ack) && seqLe(ack, c->sndNxt)) {
			uint32_t acked = ack - c->sndUna;
			uint32_t dataAcked = std::min<uint32_t>(acked, (uint32_t)c->sendBuf.size());
			c->sendBuf.erase(c->sendBuf.begin(), c->sendBuf.begin() + dataAcked);
			if (c->finSent && ack == c->sndNxt) c->finAcked = true;
			c->sndUna = ack;
			c->retries = 0;
			c->rtoAt = c->sndUna == c->sndNxt ? UINT64_MAX : now_ + hz_ / 5;
		}
		c->guestWnd = wnd;
	}

	bool needAck = false;
	if (dlen) {
		if (seq == c->rcvNxt && !c->guestFin) {
			if (host_) host_->tcpSend(c->id, data, dlen);
			c->rcvNxt += (uint32_t)dlen;
		}
		needAck = true;                            // in order or not, say where we are
	}
	if ((flags & TCP_FIN) && seq + (uint32_t)dlen == c->rcvNxt && !c->guestFin) {
		c->rcvNxt += 1;
		c->guestFin = true;
		if (host_) host_->tcpShutdown(c->id);
		needAck = true;
	} else if ((flags & TCP_FIN) && c->guestFin) {
		needAck = true;                            // a retransmitted FIN
	}
	if (needAck) sendTcp(*c, TCP_ACK, c->sndNxt, nullptr, 0);
	uint32_t id = c->id;
	pump(*c);
	auto it = tcp_.find(id);
	if (it != tcp_.end() && it->second.guestFin && it->second.finAcked) finish(id);
}

void VirtualNetwork::pump(Tcp &c) {
	if (c.state != Tcp::Established) return;
	uint32_t inflight = c.sndNxt - c.sndUna;
	uint32_t wnd = std::max<uint32_t>(c.guestWnd, inflight == 0 ? 1u : 0u);   // probe a closed window
	size_t off = c.finSent ? c.sendBuf.size() : inflight;
	while (off < c.sendBuf.size() && inflight < wnd) {
		size_t n = std::min<size_t>({ (size_t)c.mss, c.sendBuf.size() - off, (size_t)(wnd - inflight) });
		uint8_t buf[kOurMss];
		std::copy(c.sendBuf.begin() + (long)off, c.sendBuf.begin() + (long)(off + n), buf);
		sendTcp(c, TCP_ACK | TCP_PSH, c.sndNxt, buf, n);
		c.sndNxt += (uint32_t)n;
		inflight += (uint32_t)n;
		off += n;
	}
	if (c.hostEof && !c.finSent && off >= c.sendBuf.size()) {
		sendTcp(c, TCP_FIN | TCP_ACK, c.sndNxt, nullptr, 0);
		c.sndNxt += 1;
		c.finSent = true;
	}
	if (c.sndNxt != c.sndUna && c.rtoAt == UINT64_MAX) c.rtoAt = now_ + hz_ / 5;
}

void VirtualNetwork::tcpOpened(uint32_t id) {
	auto it = tcp_.find(id);
	if (it == tcp_.end() || it->second.state != Tcp::Connecting) return;
	Tcp &c = it->second;
	c.state = Tcp::SynSent;
	c.iss = 0x10000000u + id * 0x01000193u;
	c.sndUna = c.iss;
	c.sndNxt = c.iss + 1;
	sendTcp(c, TCP_SYN | TCP_ACK, c.iss, nullptr, 0, true);
	c.rtoAt = now_ + hz_ / 2;
}

void VirtualNetwork::tcpFailed(uint32_t id) {
	auto it = tcp_.find(id);
	if (it == tcp_.end()) return;
	Tcp &c = it->second;
	sendRst(c.remoteIp, c.remotePort, kGuestIp, c.guestPort, 0, c.rcvNxt, true);
	tcp_.erase(it);
}

void VirtualNetwork::tcpData(uint32_t id, const uint8_t *data, size_t len) {
	auto it = tcp_.find(id);
	if (it == tcp_.end() || it->second.hostEof) return;
	it->second.sendBuf.insert(it->second.sendBuf.end(), data, data + len);
	pump(it->second);
}

void VirtualNetwork::tcpEof(uint32_t id) {
	auto it = tcp_.find(id);
	if (it == tcp_.end()) return;
	it->second.hostEof = true;
	if (it->second.state == Tcp::Connecting) { tcpFailed(id); return; }
	pump(it->second);
}

size_t VirtualNetwork::tcpBacklog(uint32_t id) const {
	auto it = tcp_.find(id);
	return it == tcp_.end() ? 0 : it->second.sendBuf.size();
}

uint64_t VirtualNetwork::tick(uint64_t now) {
	now_ = now;
	uint64_t next = UINT64_MAX;
	std::vector<uint32_t> dead;
	for (auto &kv : tcp_) {
		Tcp &c = kv.second;
		if (c.rtoAt <= now) {
			if (++c.retries > 10) {
				sendRst(c.remoteIp, c.remotePort, kGuestIp, c.guestPort, c.sndNxt, c.rcvNxt, true);
				dead.push_back(c.id);
				continue;
			}
			if (c.state == Tcp::SynSent) {
				sendTcp(c, TCP_SYN | TCP_ACK, c.iss, nullptr, 0, true);
			} else if (c.state == Tcp::Established) {
				// Go back to the oldest unacknowledged byte and send again.
				c.sndNxt = c.sndUna;
				if (c.finSent && !c.finAcked) c.finSent = false;
				c.rtoAt = UINT64_MAX;
				pump(c);
			}
			c.rtoAt = now + (hz_ / 5) * (1ull << std::min(c.retries, 5));
		}
		next = std::min(next, c.rtoAt);
	}
	for (uint32_t id : dead) finish(id);
	return next;
}
