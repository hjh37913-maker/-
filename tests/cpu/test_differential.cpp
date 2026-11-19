#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
namespace {
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
std::uint64_t native(std::uint64_t lhs,std::uint64_t rhs) {
    std::uint64_t flags;
    asm volatile("testq %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]"
        : [flags] "=&r"(flags) : [lhs] "r"(lhs), [rhs] "r"(rhs) : "cc","memory");
    return flags;
}
}
int main() {
    // AF is undefined by x86 TEST and is deliberately excluded from comparison.
    constexpr std::uint64_t defined=(1ULL<<0)|(1ULL<<2)|(1ULL<<6)|(1ULL<<7)|(1ULL<<11);
    std::uint64_t rng=0x54455354464c4147ULL; std::size_t cases=0;
    for (unsigned reg=0;reg<16;++reg) for (unsigned rm=0;rm<16;++rm) for (unsigned i=0;i<32;++i) {
        const std::array<std::uint8_t,3> code{static_cast<std::uint8_t>(0x48|(reg>=8?4:0)|(rm>=8?1:0)),
            0x85,static_cast<std::uint8_t>(0xc0|((reg&7)<<3)|(rm&7))};
        Memory mem(3); if (!mem.write(0,code)) return 1;
        Cpu cpu; std::array<std::uint64_t,16> before{};
        for (unsigned k=0;k<16;++k) { before[k]=next(rng); *cpu.state.reg(k)=before[k]; }
        if (i==0) { before[reg]=0; *cpu.state.reg(reg)=0; }
        constexpr std::uint64_t initial=0x412;
        cpu.state.rflags=initial;
        const auto flags=native(before[reg],before[rm]);
        if (cpu.step(mem)!=StepResult::running || cpu.state.rip!=3 ||
            (cpu.state.rflags&defined)!=(flags&defined) || (cpu.state.rflags&~defined)!=(initial&~defined)) {
            std::cerr << "TEST mismatch reg=" << reg << " rm=" << rm << " iteration=" << i << '\n'; return 1;
        }
        for (unsigned k=0;k<16;++k) if (*cpu.state.reg(k)!=before[k]) return 1;
        ++cases;
    }
    std::cout << cases << " native TEST differential cases passed\n";
}
