// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <vector>

// The network behind the emulated Wi-Fi access point: a user-mode router in
// the style of QEMU's slirp. The guest sees an ordinary IPv4 LAN —
//
//   10.0.2.15  the guest, by DHCP
//   10.0.2.2   the router (and, for TCP and UDP, the host machine itself)
//   10.0.2.3   the DNS server
//
// — and everything past the router is reached through a NetHost: the
// guest's TCP connections end here, in a small TCP endpoint per
// connection, and their byte streams go to host sockets; UDP datagrams go
// to host datagram sockets. ARP, DHCP and ping of the router are answered
// here. With no NetHost (the browser, which cannot open sockets) the guest
// still gets its address, connections are refused at once and DNS lookups
// fail quickly, so applications report "no network" instead of hanging.

// The outside world, as the host environment provides it. Every call names
// a flow by an id the network chose; the host answers through the
// VirtualNetwork's tcp*/udp* entry points with the same id.
class NetHost {
public:
	virtual ~NetHost() = default;
	// Open a TCP connection to ip:port (host byte order; 10.0.2.2 means the
	// host's own loopback). Answer with tcpOpened or tcpFailed.
	virtual void tcpConnect(uint32_t id, uint32_t ip, uint16_t port) = 0;
	virtual void tcpSend(uint32_t id, const uint8_t *data, size_t len) = 0;
	// The guest has finished sending (FIN): half-close the host socket.
	virtual void tcpShutdown(uint32_t id) = 0;
	// The connection is gone (reset, or both sides done): release it.
	virtual void tcpClose(uint32_t id) = 0;
	// Send a datagram from the guest's UDP port `id` (10.0.2.3:53 means the
	// host's DNS resolver).
	virtual void udpSend(uint32_t id, uint32_t ip, uint16_t port, const uint8_t *data, size_t len) = 0;
};

class VirtualNetwork {
public:
	explicit VirtualNetwork(uint64_t cyclesPerSecond) : hz_(cyclesPerSecond) {}

	static constexpr uint32_t kGuestIp = 0x0a00020f, kRouterIp = 0x0a000202, kDnsIp = 0x0a000203;
	static constexpr uint32_t kNetmask = 0xffffff00;

	// Frames for the guest (Ethernet II, no FCS).
	std::function<void(const uint8_t *frame, size_t len)> toGuest;
	// A frame the guest sent.
	void fromGuest(const uint8_t *frame, size_t len);
	// Advance time (guest cycles) for retransmissions; returns the cycle of
	// the next timer, UINT64_MAX for none.
	uint64_t tick(uint64_t now);
	void reset();
	// Abandon every flow (the host's sockets are gone): the guest's TCP
	// connections are reset, its address and the lease are kept.
	void dropFlows();

	void setHost(NetHost *host) { host_ = host; }
	bool hasHost() const { return host_ != nullptr; }

	// ── The host's answers ──────────────────────────────────────────────
	void tcpOpened(uint32_t id);
	void tcpFailed(uint32_t id);
	void tcpData(uint32_t id, const uint8_t *data, size_t len);
	void tcpEof(uint32_t id);                        // the remote end closed
	// Bytes taken from the host but not yet acknowledged by the guest: the
	// host should stop reading a socket while this is large.
	size_t tcpBacklog(uint32_t id) const;
	void udpData(uint32_t id, uint32_t srcIp, uint16_t srcPort, const uint8_t *data, size_t len);

	// Diagnostics.
	bool guestHasLease() const { return leased_; }
	size_t tcpConnections() const { return tcp_.size(); }

private:
	uint64_t hz_;
	uint64_t now_ = 0;
	NetHost *host_ = nullptr;
	uint8_t guestMac_[6] = {};
	bool guestMacKnown_ = false, leased_ = false;
	uint16_t ipId_ = 1;

	struct Tcp {
		uint32_t id;
		uint16_t guestPort, remotePort;
		uint32_t remoteIp;
		enum State { Connecting, SynSent, Established, Closed } state = Connecting;
		uint32_t iss = 0;                  // our initial sequence number
		uint32_t sndUna = 0, sndNxt = 0;   // oldest unacknowledged, next to send
		uint32_t rcvNxt = 0;               // next byte expected from the guest
		uint32_t guestWnd = 0;
		uint16_t mss = 536;
		std::deque<uint8_t> sendBuf;       // bytes from sndUna on
		bool hostEof = false, finSent = false, finAcked = false, guestFin = false;
		uint64_t rtoAt = UINT64_MAX;
		int retries = 0;
	};
	std::map<uint32_t, Tcp> tcp_;          // by id
	uint32_t nextTcpId_ = 1;
	Tcp *findTcp(uint32_t remoteIp, uint16_t remotePort, uint16_t guestPort);

	void handleArp(const uint8_t *p, size_t len);
	void handleIp(const uint8_t *p, size_t len);
	void handleUdp(uint32_t src, uint32_t dst, const uint8_t *p, size_t len);
	void handleTcp(uint32_t src, uint32_t dst, const uint8_t *p, size_t len);
	void handleIcmp(uint32_t src, uint32_t dst, const uint8_t *p, size_t len);
	void handleDhcp(const uint8_t *p, size_t len);
	void dnsFail(uint16_t guestPort, const uint8_t *query, size_t len);

	void sendIp(uint32_t src, uint32_t dst, uint8_t proto, const std::vector<uint8_t> &payload, bool broadcast = false);
	void sendUdp(uint32_t src, uint16_t sport, uint32_t dst, uint16_t dport, const uint8_t *data, size_t len, bool broadcast = false);
	void sendTcp(Tcp &c, uint8_t flags, uint32_t seq, const uint8_t *data, size_t len, bool synOptions = false);
	void sendRst(uint32_t src, uint16_t sport, uint32_t dst, uint16_t dport, uint32_t seq, uint32_t ack, bool withAck);
	void pump(Tcp &c);                     // send what the guest's window allows
	void finish(uint32_t id);              // drop a connection, telling the host
};
