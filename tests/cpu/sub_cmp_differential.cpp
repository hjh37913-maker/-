#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>

// Native SUB provides the subtraction result and flags used by both SUB and CMP.
// Register source and the guest's sign-extended immediate have equal semantics.
namespace {
constexpr std::uint64_t arithmetic_flags = 0x8d5;
struct NativeResult { std::uint64_t value, flags; };
NativeResult native_sub(std::uint64_t lhs, std::uint64_t rhs) {
    std::uint64_t flags;
    asm volatile("subq %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]"
                 : [lhs] "+&r" (lhs), [flags] "=&r" (flags)
                 : [rhs] "r" (rhs) : "cc", "memory");
    return {lhs, flags};
}
std::uint64_t next(std::uint64_t& state) {
    state ^= state << 13; state ^= state >> 7; state ^= state << 17;
    return state;
}
bool check(std::uint64_t lhs, std::uint32_t immediate, std::uint8_t opcode, std::size_t index) {
    const std::array<std::uint8_t, 6> code{0x48, opcode, static_cast<std::uint8_t>(immediate),
        static_cast<std::uint8_t>(immediate >> 8), static_cast<std::uint8_t>(immediate >> 16),
        static_cast<std::uint8_t>(immediate >> 24)};
    const auto rhs = (immediate & 0x80000000u) ? std::uint64_t(immediate) | 0xffffffff00000000ULL : immediate;
    const auto native = native_sub(lhs, rhs);
    nightfall::Memory memory(code.size());
    if (!memory.write(0, code)) return false;
    nightfall::Cpu cpu; cpu.state.rax = lhs;
    constexpr std::uint64_t initial_flags = 0x402;
    cpu.state.rflags = initial_flags;
    const auto status = cpu.step(memory);
    const auto expected_value = opcode == 0x3d ? lhs : native.value;
    if (status != nightfall::StepResult::running || cpu.state.rip != 6 || cpu.state.rax != expected_value ||
        (cpu.state.rflags & arithmetic_flags) != (native.flags & arithmetic_flags) ||
        (cpu.state.rflags & ~arithmetic_flags) != (initial_flags & ~arithmetic_flags)) {
        std::cerr << "Mismatch #" << index << std::hex << " opcode=0x" << unsigned(opcode)
                  << " lhs=0x" << lhs << " imm=0x" << immediate << " result=0x" << cpu.state.rax
                  << " expected=0x" << expected_value << " flags=0x" << cpu.state.rflags
                  << " native flags=0x" << (native.flags & arithmetic_flags) << '\n';
        return false;
    }
    return true;
}
}
int main() {
    constexpr std::array<std::uint64_t, 12> operands{0,1,15,16,127,128,255,256,
        0x7fffffffffffffffULL,0x8000000000000000ULL,0xfffffffffffffffeULL,0xffffffffffffffffULL};
    constexpr std::array<std::uint32_t, 9> immediates{0,1,15,16,127,0x7fffffff,0x80000000,0xfffffffe,0xffffffff};
    std::size_t cases = 0;
    for (auto opcode : {std::uint8_t{0x2d}, std::uint8_t{0x3d}}) {
        for (auto lhs : operands) for (auto imm : immediates)
            if (!check(lhs, imm, opcode, ++cases)) return 1;
        std::uint64_t rng = 0x4e4947485446414cULL;
        for (unsigned i = 0; i < 20000; ++i) {
            const auto lhs = next(rng); const auto imm = static_cast<std::uint32_t>(next(rng));
            if (!check(lhs, imm, opcode, ++cases)) return 1;
        }
    }
    std::cout << cases << " native differential SUB and CMP cases passed\n";
}
