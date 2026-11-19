#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
namespace {
struct Result { std::uint64_t value, flags; };
Result native64(std::uint64_t lhs,std::uint64_t rhs,unsigned ext) {
    std::uint64_t flags;
    if (ext==0) asm volatile("addq %[rhs],%[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    else asm volatile("subq %[rhs],%[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    return {lhs,flags};
}
Result native32(std::uint32_t lhs,std::uint32_t rhs,unsigned ext) {
    std::uint64_t flags;
    if (ext==0) asm volatile("addl %[rhs],%[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    else asm volatile("subl %[rhs],%[lhs]\n\tpushfq\n\tpopq %[flags]" : [lhs] "+&r"(lhs),[flags] "=&r"(flags) : [rhs] "r"(rhs) : "cc","memory");
    return {lhs,flags};
}
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
std::array<std::uint8_t,8> bytes(std::uint64_t v) { std::array<std::uint8_t,8> a{}; for(unsigned i=0;i<8;++i) a[i]=static_cast<std::uint8_t>(v>>(8*i)); return a; }
}
int main() {
    std::uint64_t rng=0x776f7264696d6d31ULL; std::size_t count=0;
    for (unsigned width : {32u,64u}) for (bool memory : {false,true}) for (unsigned ext : {0u,5u,7u}) for (unsigned i=0;i<2048;++i) {
        auto lhs=next(rng); std::uint32_t imm=static_cast<std::uint32_t>(next(rng));
        if (i<8) { constexpr std::array<std::uint32_t,8> edge{0,1,0xffffffffu,0x80000000u,0x7fffffffu,0x10,0xff,0xffff0000u}; imm=edge[i]; lhs=edge[7-i]; }
        auto rhs=std::uint64_t(imm); if (imm & 0x80000000u) rhs|=0xffffffff00000000ULL;
        const auto ref=width==32?native32(static_cast<std::uint32_t>(lhs),imm,ext):native64(lhs,rhs,ext);
        const auto original=width==32?static_cast<std::uint32_t>(lhs):lhs;
        const auto expected=ext==7?original:ref.value;
        Cpu cpu; Memory mem(128); std::array<std::uint8_t,11> code{}; unsigned pos=0;
        const unsigned dest=i%(width==32?8:16);
        if (width==64) code[pos++]=static_cast<std::uint8_t>(0x48|(!memory && dest>=8?1:0));
        code[pos++]=0x81;
        if (memory) { code[pos++]=static_cast<std::uint8_t>((ext<<3)|5); const unsigned len=(width==64?11:10); code[pos++]=static_cast<std::uint8_t>(64-len); code[pos++]=0;code[pos++]=0;code[pos++]=0; }
        else code[pos++]=static_cast<std::uint8_t>(0xc0|(ext<<3)|(dest&7));
        for(unsigned j=0;j<4;++j) code[pos++]=static_cast<std::uint8_t>(imm>>(j*8));
        const auto initial=bytes(lhs);
        if (!mem.write(0,std::span(code).first(pos)) || !mem.write(64,std::span(initial).first(width/8))) return 1;
        *cpu.state.reg(dest)=lhs; cpu.state.rflags=0x412;
        if (cpu.step(mem)!=StepResult::running || cpu.state.rip!=pos || (cpu.state.rflags&0x8d5)!=(ref.flags&0x8d5) || (cpu.state.rflags&~0x8d5)!=(0x412&~0x8d5)) {
            std::cerr<<"flags/decode failure width="<<width<<" memory="<<memory<<" ext="<<ext<<" i="<<i<<'\n'; return 1;
        }
        if (memory) {
            std::array<std::uint8_t,8> got{}; if (!mem.read(64,std::span(got).first(width/8))) return 1;
            auto want=bytes(expected); for(unsigned j=0;j<width/8;++j) if(got[j]!=want[j]) { std::cerr<<"memory result failure\n"; return 1; }
            if (*cpu.state.reg(dest)!=lhs) return 1;
        } else {
            const auto want=ext==7?lhs:expected;
            if (*cpu.state.reg(dest)!=want) { std::cerr<<"register result failure\n"; return 1; }
        }
        ++count;
    }
    std::cout<<count<<" native arithmetic comparisons for group-1 imm32 (native register arithmetic oracle) passed\n";
}
