// V30 smoke test — verifies the minimum-viable opcode set boots a
// synthetic program from the canonical x86 reset vector and halts
// with the expected register state.

#include "v30.h"

#include <array>
#include <cassert>
#include <cstdio>

namespace {

class FlatMemBus : public V30Bus {
public:
    std::array<uint8_t, 0x100000> mem{};

    uint8_t readMemByte(uint32_t addr) override {
        return mem[addr & 0xFFFFF];
    }
    uint16_t readMemWord(uint32_t addr) override {
        addr &= 0xFFFFF;
        return uint16_t(mem[addr]) | (uint16_t(mem[(addr + 1) & 0xFFFFF]) << 8);
    }
    void writeMemByte(uint32_t addr, uint8_t v) override {
        mem[addr & 0xFFFFF] = v;
    }
    void writeMemWord(uint32_t addr, uint16_t v) override {
        addr &= 0xFFFFF;
        mem[addr]                 = uint8_t(v & 0xFF);
        mem[(addr + 1) & 0xFFFFF] = uint8_t((v >> 8) & 0xFF);
    }
    uint8_t  readIoByte(uint16_t)               override { return 0xFF; }
    uint16_t readIoWord(uint16_t)               override { return 0xFFFF; }
    void     writeIoByte(uint16_t, uint8_t)     override {}
    void     writeIoWord(uint16_t, uint16_t)    override {}
};

} // namespace

int main() {
    FlatMemBus bus;
    V30        cpu(bus);

    // Reset vector at FFFF:0000 (linear 0xFFFF0):
    //   EA 00 00 00 F8       JMP F800:0000
    bus.mem[0xFFFF0] = 0xEA;
    bus.mem[0xFFFF1] = 0x00;
    bus.mem[0xFFFF2] = 0x00;
    bus.mem[0xFFFF3] = 0x00;
    bus.mem[0xFFFF4] = 0xF8;

    // Target program at F800:0000 (linear 0xF8000):
    //   B8 34 12             MOV AX, 0x1234
    //   F4                   HLT
    bus.mem[0xF8000] = 0xB8;
    bus.mem[0xF8001] = 0x34;
    bus.mem[0xF8002] = 0x12;
    bus.mem[0xF8003] = 0xF4;

    cpu.reset();
    int steps = 0;
    while (!cpu.halted && steps < 1000) {
        cpu.step();
        ++steps;
    }

    if (!cpu.halted) {
        std::fprintf(stderr, "FAIL: cpu did not halt within 1000 steps\n");
        return 1;
    }
    if (cpu.regs.w[0] != 0x1234) {
        std::fprintf(stderr, "FAIL: AX = 0x%04X, expected 0x1234\n", cpu.regs.w[0]);
        return 1;
    }
    if (cpu.sregs[1] != 0xF800) {
        std::fprintf(stderr, "FAIL: CS = 0x%04X, expected 0xF800\n", cpu.sregs[1]);
        return 1;
    }
    std::printf("PASS: AX=0x%04X CS=0x%04X IP=0x%04X halted after %d steps\n",
                cpu.regs.w[0], cpu.sregs[1], cpu.ip, steps);
    return 0;
}
