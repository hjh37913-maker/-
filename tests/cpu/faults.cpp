#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
int main() {
    constexpr std::array<std::uint8_t,10> mov{0x48,0xb8,1,2,3,4,5,6,7,8};
    constexpr std::array<std::uint8_t,6> add{0x48,0x05,1,2,3,4};
    constexpr std::array<std::uint8_t,6> sub{0x48,0x2d,1,2,3,4};
    constexpr std::array<std::uint8_t,6> cmp{0x48,0x3d,1,2,3,4};
    auto test = [](auto code) {
        for (std::size_t length = 0; length < code.size(); ++length) {
            Memory m(length); if (!m.write(0, std::span(code).first(length))) return false;
            Cpu c; c.state.rax = 42; c.state.rflags = 0x402;
            if (c.step(m) != StepResult::memory_fault || c.state.rip != 0 || c.state.rax != 42 || c.state.rflags != 0x402) return false;
        }
        Memory m(code.size()); if (!m.write(0, code)) return false;
        Cpu c; return c.step(m) == StepResult::running && c.state.rip == code.size();
    };
    { Memory m(2); constexpr std::array<std::uint8_t,2> unsupported{0x48,0x21};
      if (!m.write(0,unsupported)) return 1;
      Cpu c; c.state.rax=42; c.state.rflags=0x402;
      if (c.step(m)!=StepResult::invalid_instruction || c.state.rip!=0 || c.state.rax!=42 || c.state.rflags!=0x402) {
          std::cerr << "Unsupported REX opcode did not stop cleanly\n"; return 1;
      }
    }
    { Memory m(2); constexpr std::array<std::uint8_t,2> truncated_indirect{0x48,0xff};
      if (!m.write(0,truncated_indirect)) return 1;
      Cpu c; c.state.rax=42; c.state.rflags=0x402;
      if (c.step(m)!=StepResult::memory_fault || c.state.rip!=0 || c.state.rax!=42 || c.state.rflags!=0x402) return 1;
    }
    if (!test(mov) || !test(add) || !test(sub) || !test(cmp)) { std::cerr << "Truncated instruction fault regression\n"; return 1; }
    return 0;
}
