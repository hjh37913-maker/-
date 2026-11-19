#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
namespace {
constexpr std::uint64_t arithmetic_mask=0x8d5, test_mask=0x8c5;
struct Result { std::uint32_t value; std::uint64_t flags; };
Result native(std::uint32_t lhs,std::uint32_t rhs,bool sub,bool test) {
    std::uint64_t flags;
    if (test) asm volatile("testl %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    else if (sub) asm volatile("subl %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    else asm volatile("addl %[rhs], %[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    return {lhs,flags};
}
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
}
int main() {
    constexpr std::array<std::uint8_t,9> ops{0x89,0x8b,0x01,0x03,0x29,0x2b,0x39,0x3b,0x85};
    std::uint64_t rng=0x324249544f524143ULL; std::size_t cases=0;
    for (auto op:ops) for (unsigned dest=0;dest<8;++dest) for (unsigned src=0;src<8;++src) for (unsigned k=0;k<16;++k) {
        const bool reverse=op==0x8b || op==0x03 || op==0x2b || op==0x3b;
        const unsigned reg=reverse?dest:src,rm=reverse?src:dest;
        const std::array<std::uint8_t,2> code{op,static_cast<std::uint8_t>(0xc0|(reg<<3)|rm)};
        Memory mem(2); if (!mem.write(0,code)) return 1; Cpu cpu;
        std::array<std::uint64_t,8> before{};
        for (unsigned i=0;i<8;++i) { before[i]=next(rng); *cpu.state.reg(i)=before[i]; }
        constexpr std::uint64_t initial=0x412; cpu.state.rflags=initial;
        const bool mov=op==0x89||op==0x8b,test=op==0x85,cmp=op==0x39||op==0x3b,sub=cmp||op==0x29||op==0x2b;
        const auto reference=native(static_cast<std::uint32_t>(before[dest]),static_cast<std::uint32_t>(before[src]),sub,test);
        const auto want=(cmp||test)?before[dest]:(mov?static_cast<std::uint32_t>(before[src]):reference.value);
        const auto mask=test?test_mask:arithmetic_mask;
        if (cpu.step(mem)!=StepResult::running || cpu.state.rip!=2 || *cpu.state.reg(dest)!=want ||
            (mov?cpu.state.rflags!=initial:((cpu.state.rflags&mask)!=(reference.flags&mask) || (cpu.state.rflags&~mask)!=(initial&~mask)))) {
            std::cerr << "reg mismatch op=" << unsigned(op) << " dest=" << dest << " src=" << src << "\n"; return 1;
        }
        for (unsigned i=0;i<8;++i) if (i!=dest && *cpu.state.reg(i)!=before[i]) return 1;
        ++cases;
    }
    for (unsigned extension : {0u,5u,7u}) for (unsigned reg=0;reg<8;++reg) for (unsigned imm=0;imm<256;++imm) {
        const std::array<std::uint8_t,3> code{0x83,static_cast<std::uint8_t>(0xc0|(extension<<3)|reg),static_cast<std::uint8_t>(imm)};
        Memory mem(3); if (!mem.write(0,code)) return 1; Cpu cpu; const auto lhs=next(rng); *cpu.state.reg(reg)=lhs; cpu.state.rflags=0x412;
        const auto rhs=static_cast<std::uint32_t>((imm&0x80)?(0xffffff00u|imm):imm);
        const auto ref=native(static_cast<std::uint32_t>(lhs),rhs,extension!=0,false);
        if (cpu.step(mem)!=StepResult::running || *cpu.state.reg(reg)!=(extension==7?lhs:ref.value) ||
            (cpu.state.rflags&arithmetic_mask)!=(ref.flags&arithmetic_mask) || (cpu.state.rflags&~arithmetic_mask)!=(0x412&~arithmetic_mask)) {
            std::cerr << "imm mismatch ext=" << extension << " reg=" << reg << " imm=" << imm << '\n'; return 1;
        }
        ++cases;
    }
    std::cout << cases << " 32-bit register/immediate differential cases passed\n";
}
