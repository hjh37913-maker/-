#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    // 32-bit immediate writes must clear the upper half for all GPRs.
    for (unsigned reg=0;reg<16;++reg) {
        std::array<std::uint8_t,6> code{}; const unsigned offset=reg<8?0:1;
        if (offset) code[0]=0x41;
        code[offset]=static_cast<std::uint8_t>(0xb8+(reg&7));
        code[offset+1]=0xef; code[offset+2]=0xcd; code[offset+3]=0xab; code[offset+4]=0x89;
        const auto len=offset+5; Memory mem(len); CHECK(mem.write(0,std::span(code).first(len)));
        Cpu cpu; *cpu.state.reg(reg)=UINT64_MAX; cpu.state.rflags=0x402;
        CHECK(cpu.step(mem)==StepResult::running && *cpu.state.reg(reg)==0x89abcdefULL && cpu.state.rflags==0x402);
        for (unsigned n=1;n<len;++n) { Memory truncated(n); CHECK(truncated.write(0,std::span(code).first(n))); Cpu t;
            CHECK(t.step(truncated)==StepResult::memory_fault && t.state.rip==0); }
    }
    // LEA does not access memory even when the resulting address is outside the address space.
    { Memory m(4); const std::array<std::uint8_t,4> code{0x48,0x8d,0x04,0x37}; CHECK(m.write(0,code)); Cpu c;
      *c.state.reg(7)=UINT64_MAX-3; *c.state.reg(6)=9; c.state.rflags=0x402;
      CHECK(c.step(m)==StepResult::running && c.state.rax==5 && c.state.rflags==0x402); }
    { Memory m(3); const std::array<std::uint8_t,3> code{0x48,0x8d,0xc0}; CHECK(m.write(0,code)); Cpu c;
      CHECK(c.step(m)==StepResult::invalid_instruction && c.state.rip==0); }
    // LEA RIP-relative uses the instruction end as base.
    { Memory m(7); const std::array<std::uint8_t,7> code{0x48,0x8d,0x05,0xf8,0xff,0xff,0xff}; CHECK(m.write(0,code)); Cpu c;
      CHECK(c.step(m)==StepResult::running && c.state.rax==UINT64_MAX); }
    // Signed imm8 for RSP, extended register, CMP preserves value and flags on faults.
    { Memory m(4); const std::array<std::uint8_t,4> code{0x48,0x83,0xec,0x10}; CHECK(m.write(0,code)); Cpu c; *c.state.reg(4)=32;
      CHECK(c.step(m)==StepResult::running && *c.state.reg(4)==16); }
    { Memory m(4); const std::array<std::uint8_t,4> code{0x49,0x83,0xc0,0xff}; CHECK(m.write(0,code)); Cpu c; *c.state.reg(8)=1;
      CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==0 && (c.state.rflags&0x41)==0x41); }
    { Memory m(4); const std::array<std::uint8_t,4> code{0x48,0x83,0xf8,0xff}; CHECK(m.write(0,code)); Cpu c; c.state.rax=UINT64_MAX;
      CHECK(c.step(m)==StepResult::running && c.state.rax==UINT64_MAX && (c.state.rflags&0x40)); }
    { Memory m(3); const std::array<std::uint8_t,3> code{0x48,0x83,0xc0}; CHECK(m.write(0,code)); Cpu c; c.state.rax=5; c.state.rflags=0x402;
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==5 && c.state.rflags==0x402); }
    { Memory m(4); const std::array<std::uint8_t,4> code{0x48,0x83,0xd0,1}; CHECK(m.write(0,code)); Cpu c;
      CHECK(c.step(m)==StepResult::running && c.state.rax==1 && c.state.rip==4); }
}
