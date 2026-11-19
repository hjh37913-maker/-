#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    { Memory m(2); Cpu c; const std::array<std::uint8_t,2> code{0x31,0xc0}; CHECK(m.write(0,code));
      c.state.rax=0xabcdef0123456789; c.state.rflags=0x813; CHECK(c.step(m)==StepResult::running);
      CHECK(c.state.rax==0 && c.state.rflags==0x56 && c.state.rip==2); }
    { Memory m(3); Cpu c; const std::array<std::uint8_t,3> code{0x4d,0x33,0xc1}; CHECK(m.write(0,code));
      *c.state.reg(8)=0xffff000000000000ULL; *c.state.reg(9)=0x8000000000000001ULL;
      CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==0x7fff000000000001ULL); }
    { Memory m(64); Cpu c; const std::array<std::uint8_t,2> code{0x31,0x03}; const std::array<std::uint8_t,4> data{1,2,3,4};
      CHECK(m.write(0,code) && m.write(32,data) && m.protect(34,1,Permission::read)); c.state.rax=0x1234; *c.state.reg(3)=32; c.state.rflags=0x812;
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==0x1234 && c.state.rflags==0x812);
      std::array<std::uint8_t,4> result{}; CHECK(m.read(32,result) && result==data); }
    { Memory m(3); Cpu c; const std::array<std::uint8_t,3> code{0x48,0x31,0x03}; CHECK(m.write(0,code));
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0); }
    std::cout << "portable XOR cases passed\n";
}
