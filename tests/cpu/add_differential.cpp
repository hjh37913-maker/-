#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>

// This oracle is only built on x86-64 with GNU-style inline assembly. It
// compares the immediate instruction against native ADD with an equivalent
// sign-extended register operand. Both forms have identical arithmetic flags.
namespace {
constexpr std::uint64_t arithmetic_flags = 0x8d5; // CF PF AF ZF SF OF
struct NativeResult { std::uint64_t value, flags; };
NativeResult native_add(std::uint64_t lhs, std::uint64_t rhs) {
    std::uint64_t flags;
    asm volatile("addq %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]"
                 : [lhs] "+&r" (lhs), [flags] "=&r" (flags)
                 : [rhs] "r" (rhs) : "cc", "memory");
    return {lhs, flags};
}
std::uint64_t sign_extend(std::uint32_t value) {
    return value & 0x80000000u ? std::uint64_t(value) | 0xffffffff00000000ULL : value;
}
bool check(std::uint64_t lhs, std::uint32_t immediate, std::size_t index) {
    const std::array<std::uint8_t, 6> code{
        0x48, 0x05, static_cast<std::uint8_t>(immediate),
        static_cast<std::uint8_t>(immediate >> 8), static_cast<std::uint8_t>(immediate >> 16),
        static_cast<std::uint8_t>(immediate >> 24)};
    nightfall::Memory memory(code.size());
    if (!memory.write(0, code)) return false;
    nightfall::Cpu cpu;
    cpu.state.rax = lhs;
    constexpr std::uint64_t initial_flags = 0x402; // Preserve DF and reserved bit.
    cpu.state.rflags = initial_flags;
    const auto expected = native_add(lhs, sign_extend(immediate));
    const auto status = cpu.step(memory);
    if (status != nightfall::StepResult::running || cpu.state.rip != 6 ||
        cpu.state.rax != expected.value ||
        (cpu.state.rflags & arithmetic_flags) != (expected.flags & arithmetic_flags) ||
        (cpu.state.rflags & ~arithmetic_flags) != (initial_flags & ~arithmetic_flags)) {
        std::cerr << "Mismatch #" << index << std::hex << " lhs=0x" << lhs
                  << " imm=0x" << immediate << " got value=0x" << cpu.state.rax
                  << " flags=0x" << cpu.state.rflags << " expected value=0x"
                  << expected.value << " arithmetic flags=0x" << (expected.flags & arithmetic_flags) << '\n';
        return false;
    }
    return true;
}
std::uint64_t next(std::uint64_t& state) {
    state ^= state << 13; state ^= state >> 7; state ^= state << 17;
    return state;
}
}
int main() {
    constexpr std::array<std::uint64_t, 12> operands{
        0, 1, 15, 16, 127, 128, 255, 256, 0x7fffffffffffffffULL,
        0x8000000000000000ULL, 0xfffffffffffffffeULL, 0xffffffffffffffffULL};
    constexpr std::array<std::uint32_t, 9> immediates{
        0, 1, 15, 16, 127, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff};
    std::size_t cases = 0;
    for (auto lhs : operands) for (auto imm : immediates)
        if (!check(lhs, imm, ++cases)) return 1;
    // Fixed seed makes failures reproducible, including on future compilers.
    std::uint64_t rng = 0x4e4947485446414cULL;
    for (unsigned i = 0; i < 20000; ++i) {
        const auto lhs = next(rng);
        const auto imm = static_cast<std::uint32_t>(next(rng));
        if (!check(lhs, imm, ++cases)) return 1;
    }
    std::cout << cases << " native differential ADD cases passed\n";
}
