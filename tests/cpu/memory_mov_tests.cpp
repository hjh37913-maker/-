#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    std::size_t cases=0;
    for (auto opcode : {std::uint8_t{0x89},std::uint8_t{0x8b}})
    for (unsigned mode=0;mode<3;++mode) for (unsigned reg=0;reg<16;++reg) for (unsigned base=0;base<16;++base) {
        if ((base&7)==4 || (mode==0 && (base&7)==5)) continue; // SIB and RIP-relative unsupported
        const std::array<std::uint8_t,7> instruction{
            static_cast<std::uint8_t>(0x48 | (reg>=8?4:0) | (base>=8?1:0)),opcode,
            static_cast<std::uint8_t>((mode<<6)|((reg&7)<<3)|(base&7)),0xf8,0xff,0xff,0xff};
        const unsigned length=mode==0?3:mode==1?4:7;
        Memory memory(256); CHECK(memory.write(0,std::span(instruction).first(length)));
        const std::array<std::uint8_t,8> input{0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
        CHECK(memory.write(163,input));
        Cpu cpu; for (unsigned i=0;i<16;++i) *cpu.state.reg(i)=0x1020304050607080ULL+i;
        *cpu.state.reg(base)=mode==0?163:171; // effective address 163, deliberately unaligned
        std::array<std::uint64_t,16> before{};
        for (unsigned i=0;i<16;++i) before[i]=*cpu.state.reg(i);
        cpu.state.rflags=0x402;
        CHECK(cpu.step(memory)==StepResult::running && cpu.state.rip==length && cpu.state.rflags==0x402);
        const auto expected=opcode==0x8b?0xefcdab8967452301ULL:before[reg];
        if (opcode==0x8b) CHECK(*cpu.state.reg(reg)==expected);
        for (unsigned i=0;i<16;++i) if (opcode==0x89 || i!=reg) CHECK(*cpu.state.reg(i)==before[i]);
        std::array<std::uint8_t,8> output{}; CHECK(memory.read(163,output));
        for (unsigned i=0;i<8;++i) CHECK(output[i]==static_cast<std::uint8_t>((opcode==0x89?expected:0xefcdab8967452301ULL)>>(8*i)));
        ++cases;
    }
    // A cross-permission access must fault before touching either CPU or memory.
    for (auto op : {std::uint8_t{0x89},std::uint8_t{0x8b}}) {
        Memory m(32); const std::array<std::uint8_t,3> ins{0x48,op,0x03}; // [rbx] <-> rax
        const std::array<std::uint8_t,8> initial{1,2,3,4,5,6,7,8};
        CHECK(m.write(0,ins) && m.write(16,initial));
        CHECK(m.protect(20,1,op==0x89?Permission::read:Permission::write));
        Cpu cpu; cpu.state.rax=0x8877665544332211ULL; *cpu.state.reg(3)=16; cpu.state.rflags=0x402;
        CHECK(cpu.step(m)==StepResult::memory_fault && cpu.state.rip==0 && cpu.state.rax==0x8877665544332211ULL && cpu.state.rflags==0x402);
        CHECK(m.protect(20,1,Permission::read)); std::array<std::uint8_t,8> out{}; CHECK(m.read(16,out) && out==initial);
    }
    // Out-of-range effective addresses must leave state untouched.
    { Memory m(3); const std::array<std::uint8_t,3> ins{0x48,0x8b,0x03}; CHECK(m.write(0,ins)); Cpu c;
      *c.state.reg(3)=UINT64_MAX; CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0); }
    for (auto mod : {std::uint8_t{0x40},std::uint8_t{0x80}}) {
        const std::array<std::uint8_t,7> ins{0x48,0x8b,static_cast<std::uint8_t>(mod|3),0xf8,0xff,0xff,0xff};
        const unsigned length=mod==0x40?4:7;
        for (unsigned n=3;n<length;++n) {
            Memory m(n); CHECK(m.write(0,std::span(ins).first(n))); Cpu c; c.state.rax=42;
            CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==42);
        }
    }
    std::cout << cases << " register/base/direction/addressing combinations passed\n";
}
