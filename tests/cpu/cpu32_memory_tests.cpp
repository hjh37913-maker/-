#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    std::size_t cases=0;
    for (auto opcode : {std::uint8_t{0x89},std::uint8_t{0x8b}}) for (unsigned mode=0;mode<3;++mode)
    for (unsigned reg=0;reg<8;++reg) for (unsigned base=0;base<8;++base) {
        if (base==4 || (mode==0 && base==5)) continue; // Target SIB/RIP separately below.
        const std::array<std::uint8_t,6> code{opcode,static_cast<std::uint8_t>((mode<<6)|(reg<<3)|base),0xf8,0xff,0xff,0xff};
        const unsigned length=mode==0?2:mode==1?3:6;
        Memory m(256); CHECK(m.write(0,std::span(code).first(length)));
        const std::array<std::uint8_t,4> initial{0x12,0x34,0x56,0x78}; CHECK(m.write(163,initial));
        Cpu c; for (unsigned i=0;i<8;++i) *c.state.reg(i)=0xfeedface00000000ULL+i;
        *c.state.reg(base)=mode==0?163:171; c.state.rflags=0x402;
        std::array<std::uint64_t,8> before{}; for (unsigned i=0;i<8;++i) before[i]=*c.state.reg(i);
        CHECK(c.step(m)==StepResult::running && c.state.rip==length && c.state.rflags==0x402);
        if (opcode==0x8b) CHECK(*c.state.reg(reg)==0x78563412ULL);
        for (unsigned i=0;i<8;++i) if (opcode==0x89 || i!=reg) CHECK(*c.state.reg(i)==before[i]);
        std::array<std::uint8_t,4> data{}; CHECK(m.read(163,data));
        const auto value=opcode==0x89?before[reg]:0x78563412ULL;
        for (unsigned i=0;i<4;++i) CHECK(data[i]==static_cast<std::uint8_t>(value>>(i*8)));
        ++cases;
    }
    // C7 RIP-relative displacement is relative to end of immediate, not end of displacement.
    { Memory m(64); const std::array<std::uint8_t,10> code{0xc7,0x05,22,0,0,0,0xef,0xcd,0xab,0x89};
      CHECK(m.write(0,code)); Cpu c; c.state.rflags=0x402;
      CHECK(c.step(m)==StepResult::running && c.state.rip==10 && c.state.rflags==0x402);
      std::array<std::uint8_t,4> data{}; CHECK(m.read(32,data)); CHECK((data==std::array<std::uint8_t,4>{0xef,0xcd,0xab,0x89})); }
    // SIB [rsp-4], little-endian store, and zero-extended read.
    { Memory m(64); const std::array<std::uint8_t,8> code{0x89,0x44,0x24,0xfc,0x8b,0x4c,0x24,0xfc}; CHECK(m.write(0,code));
      Cpu c; *c.state.reg(4)=48; c.state.rax=0x11223344aabbccddULL; *c.state.reg(1)=UINT64_MAX;
      CHECK(c.step(m)==StepResult::running && c.step(m)==StepResult::running && *c.state.reg(1)==0xaabbccddULL); }
    // LEA computes a 64-bit effective address then zero-extends its low 32 bits.
    { Memory m(3); const std::array<std::uint8_t,3> code{0x8d,0x04,0x10}; CHECK(m.write(0,code));
      Cpu c; c.state.rax=0xffffffff00000004ULL; *c.state.reg(2)=0x0000000100000008ULL;
      CHECK(c.step(m)==StepResult::running && c.state.rax==12); }
    // Denied write crossing a permission boundary is atomic.
    { Memory m(64); const std::array<std::uint8_t,2> code{0x89,0x03}; const std::array<std::uint8_t,4> data{1,2,3,4};
      CHECK(m.write(0,code) && m.write(40,data) && m.protect(42,1,Permission::read));
      Cpu c; c.state.rax=42; *c.state.reg(3)=40; CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==42);
      std::array<std::uint8_t,4> after{}; CHECK(m.read(40,after) && after==data); }
    // Truncated C7 and unsupported extension must not modify state.
    { const std::array<std::uint8_t,10> code{0xc7,0x05,22,0,0,0,0xef,0xcd,0xab,0x89};
      for (unsigned n=1;n<10;++n) { Memory m(n); CHECK(m.write(0,std::span(code).first(n))); Cpu c; c.state.rax=123;
        CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==123); } }
    { Memory m(2); const std::array<std::uint8_t,2> code{0xc7,0xc8}; CHECK(m.write(0,code)); Cpu c;
      CHECK(c.step(m)==StepResult::invalid_instruction && c.state.rip==0); }
    { Memory m(6); const std::array<std::uint8_t,6> code{0xc7,0xc0,0xef,0xcd,0xab,0x89}; CHECK(m.write(0,code)); Cpu c; c.state.rax=UINT64_MAX;
      CHECK(c.step(m)==StepResult::running && c.state.rax==0x89abcdefULL); }
    std::cout << cases << " 32-bit memory register/base/mode combinations passed\n";
}
