#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
namespace {
struct Native { std::uint64_t memory, reg, flags; };
Native native(std::uint8_t op,std::uint64_t m,std::uint64_t r) {
    std::uint64_t flags=0;
#define EXEC(text) asm volatile(text "\n\tpushfq\n\tpopq %[f]" : [m] "+m"(m), [r] "+&r"(r), [f] "=&r"(flags) : : "cc", "memory")
    switch(op) {
    case 0x01: EXEC("addq %[r], %[m]"); break;
    case 0x03: EXEC("addq %[m], %[r]"); break;
    case 0x29: EXEC("subq %[r], %[m]"); break;
    case 0x2b: EXEC("subq %[m], %[r]"); break;
    case 0x31: EXEC("xorq %[r], %[m]"); break;
    case 0x33: EXEC("xorq %[m], %[r]"); break;
    case 0x39: EXEC("cmpq %[r], %[m]"); break;
    case 0x3b: EXEC("cmpq %[m], %[r]"); break;
    case 0x85: EXEC("testq %[r], %[m]"); break;
    }
#undef EXEC
    return {m,r,flags};
}
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
std::array<std::uint8_t,8> bytes(std::uint64_t v) { std::array<std::uint8_t,8> out{}; for (unsigned i=0;i<8;++i) out[i]=static_cast<std::uint8_t>(v>>(i*8)); return out; }
}
int main() {
    std::uint64_t rng=0x5445535436344d45ULL; std::size_t cases=0;
    for (auto op : {0x01,0x03,0x29,0x2b,0x31,0x33,0x39,0x3b,0x85}) for (unsigned i=0;i<4096;++i) {
        auto lhs=next(rng), rhs=next(rng);
        if (i<8) { constexpr std::array<std::uint64_t,8> edge{0,1,~0ULL,0x8000000000000000ULL,0x7fffffffffffffffULL,0xf,0x10,0x100}; lhs=edge[i]; rhs=edge[7-i]; }
        const auto expected=native(static_cast<std::uint8_t>(op),lhs,rhs);
        Memory m(128); Cpu cpu; const std::array<std::uint8_t,3> code{0x48,static_cast<std::uint8_t>(op),0x03};
        if (!m.write(0,code) || !m.write(64,bytes(lhs))) return 1;
        *cpu.state.reg(3)=64; cpu.state.rax=rhs; cpu.state.rflags=0x412;
        const std::uint64_t mask=(op==0x85 || op==0x31 || op==0x33)?0x8c5:0x8d5;
        std::array<std::uint8_t,8> data{};
        if (cpu.step(m)!=StepResult::running || !m.read(64,data) || data!=bytes(expected.memory) || cpu.state.rax!=expected.reg ||
            cpu.state.rip!=3 || (cpu.state.rflags&mask)!=(expected.flags&mask) || (cpu.state.rflags&~mask)!=(0x412&~mask)) {
            std::cerr << "native 64-bit memory mismatch op=" << op << " iteration=" << i << '\n'; return 1;
        }
        ++cases;
    }
    std::cout << cases << " native 64-bit memory arithmetic/TEST/XOR cases passed\n";
}
