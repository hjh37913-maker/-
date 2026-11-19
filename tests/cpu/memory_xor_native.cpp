#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
namespace {
struct Native { std::uint32_t memory, reg; std::uint64_t flags; };
Native native_mem(std::uint8_t op,std::uint32_t m,std::uint32_t r) {
    std::uint64_t flags=0;
#define EXEC(text) asm volatile(text "\n\tpushfq\n\tpopq %[f]" : [m] "+m"(m), [r] "+&r"(r), [f] "=&r"(flags) : : "cc", "memory")
    switch(op) {
    case 0x01: EXEC("addl %[r], %[m]"); break;
    case 0x03: EXEC("addl %[m], %[r]"); break;
    case 0x29: EXEC("subl %[r], %[m]"); break;
    case 0x2b: EXEC("subl %[m], %[r]"); break;
    case 0x31: EXEC("xorl %[r], %[m]"); break;
    case 0x33: EXEC("xorl %[m], %[r]"); break;
    case 0x39: EXEC("cmpl %[r], %[m]"); break;
    case 0x3b: EXEC("cmpl %[m], %[r]"); break;
    case 0x85: EXEC("testl %[r], %[m]"); break;
    }
#undef EXEC
    return {m,r,flags};
}
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
std::array<std::uint8_t,4> bytes(std::uint32_t v) { return {std::uint8_t(v),std::uint8_t(v>>8),std::uint8_t(v>>16),std::uint8_t(v>>24)}; }
std::uint32_t value(const std::array<std::uint8_t,4>& v) { return std::uint32_t(v[0]) | (std::uint32_t(v[1])<<8) | (std::uint32_t(v[2])<<16) | (std::uint32_t(v[3])<<24); }
}
int main() {
    std::uint64_t rng=0x41524954484d454dULL; std::size_t cases=0;
    for (auto op : {0x01,0x03,0x29,0x2b,0x31,0x33,0x39,0x3b,0x85}) for (unsigned i=0;i<4096;++i) {
        auto lhs=static_cast<std::uint32_t>(next(rng)); auto rhs=static_cast<std::uint32_t>(next(rng));
        if (i<8) { constexpr std::array<std::uint32_t,8> edge{0,1,0xffffffffu,0x80000000u,0x7fffffffu,0xf,0x10,0x100}; lhs=edge[i]; rhs=edge[7-i]; }
        const auto expected=native_mem(static_cast<std::uint8_t>(op),lhs,rhs);
        Memory m(64); Cpu cpu; const std::array<std::uint8_t,2> code{static_cast<std::uint8_t>(op),0x03};
        if (!m.write(0,code) || !m.write(32,bytes(lhs))) return 1;
        *cpu.state.reg(3)=32; cpu.state.rax=0xabcdef1200000000ULL|rhs; cpu.state.rflags=0x412;
        const bool modifies_reg=op==0x03 || op==0x2b || op==0x33;
        const auto want_rax=modifies_reg ? std::uint64_t(expected.reg) : (0xabcdef1200000000ULL|rhs);
        const std::uint64_t mask=(op==0x85 || op==0x31 || op==0x33)?0x8c5:0x8d5;
        std::array<std::uint8_t,4> data{};
        if (cpu.step(m)!=StepResult::running || !m.read(32,data) || value(data)!=expected.memory || cpu.state.rax!=want_rax ||
            cpu.state.rip!=2 || (cpu.state.rflags&mask)!=(expected.flags&mask) || (cpu.state.rflags&~mask)!=(0x412&~mask)) {
            std::cerr << "native memory mismatch op=" << op << " iteration=" << i << '\n'; return 1;
        }
        ++cases;
    }
    // Both encodings and all legacy register pairs, including aliases, for 32-bit XOR.
    for (auto op : {0x31,0x33}) for (unsigned reg=0;reg<8;++reg) for (unsigned rm=0;rm<8;++rm) for (unsigned i=0;i<32;++i) {
        Memory mem(2); Cpu cpu; const std::array<std::uint8_t,2> code{std::uint8_t(op),std::uint8_t(0xc0|(reg<<3)|rm)};
        if (!mem.write(0,code)) return 1;
        std::array<std::uint64_t,8> before{}; for (unsigned j=0;j<8;++j) { before[j]=next(rng); *cpu.state.reg(j)=before[j]; }
        const auto dst=op==0x33?reg:rm,src=op==0x33?rm:reg; auto result=std::uint32_t(before[dst]), operand=std::uint32_t(before[src]); std::uint64_t flags;
        asm volatile("xorl %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(result), [flags] "=&r"(flags) : [rhs] "r"(operand) : "cc","memory");
        cpu.state.rflags=0x412;
        if (cpu.step(mem)!=StepResult::running || cpu.state.rip!=2 || *cpu.state.reg(dst)!=result ||
            (cpu.state.rflags&0x8c5)!=(flags&0x8c5) || (cpu.state.rflags&~0x8c5)!=(0x412&~0x8c5)) return 1;
        for (unsigned j=0;j<8;++j) if (j!=dst && *cpu.state.reg(j)!=before[j]) return 1;
        ++cases;
    }
    // Native 64-bit XOR reference, including aliased source/destination and REX extensions.
    for (auto op : {0x31,0x33}) for (unsigned reg=0;reg<16;++reg) for (unsigned rm=0;rm<16;++rm) for (unsigned i=0;i<16;++i) {
        Memory mem(3); Cpu cpu; const std::array<std::uint8_t,3> code{std::uint8_t(0x48|(reg>=8?4:0)|(rm>=8?1:0)),std::uint8_t(op),std::uint8_t(0xc0|((reg&7)<<3)|(rm&7))};
        if (!mem.write(0,code)) return 1;
        std::array<std::uint64_t,16> before{}; for (unsigned j=0;j<16;++j) { before[j]=next(rng); *cpu.state.reg(j)=before[j]; }
        const auto dst=op==0x33?reg:rm,src=op==0x33?rm:reg; auto result=before[dst], operand=before[src]; std::uint64_t flags;
        asm volatile("xorq %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(result), [flags] "=&r"(flags) : [rhs] "r"(operand) : "cc","memory");
        cpu.state.rflags=0x412;
        if (cpu.step(mem)!=StepResult::running || cpu.state.rip!=3 || *cpu.state.reg(dst)!=result ||
            (cpu.state.rflags&0x8c5)!=(flags&0x8c5) || (cpu.state.rflags&~0x8c5)!=(0x412&~0x8c5)) return 1;
        for (unsigned j=0;j<16;++j) if (j!=dst && *cpu.state.reg(j)!=before[j]) return 1;
        ++cases;
    }
    std::cout << cases << " native memory arithmetic and 32/64-bit XOR differential cases passed\n";
}
