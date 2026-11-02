// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Psion LX scripted console harness.
//
// Boots the LX (OS image + factory NAND) and drives its serial console — the
// image runs a getty on ttySA0 with a password-less root — from a script, so
// a feature can be checked from inside the guest the way a user at a shell
// would: mount the CF card, play into /dev/dsp, bring up wlan0.
//
//   lx-harness ROM NAND [--cf CARD.img] [--wav OUT.wav] [--no-wifi]
//              [--save-cf OUT.img] [--net] SCRIPT
//
// SCRIPT lines (sim seconds; '#' starts a comment):
//   wait TEXT [TIMEOUT]   run until TEXT appears in new console output
//   send TEXT             type TEXT followed by a newline
//   sleep SECONDS         run for that much guest time
//   insert-cf / eject-cf  hot-plug the --cf card
//   expect TEXT           fail unless TEXT appeared since the last mark
//   mark                  start a new expect window
//   screenshot PATH       write the panel as a PGM
//
// Exit status is non-zero when a wait times out or an expect fails.

#include "../core/netbookpro.h"
#include "../core/inflate.h"
#include "../core/virtnet.h"
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> readFile(const char *path) {
	std::ifstream f(path, std::ios::binary);
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void writeWav(const char *path, const std::vector<int16_t> &pcm, int rate, int channels) {
	FILE *f = std::fopen(path, "wb");
	if (!f) return;
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	uint32_t data = (uint32_t)(pcm.size() * 2);
	std::fwrite("RIFF", 1, 4, f); u32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16((uint16_t)channels); u32((uint32_t)rate); u32((uint32_t)(rate * 2 * channels));
	u16((uint16_t)(2 * channels)); u16(16);
	std::fwrite("data", 1, 4, f); u32(data);
	std::fwrite(pcm.data(), 2, pcm.size(), f);
	std::fclose(f);
}

// The outside world through BSD sockets, as the desktop app's main process
// provides it with Node's: 10.0.2.2 is this machine's loopback, 10.0.2.3:53
// the resolver in /etc/resolv.conf.
struct SocketHost : NetHost {
	VirtualNetwork *net = nullptr;
	struct Conn { int fd; bool connecting; std::string out; bool shut = false; bool eof = false; };
	std::map<uint32_t, Conn> tcp;
	std::map<uint32_t, int> udp;
	uint32_t resolver = 0x08080808;
	uint16_t resolverPort = 53;
	bool log = false;

	SocketHost() {
		std::ifstream rc("/etc/resolv.conf");
		std::string line;
		while (std::getline(rc, line))
			if (line.rfind("nameserver ", 0) == 0) {
				in_addr a;
				if (inet_aton(line.c_str() + 11, &a)) { resolver = ntohl(a.s_addr); break; }
			}
		if (const char *d = std::getenv("LX_DNS")) {            // IP:PORT, for tests
			std::string v = d;
			size_t c = v.find(':');
			in_addr a;
			if (inet_aton(v.substr(0, c).c_str(), &a)) resolver = ntohl(a.s_addr);
			if (c != std::string::npos) resolverPort = (uint16_t)std::atoi(v.c_str() + c + 1);
		}
	}
	static uint32_t mapIp(uint32_t ip) { return ip == VirtualNetwork::kRouterIp ? 0x7f000001 : ip; }
	void tcpConnect(uint32_t id, uint32_t ip, uint16_t port) override {
		int fd = socket(AF_INET, SOCK_STREAM, 0);
		fcntl(fd, F_SETFL, O_NONBLOCK);
		sockaddr_in a = {};
		a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = htonl(mapIp(ip));
		int r = connect(fd, (sockaddr *)&a, sizeof(a));
		if (log) std::fprintf(stderr, "[net] tcp %u connect %08x:%u -> %d\n", id, ip, port, r == 0 ? 0 : errno);
		if (r != 0 && errno != EINPROGRESS) { close(fd); net->tcpFailed(id); return; }
		tcp[id] = { fd, r != 0, std::string() };
		if (r == 0) net->tcpOpened(id);
	}
	void tcpSend(uint32_t id, const uint8_t *d, size_t n) override {
		auto it = tcp.find(id);
		if (it != tcp.end()) it->second.out.append((const char *)d, n);
	}
	void tcpShutdown(uint32_t id) override { auto it = tcp.find(id); if (it != tcp.end()) it->second.shut = true; }
	void tcpClose(uint32_t id) override {
		auto it = tcp.find(id);
		if (it == tcp.end()) return;
		if (log) std::fprintf(stderr, "[net] tcp %u close\n", id);
		close(it->second.fd);
		tcp.erase(it);
	}
	void udpSend(uint32_t id, uint32_t ip, uint16_t port, const uint8_t *d, size_t n) override {
		int &fd = udp[id];
		if (!fd) { fd = socket(AF_INET, SOCK_DGRAM, 0); fcntl(fd, F_SETFL, O_NONBLOCK); }
		bool dns = ip == VirtualNetwork::kDnsIp && port == 53;
		uint32_t to = dns ? resolver : mapIp(ip);
		sockaddr_in a = {};
		a.sin_family = AF_INET; a.sin_port = htons(dns ? resolverPort : port); a.sin_addr.s_addr = htonl(to);
		sendto(fd, d, n, 0, (sockaddr *)&a, sizeof(a));
		if (log) std::fprintf(stderr, "[net] udp %u -> %08x:%u (%zu bytes)\n", id, to, port, n);
	}
	void poll() {
		uint8_t buf[16384];
		std::vector<uint32_t> ids;
		for (auto &kv : tcp) ids.push_back(kv.first);
		for (uint32_t id : ids) {
			auto it = tcp.find(id);
			if (it == tcp.end()) continue;
			Conn &c = it->second;
			if (c.connecting) {
				pollfd p = { c.fd, POLLOUT, 0 };
				if (::poll(&p, 1, 0) <= 0) continue;
				int err = 0; socklen_t l = sizeof(err);
				getsockopt(c.fd, SOL_SOCKET, SO_ERROR, &err, &l);
				if (err) { close(c.fd); tcp.erase(it); net->tcpFailed(id); continue; }
				c.connecting = false;
				net->tcpOpened(id);
				continue;
			}
			if (!c.out.empty()) {
				ssize_t w = send(c.fd, c.out.data(), c.out.size(), MSG_NOSIGNAL);
				if (w > 0) c.out.erase(0, (size_t)w);
			}
			if (c.shut && c.out.empty()) { shutdown(c.fd, SHUT_WR); c.shut = false; }
			if (!c.eof && net->tcpBacklog(id) < 128 * 1024) {
				ssize_t r = recv(c.fd, buf, sizeof(buf), 0);
				if (r > 0) net->tcpData(id, buf, (size_t)r);
				else if (r == 0) { c.eof = true; net->tcpEof(id); }
			}
		}
		for (auto &kv : udp) {
			sockaddr_in a = {}; socklen_t l = sizeof(a);
			ssize_t r = recvfrom(kv.second, buf, sizeof(buf), 0, (sockaddr *)&a, &l);
			if (r <= 0) continue;
			uint32_t from = ntohl(a.sin_addr.s_addr);
			uint16_t port = ntohs(a.sin_port);
			if (from == resolver && port == resolverPort) { from = VirtualNetwork::kDnsIp; port = 53; }
			else if (from == 0x7f000001) from = VirtualNetwork::kRouterIp;
			net->udpData(kv.first, from, port, buf, (size_t)r);
		}
	}
};

} // namespace

int main(int argc, char **argv) {
	const char *romPath = nullptr, *nandPath = nullptr, *scriptPath = nullptr;
	const char *cfPath = nullptr, *wavPath = nullptr, *saveCfPath = nullptr;
	bool echo = std::getenv("LX_ECHO") != nullptr;
	bool wifi = true, useNet = false;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if (a == "--cf" && i + 1 < argc) cfPath = argv[++i];
		else if (a == "--wav" && i + 1 < argc) wavPath = argv[++i];
		else if (a == "--save-cf" && i + 1 < argc) saveCfPath = argv[++i];
		else if (a == "--no-wifi") wifi = false;
		else if (a == "--net") useNet = true;
		else if (a == "--echo") echo = true;
		else if (!romPath) romPath = argv[i];
		else if (!nandPath) nandPath = argv[i];
		else scriptPath = argv[i];
	}
	if (!romPath || !nandPath || !scriptPath) {
		std::fprintf(stderr, "usage: lx-harness ROM NAND [--cf IMG] [--wav OUT] [--save-cf OUT] [--no-wifi] SCRIPT\n");
		return 2;
	}

	NetbookPro::Emulator emu;
	std::vector<uint8_t> rom = readFile(romPath);
	emu.loadROM(rom.data(), rom.size());
	std::vector<uint8_t> nand = readFile(nandPath);
	if (Inflate::isGzip(nand.data(), nand.size())) {
		std::vector<uint8_t> raw;
		if (!Inflate::gzip(nand.data(), nand.size(), raw)) { std::fprintf(stderr, "bad NAND gzip\n"); return 2; }
		nand.swap(raw);
	}
	if (!emu.attachInternalFlash(nand.data(), nand.size())) { std::fprintf(stderr, "NAND image size wrong\n"); return 2; }
	emu.setWifiCardInserted(wifi);
	std::vector<uint8_t> cf;
	if (cfPath) {
		cf = readFile(cfPath);
		if (!emu.attachCard(cf.data(), cf.size())) { std::fprintf(stderr, "CF attach failed\n"); return 2; }
	}
	emu.setHostAudioEnabled(true, false);
	SocketHost host;
	host.log = std::getenv("LX_NET_LOG") != nullptr;
	host.net = emu.virtualNetwork();
	if (useNet) emu.virtualNetwork()->setHost(&host);

	const int64_t hz = NetbookPro::Emulator::kCpuHz;
	const int64_t step = hz / 100;                     // 10 ms of guest time per poll
	int64_t now = 0;
	size_t seen = 0, markAt = 0;
	std::vector<int16_t> audio;
	int16_t abuf[8192];

	auto run = [&](int64_t until) {
		while (now < until) {
			now = std::min(until, now + step);
			emu.executeUntil(now);
			if (useNet) host.poll();
			size_t n;
			while ((n = emu.readAudioOutput(abuf, 8192)) > 0) audio.insert(audio.end(), abuf, abuf + n);
			const std::string &log = emu.serialLog();
			if (echo && log.size() > seen) std::fwrite(log.data() + seen, 1, log.size() - seen, stdout), std::fflush(stdout);
			seen = log.size();
		}
	};

	std::ifstream script(scriptPath);
	std::string line;
	int lineNo = 0, failures = 0;
	while (std::getline(script, line)) {
		lineNo++;
		size_t hash = line.find(" #");
		if (!line.empty() && line[0] == '#') continue;
		if (hash != std::string::npos) line.erase(hash);
		while (!line.empty() && (line.back() == ' ' || line.back() == '\r')) line.pop_back();
		if (line.empty()) continue;
		std::string cmd = line.substr(0, line.find(' '));
		std::string arg = line.size() > cmd.size() ? line.substr(cmd.size() + 1) : std::string();
		if (cmd == "wait") {
			double timeout = 60;
			size_t sp = arg.rfind(' ');
			if (sp != std::string::npos) {
				char *end = nullptr;
				double t = std::strtod(arg.c_str() + sp + 1, &end);
				if (end && *end == 0 && t > 0) { timeout = t; arg.erase(sp); }
			}
			size_t from = seen;
			int64_t limit = now + (int64_t)(timeout * hz);
			bool found = false;
			while (now < limit) {
				run(now + step);
				if (emu.serialLog().find(arg, from) != std::string::npos) { found = true; break; }
			}
			if (!found) {
				std::fprintf(stderr, "line %d: timed out waiting for \"%s\" (t=%.1fs)\n", lineNo, arg.c_str(), (double)now / hz);
				std::fprintf(stderr, "--- console tail ---\n%s\n--- kernel log tail ---\n%s\n",
				             emu.serialLog().substr(emu.serialLog().size() > 3000 ? emu.serialLog().size() - 3000 : 0).c_str(),
				             emu.kernelLog().substr(emu.kernelLog().size() > 3000 ? emu.kernelLog().size() - 3000 : 0).c_str());
				failures++;
				break;
			}
		} else if (cmd == "send") {
			arg += "\r";
			// The UART FIFO holds 64 bytes: feed it as the guest drains it.
			for (size_t i = 0; i < arg.size();) {
				size_t room = emu.soc().uartRxSpace(PXA255::FFUART);
				size_t n = std::min(room, arg.size() - i);
				if (n) { emu.serialInput((const uint8_t *)arg.data() + i, n); i += n; }
				run(now + hz / 1000);
			}
		} else if (cmd == "sleep") {
			run(now + (int64_t)(std::atof(arg.c_str()) * hz));
		} else if (cmd == "mark") {
			markAt = emu.serialLog().size();
		} else if (cmd == "expect") {
			if (emu.serialLog().find(arg, markAt) == std::string::npos) {
				std::fprintf(stderr, "line %d: expected \"%s\"\n", lineNo, arg.c_str());
				failures++;
			}
		} else if (cmd == "insert-cf") {
			if (!cf.empty()) emu.attachCard(cf.data(), cf.size());
		} else if (cmd == "eject-cf") {
			// Keep what the guest wrote, as the frontend does, for the next insert.
			if (emu.getCardImageData()) cf.assign(emu.getCardImageData(), emu.getCardImageData() + emu.getCardImageSize());
			emu.detachCard();
		} else if (cmd == "kernel-log") {
			std::printf("%s\n", emu.kernelLog().c_str());
		} else if (cmd == "screenshot") {
			int w = emu.getLCDWidth(), h = emu.getLCDHeight();
			std::vector<uint8_t> px((size_t)w * h);
			std::vector<uint8_t *> rows(h);
			for (int y = 0; y < h; y++) rows[y] = px.data() + (size_t)y * w;
			emu.readLCDIntoBuffer(rows.data(), false);
			if (FILE *f = std::fopen(arg.c_str(), "wb")) {
				std::fprintf(f, "P5\n%d %d\n255\n", w, h);
				std::fwrite(px.data(), 1, px.size(), f);
				std::fclose(f);
			}
		} else {
			std::fprintf(stderr, "line %d: unknown command %s\n", lineNo, cmd.c_str());
			failures++;
		}
	}
	if (wavPath) writeWav(wavPath, audio, emu.getAudioSampleRate(), 1);
	if (saveCfPath && emu.getCardImageData()) {
		if (FILE *f = std::fopen(saveCfPath, "wb")) {
			std::fwrite(emu.getCardImageData(), 1, emu.getCardImageSize(), f);
			std::fclose(f);
		}
	}
	std::fprintf(stderr, "lx-harness: %s at t=%.1fs, %zu audio samples\n", failures ? "FAILED" : "ok",
	             (double)now / hz, audio.size());
	return failures ? 1 : 0;
}
