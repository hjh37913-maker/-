#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    Cpu c; CHECK(c.state.reg(16)==nullptr && c.state.reg(999)==nullptr);
    for (unsigned i=0;i<16;++i) {
        const std::array<std::uint8_t,10> code{static_cast<std::uint8_t>(i<8?0x48:0x49),static_cast<std::uint8_t>(0xb8+(i&7)),
            0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01};
        Memory m(code.size()); CHECK(m.write(0,code)); Cpu cpu; cpu.state.rflags=0x402;
        CHECK(cpu.step(m)==StepResult::running && cpu.state.rip==10);
        CHECK(*cpu.state.reg(i)==0x0123456789abcdefULL && cpu.state.rflags==0x402);
        for (unsigned j=0;j<16;++j) if (j!=i) CHECK(*cpu.state.reg(j)==0);
        for (std::size_t n=1;n<10;++n) {
            Memory truncated(n); CHECK(truncated.write(0,std::span(code).first(n))); Cpu t;
            CHECK(t.step(truncated)==StepResult::memory_fault && t.state.rip==0 && *t.state.reg(i)==0);
        }
    }
    for (auto op : {0x01,0x03,0x29,0x2b,0x39,0x3b}) {
        // Arithmetic memory operands are intentionally unsupported.
        const std::array<std::uint8_t,3> memop{0x48,static_cast<std::uint8_t>(op),0x00};
        Memory m(3); CHECK(m.write(0,memop)); Cpu t; t.state.rax=42; t.state.rflags=0x402;
        CHECK(t.step(m)==StepResult::invalid_instruction && t.state.rip==0 && t.state.rax==42 && t.state.rflags==0x402);
        Memory truncated(2); CHECK(truncated.write(0,std::span(memop).first(2))); Cpu u;
        CHECK(u.step(truncated)==StepResult::memory_fault && u.state.rip==0);
    }
    // MOV R8, 7; MOV RCX, R8; ADD RCX, R8; MOV RAX, RCX; HLT.
    const std::array<std::uint8_t,20> program{0x49,0xb8,7,0,0,0,0,0,0,0, 0x4c,0x89,0xc1,
        0x4c,0x01,0xc1, 0x48,0x89,0xc8, 0xf4};
    Memory m(program.size()); CHECK(m.write(0,program)); Cpu cpu;
    for (int i=0;i<4;++i) CHECK(cpu.step(m)==StepResult::running);
    CHECK(cpu.step(m)==StepResult::halted && cpu.state.rax==14 && *cpu.state.reg(1)==14 && *cpu.state.reg(8)==7);
    return 0;
}
