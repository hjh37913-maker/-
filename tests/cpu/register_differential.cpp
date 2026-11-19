#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
namespace {
constexpr std::uint64_t mask=0x8d5;
struct Result { std::uint64_t value, flags; };
Result native(std::uint64_t lhs,std::uint64_t rhs,bool subtract) {
    std::uint64_t flags;
    if (subtract) asm volatile("subq %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]"
        : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    else asm volatile("addq %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]"
        : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    return {lhs,flags};
}
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
}
int main() {
    constexpr std::array<std::uint8_t,8> ops{0x89,0x8b,0x01,0x03,0x29,0x2b,0x39,0x3b};
    std::uint64_t rng=0x5245474953544552ULL; std::size_t cases=0;
    for (auto op:ops) for (unsigned dest=0;dest<16;++dest) for (unsigned src=0;src<16;++src) for (unsigned k=0;k<16;++k) {
        const bool reverse=op==0x8b || op==0x03 || op==0x2b || op==0x3b;
        const unsigned reg=reverse?dest:src, rm=reverse?src:dest;
        const std::array<std::uint8_t,3> code{static_cast<std::uint8_t>(0x48 | (reg>=8?4:0) | (rm>=8?1:0)), op,
            static_cast<std::uint8_t>(0xc0 | ((reg&7)<<3) | (rm&7))};
        Memory memory(3); if (!memory.write(0,code)) return 1;
        Cpu cpu; std::array<std::uint64_t,16> before{};
        for (unsigned i=0;i<16;++i) { before[i]=next(rng); *cpu.state.reg(i)=before[i]; }
        cpu.state.rflags=0x402;
        const bool move=op==0x89||op==0x8b, subtract=op==0x29||op==0x2b||op==0x39||op==0x3b;
        const bool compare=op==0x39||op==0x3b;
        const auto result=native(before[dest],before[src],subtract);
        const auto expected=move?before[src]:(compare?before[dest]:result.value);
        if (cpu.step(memory)!=StepResult::running || cpu.state.rip!=3 || *cpu.state.reg(dest)!=expected ||
            (move ? cpu.state.rflags!=0x402 : ((cpu.state.rflags&mask)!=(result.flags&mask) || (cpu.state.rflags&~mask)!=(0x402&~mask)))) {
            std::cerr << "Mismatch opcode=" << unsigned(op) << " dest=" << dest << " src=" << src << " iteration=" << k << '\n'; return 1;
        }
        for (unsigned i=0;i<16;++i) if (i!=dest && *cpu.state.reg(i)!=before[i]) { std::cerr<<"Other register changed\n"; return 1; }
        ++cases;
    }
    std::cout << cases << " register instruction differential cases passed\n";
}
