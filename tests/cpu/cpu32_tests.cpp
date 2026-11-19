#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    { Memory m(2); const std::array<std::uint8_t,2> code{0x89,0xc0}; CHECK(m.write(0,code)); Cpu c;
      c.state.rax=0x1234567887654321ULL; c.state.rflags=0x412;
      CHECK(c.step(m)==StepResult::running && c.state.rax==0x87654321ULL && c.state.rflags==0x412); }
    { Memory m(2); const std::array<std::uint8_t,2> code{0x85,0xc0}; CHECK(m.write(0,code)); Cpu c;
      c.state.rax=0xffffffff00000000ULL; c.state.rflags=0x812;
      CHECK(c.step(m)==StepResult::running && (c.state.rflags&0x40) && !(c.state.rflags&0x801) && c.state.rax==0xffffffff00000000ULL); }
    { Memory m(3); const std::array<std::uint8_t,3> code{0x83,0xf8,0xff}; CHECK(m.write(0,code)); Cpu c;
      c.state.rax=0x12345678ffffffffULL; CHECK(c.step(m)==StepResult::running && c.state.rax==0x12345678ffffffffULL && (c.state.rflags&0x40)); }
    for (auto op : {std::uint8_t{0x01},std::uint8_t{0x85},std::uint8_t{0x83}}) {
      const std::array<std::uint8_t,3> code{op,0,1}; Memory m(3); CHECK(m.write(0,code)); Cpu c; c.state.rax=42;
      CHECK(c.step(m)==StepResult::invalid_instruction && c.state.rax==42 && c.state.rip==0);
      Memory truncated(1); CHECK(truncated.write(0,std::span(code).first(1))); Cpu d;
      CHECK(d.step(truncated)==StepResult::memory_fault && d.state.rip==0);
    }
    { Memory m(2); const std::array<std::uint8_t,2> code{0x83,0xc0}; CHECK(m.write(0,code)); Cpu c;
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0); }
    { Memory m(16); const std::array<std::uint8_t,5> code{0xe9,0xf9,0xff,0xff,0xff}; CHECK(m.write(4,code)); Cpu c; c.state.rip=4;
      CHECK(c.step(m)==StepResult::running && c.state.rip==2); }
    for (unsigned n=1;n<5;++n) { const std::array<std::uint8_t,5> code{0xe9,1,0,0,0}; Memory m(n); CHECK(m.write(0,std::span(code).first(n))); Cpu c;
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0); }
}
