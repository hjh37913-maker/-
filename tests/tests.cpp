#include "core/machine.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
using namespace nightfall;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x "\n"; ++failures; } } while (0)
int main() {
    { Memory m(4); std::array<std::uint8_t,2> a{1,2}, out{};
      CHECK(m.write(2,a)); CHECK(m.read(2,out)); CHECK(out==a);
      CHECK(!m.write(3,a)); CHECK(!m.read(UINT64_MAX,out)); CHECK(m.read(4,std::span(out).first(0)));
      CHECK(m.read(2,out)); CHECK(out==a); }
    { Machine m(32); const std::array<std::uint8_t,17> p{0x48,0xb8,40,0,0,0,0,0,0,0,0x48,0x05,2,0,0,0,0xf4};
      CHECK(m.memory.write(0,p)); CHECK(m.run(2)==RunResult::budget_exhausted); CHECK(m.cpu.state.rax==42); CHECK(m.run(1)==RunResult::halted); CHECK(m.cpu.state.rip==17); }
    { Machine m(6); const std::array<std::uint8_t,6> p{0x48,0x05,1,0,0,0}; CHECK(m.memory.write(0,p));
      m.cpu.state.rax=UINT64_MAX; CHECK(m.cpu.step(m.memory)==StepResult::running); CHECK(m.cpu.state.rax==0);
      CHECK((m.cpu.state.rflags & 0xd5)==0x55); CHECK(!(m.cpu.state.rflags & 0x800)); }
    { Machine m(6); const std::array<std::uint8_t,6> p{0x48,0x05,0xff,0xff,0xff,0xff}; CHECK(m.memory.write(0,p));
      m.cpu.state.rax=0x8000000000000000ULL; CHECK(m.cpu.step(m.memory)==StepResult::running);
      CHECK(m.cpu.state.rax==0x7fffffffffffffffULL); CHECK(m.cpu.state.rflags & 0x800); CHECK(m.cpu.state.rflags & 1); }
    { Machine m(2); const std::array<std::uint8_t,2> p{0x48,0xb8}; CHECK(m.memory.write(0,p)); m.cpu.state.rax=123;
      CHECK(m.run(1)==RunResult::memory_fault); CHECK(m.cpu.state.rip==0); CHECK(m.cpu.state.rax==123); }
    { Machine m(1); const std::array<std::uint8_t,1> p{0xcc}; CHECK(m.memory.write(0,p));
      CHECK(m.run(1)==RunResult::invalid_instruction); CHECK(m.cpu.state.rip==0); CHECK(m.run(0)==RunResult::budget_exhausted); }
    if (failures) return 1;
    std::cout << "All checks passed\n";
}
