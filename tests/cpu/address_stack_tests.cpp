#include "core/machine.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    // Store via RSP SIB and load via R12-indexed SIB with disp8 and scale 4.
    { Memory m(256); constexpr std::array<std::uint8_t,4> store{0x48,0x89,0x04,0x24};
      CHECK(m.write(0,store)); Cpu c; c.state.rax=0x0123456789abcdefULL; *c.state.reg(4)=128;
      CHECK(c.step(m)==StepResult::running && c.state.rip==4);
      std::array<std::uint8_t,8> data{}; CHECK(m.read(128,data));
      CHECK((data==std::array<std::uint8_t,8>{0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01})); }
    { Memory m(256); constexpr std::array<std::uint8_t,5> load{0x4a,0x8b,0x44,0xa3,0xf8};
      constexpr std::array<std::uint8_t,8> data{42,0,0,0,0,0,0,0}; CHECK(m.write(0,load) && m.write(104,data));
      Cpu c; *c.state.reg(3)=100; *c.state.reg(12)=3; CHECK(c.step(m)==StepResult::running && c.state.rax==42 && c.state.rip==5); }
    // RIP-relative and SIB without base; signed disp32 is based on end of instruction for RIP.
    { Memory m(256); constexpr std::array<std::uint8_t,7> load{0x48,0x8b,0x05,121,0,0,0};
      constexpr std::array<std::uint8_t,8> data{7,0,0,0,0,0,0,0}; CHECK(m.write(0,load) && m.write(128,data));
      Cpu c; CHECK(c.step(m)==StepResult::running && c.state.rax==7 && c.state.rip==7); }
    { Memory m(256); constexpr std::array<std::uint8_t,8> load{0x49,0x8b,0x04,0x25,128,0,0,0};
      constexpr std::array<std::uint8_t,8> data{9,0,0,0,0,0,0,0}; CHECK(m.write(0,load) && m.write(128,data));
      Cpu c; *c.state.reg(13)=33; CHECK(c.step(m)==StepResult::running && c.state.rax==9); }
    // Truncated SIB and RIP-relative instructions fault without changing state.
    for (const auto code : {std::array<std::uint8_t,8>{0x48,0x8b,0x04,0x25,128,0,0,0},
                            std::array<std::uint8_t,8>{0x48,0x8b,0x05,121,0,0,0,0}}) {
      const unsigned length=code[2]==0x05?7:8;
      for (unsigned n=1;n<length;++n) { Memory m(n); CHECK(m.write(0,std::span(code).first(n))); Cpu c; c.state.rax=55;
        CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==55); }
    }
    // Register PUSH/POP for all 16 registers, including RSP semantics.
    for (unsigned reg=0;reg<16;++reg) {
      Memory m(128); std::array<std::uint8_t,4> code{}; unsigned length;
      if (reg<8) { code[0]=0x50+reg; code[1]=0x58+reg; length=2; }
      else { code[0]=0x41; code[1]=0x50+(reg&7); code[2]=0x41; code[3]=0x58+(reg&7); length=4; }
      CHECK(m.write(0,std::span(code).first(length))); Cpu c;
      *c.state.reg(reg)=0x123456789abcdef0ULL; *c.state.reg(4)=96;
      const auto value=*c.state.reg(reg); c.state.rflags=0x402;
      CHECK(c.step(m)==StepResult::running && *c.state.reg(4)==88);
      CHECK(c.step(m)==StepResult::running && *c.state.reg(4)==96 && *c.state.reg(reg)==value && c.state.rflags==0x402);
    }
    // POP RSP must load the popped value instead of the incremented stack pointer.
    { Memory m(64); const std::array<std::uint8_t,1> op{0x5c}; const std::array<std::uint8_t,8> data{17,0,0,0,0,0,0,0};
      CHECK(m.write(0,op) && m.write(32,data)); Cpu c; *c.state.reg(4)=32;
      CHECK(c.step(m)==StepResult::running && *c.state.reg(4)==17); }
    // CALL and RET faults preserve RIP/RSP; no partial stack write.
    { Memory m(32); constexpr std::array<std::uint8_t,5> call{0xe8,0,0,0,0}; CHECK(m.write(0,call));
      CHECK(m.protect(28,1,Permission::read)); Cpu c; *c.state.reg(4)=32;
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && *c.state.reg(4)==32);
      std::array<std::uint8_t,8> data{}; CHECK(m.read(24,data)); CHECK((data==std::array<std::uint8_t,8>{})); }
    { Memory m(16); constexpr std::array<std::uint8_t,1> ret{0xc3}; CHECK(m.write(0,ret)); Cpu c; *c.state.reg(4)=16;
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && *c.state.reg(4)==16); }
    { Memory m(1); constexpr std::array<std::uint8_t,1> prefix{0x41}; CHECK(m.write(0,prefix)); Cpu c;
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0); }
    // mov rax,40; call add-two; hlt; add-two: add rax,2; ret.
    { Machine m(128); constexpr std::array<std::uint8_t,23> code{0x48,0xb8,40,0,0,0,0,0,0,0,
      0xe8,1,0,0,0,0xf4,0x48,0x05,2,0,0,0,0xc3}; CHECK(m.memory.write(0,code));
      *m.cpu.state.reg(4)=128; CHECK(m.run(5)==RunResult::halted && m.cpu.state.rax==42 && *m.cpu.state.reg(4)==128); }
}
