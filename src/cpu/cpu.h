#pragma once
#include "memory/memory.h"
#include <array>
#include <cstdint>
namespace nightfall {
enum class StepResult { running, halted, invalid_instruction, memory_fault };
struct CpuState {
    std::uint64_t rax = 0;
    // Encoded register indices: 0=RAX, 1=RCX, 2=RDX, 3=RBX,
    // 4=RSP, 5=RBP, 6=RSI, 7=RDI, 8..15=R8..R15.
    std::array<std::uint64_t,15> other_registers{};
    std::uint64_t* reg(unsigned index) noexcept {
        return index == 0 ? &rax : index < 16 ? &other_registers[index-1] : nullptr;
    }
    const std::uint64_t* reg(unsigned index) const noexcept {
        return index == 0 ? &rax : index < 16 ? &other_registers[index-1] : nullptr;
    }
    std::uint64_t rip = 0;
    // Arithmetic and logical operations update modeled flags; other flags are preserved.
    std::uint64_t rflags = 2;
};
class Cpu {
public:
    CpuState state;
    StepResult step(Memory& memory);
};
}
