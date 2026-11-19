#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
namespace {
constexpr std::uint64_t mask=0x8d5;
struct Result { std::uint64_t value, flags; };
Result native(std::uint64_t lhs,std::uint64_t rhs,bool sub) {
    std::uint64_t flags;
    if (sub) asm volatile("subq %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    else asm volatile("addq %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    return {lhs,flags};
}
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
}
int main() {
    std::uint64_t rng=0x494d4d384f524143ULL; std::size_t cases=0;
    for (unsigned extension : {0u,5u,7u}) for (unsigned reg=0;reg<16;++reg) for (unsigned immediate=0;immediate<256;++immediate) {
        const std::array<std::uint8_t,4> code{static_cast<std::uint8_t>(0x48|(reg>=8?1:0)),0x83,
            static_cast<std::uint8_t>(0xc0|(extension<<3)|(reg&7)),static_cast<std::uint8_t>(immediate)};
        Memory memory(4); if (!memory.write(0,code)) return 1;
        Cpu cpu; const auto lhs=next(rng); *cpu.state.reg(reg)=lhs; cpu.state.rflags=0x402;
        const auto rhs=immediate&0x80 ? 0xffffffffffffff00ULL | immediate : immediate;
        const auto expected=native(lhs,rhs,extension!=0);
        if (cpu.step(memory)!=StepResult::running || cpu.state.rip!=4 ||
            *cpu.state.reg(reg)!=(extension==7?lhs:expected.value) ||
            (cpu.state.rflags&mask)!=(expected.flags&mask) || (cpu.state.rflags&~mask)!=(0x402&~mask)) {
            std::cerr << "Mismatch extension=" << extension << " reg=" << reg << " imm=" << immediate << '\n'; return 1;
        }
        ++cases;
    }
    std::cout << cases << " native imm8 differential cases passed\n";
}
