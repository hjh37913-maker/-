#include "core/machine.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
static int errors = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; ++errors; } } while (0)
int main() {
    // Taken/untaken JZ and JNZ, forward and negative offsets, flags preserved.
    for (auto opcode : {std::uint8_t{0x74}, std::uint8_t{0x75}}) {
        for (bool zf : {false, true}) {
            Machine m(32); const std::array<std::uint8_t,2> p{opcode, 3};
            CHECK(m.memory.write(4,p)); m.cpu.state.rip = 4; m.cpu.state.rflags = 2 | (zf ? 0x40 : 0);
            const auto before = m.cpu.state.rflags;
            CHECK(m.cpu.step(m.memory) == StepResult::running);
            CHECK(m.cpu.state.rip == ((opcode == 0x74 ? zf : !zf) ? 9u : 6u));
            CHECK(m.cpu.state.rflags == before);
        }
    }
    { Machine m(4); const std::array<std::uint8_t,2> p{0xeb,0xfe};
      CHECK(m.memory.write(0,p)); CHECK(m.run(10)==RunResult::budget_exhausted); CHECK(m.cpu.state.rip==0); }
    { Machine m(2); const std::array<std::uint8_t,2> p{0xeb,0xfd};
      CHECK(m.memory.write(0,p)); CHECK(m.cpu.step(m.memory)==StepResult::running); CHECK(m.cpu.state.rip==UINT64_MAX);
      CHECK(m.cpu.step(m.memory)==StepResult::memory_fault); CHECK(m.cpu.state.rip==UINT64_MAX); }
    for (auto opcode : {std::uint8_t{0xeb},std::uint8_t{0x74},std::uint8_t{0x75}}) {
      Machine m(1); const std::array<std::uint8_t,1> p{opcode}; CHECK(m.memory.write(0,p)); m.cpu.state.rax=9;
      CHECK(m.cpu.step(m.memory)==StepResult::memory_fault); CHECK(m.cpu.state.rip==0 && m.cpu.state.rax==9 && m.cpu.state.rflags==2);
    }
    // Synthetic loop: RAX=3; subtract one; compare with zero; jump if not equal; halt.
    { Machine m(32); const std::array<std::uint8_t,25> p{
        0x48,0xb8,3,0,0,0,0,0,0,0, 0x48,0x2d,1,0,0,0,
        0x48,0x3d,0,0,0,0, 0x75,0xf2,0xf4};
      CHECK(m.memory.write(0,p)); CHECK(m.run(11)==RunResult::halted);
      CHECK(m.cpu.state.rax==0); CHECK(m.cpu.state.rip==25); CHECK((m.cpu.state.rflags & 0x40)!=0); }
    return errors ? 1 : 0;
}
