// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
//
// Runs a flat ARM binary (built from guest.c by run-xscale-test.sh) on the
// XScale core with 2 MB of RAM at 0xA0000000 and a two-register test bus:
// a byte written to 0x40100000 is printed, a word written to 0x40100004
// ends the run with that exit code.
#include "../../../core/xscale.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
struct TestBus : XScale::Bus {
	bool done = false;
	uint32_t code = 0;
	bool busRead(uint32_t, int, uint32_t &out) override { out = 0; return true; }
	bool busWrite(uint32_t pa, int, uint32_t v) override {
		if (pa == 0x40100000) { std::putchar((int)(v & 0xff)); return true; }
		if (pa == 0x40100004) { done = true; code = v; return true; }
		return false;
	}
};
} // namespace

int main(int argc, char **argv) {
	if (argc < 2) { std::fprintf(stderr, "usage: %s guest.bin\n", argv[0]); return 2; }
	FILE *f = std::fopen(argv[1], "rb");
	if (!f) { std::perror(argv[1]); return 2; }
	std::vector<uint8_t> ram(2 << 20);
	size_t n = std::fread(ram.data(), 1, ram.size(), f);
	std::fclose(f);
	if (!n) { std::fprintf(stderr, "empty image\n"); return 2; }
	TestBus bus;
	XScale::Cpu cpu(bus);
	cpu.setRam(ram.data(), 0xa0000000u, (uint32_t)ram.size());
	if (argc > 2 && std::string(argv[2]) == "plain") cpu.setPredecode(false);
	cpu.setPc(0xa0000000u);
	uint64_t budget = 2000000000ull;
	while (!bus.done && cpu.instructions() < budget) cpu.run(100000);
	if (!bus.done) {
		std::fprintf(stderr, "guest did not finish: pc=%08x cpsr=%08x insns=%llu\n",
		             cpu.pc(), cpu.cpsr(), (unsigned long long)cpu.instructions());
		return 1;
	}
	std::fprintf(stderr, "[%llu instructions, und=%llu abt=%llu]\n",
	             (unsigned long long)cpu.instructions(),
	             (unsigned long long)cpu.excCount[1],
	             (unsigned long long)(cpu.excCount[3] + cpu.excCount[4]));
	return (int)bus.code;
}
