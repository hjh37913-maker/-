#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
static std::array<std::uint8_t,4> bytes(std::uint32_t x) {
    return {static_cast<std::uint8_t>(x),static_cast<std::uint8_t>(x>>8),static_cast<std::uint8_t>(x>>16),static_cast<std::uint8_t>(x>>24)};
}
static std::uint32_t value(const std::array<std::uint8_t,4>& x) {
    return std::uint32_t(x[0]) | (std::uint32_t(x[1])<<8) | (std::uint32_t(x[2])<<16) | (std::uint32_t(x[3])<<24);
}
int main() {
    // Compare memory and existing register forms with identical operands and initial flags.
    constexpr std::array<std::uint8_t,7> ops{0x01,0x03,0x29,0x2b,0x39,0x3b,0x85};
    std::uint32_t seed=0x8b25891;
    std::size_t count=0;
    for (auto op:ops) for (unsigned n=0;n<4096;++n) {
        seed=seed*1664525u+1013904223u; auto a=seed;
        seed=seed*1664525u+1013904223u; auto b=seed;
        if (n<8) { constexpr std::array<std::uint32_t,8> edge{0,1,0xffffffffu,0x80000000u,0x7fffffffu,0x10,0xf,0x100}; a=edge[n]; b=edge[7-n]; }
        Memory mem(64), refmem(2); Cpu c, ref;
        const std::array<std::uint8_t,2> instr{op,0x03}, reference{op,0xc1}; // [rbx],eax vs ecx,eax
        CHECK(mem.write(0,instr) && mem.write(32,bytes(a)) && refmem.write(0,reference));
        *c.state.reg(3)=32; *c.state.reg(0)=0x1234567800000000ULL|b;
        *ref.state.reg(1)=0xabcdef0100000000ULL|a; *ref.state.reg(0)=c.state.rax;
        c.state.rflags=ref.state.rflags=(n&1)?0x412:0x803;
        CHECK(c.step(mem)==StepResult::running && ref.step(refmem)==StepResult::running);
        std::array<std::uint8_t,4> data{}; CHECK(mem.read(32,data));
        const bool to_reg=op==0x03 || op==0x2b || op==0x3b;
        CHECK(c.state.rflags==ref.state.rflags && c.state.rax==ref.state.rax && c.state.rip==2);
        CHECK(value(data)==static_cast<std::uint32_t>(to_reg || op==0x39 || op==0x85 ? a : *ref.state.reg(1)));
        ++count;
    }
    // Sign-extended immediates and all supported extensions, RIP-relative end includes immediate.
    for (unsigned ext : {0u,5u,7u}) for (unsigned imm=0;imm<256;++imm) {
        Memory mem(64), refmem(3); Cpu c, ref;
        const std::array<std::uint8_t,7> instr{0x83,static_cast<std::uint8_t>((ext<<3)|5),25,0,0,0,static_cast<std::uint8_t>(imm)};
        const std::array<std::uint8_t,3> reference{0x83,static_cast<std::uint8_t>(0xc0|(ext<<3)),static_cast<std::uint8_t>(imm)};
        CHECK(mem.write(0,instr) && mem.write(32,bytes(0x80000042)) && refmem.write(0,reference));
        ref.state.rax=0x80000042; c.state.rflags=ref.state.rflags=0x412;
        CHECK(c.step(mem)==StepResult::running && ref.step(refmem)==StepResult::running);
        std::array<std::uint8_t,4> data{}; CHECK(mem.read(32,data));
        CHECK(value(data)==static_cast<std::uint32_t>(ref.state.rax) && c.state.rflags==ref.state.rflags && c.state.rip==7); ++count;
    }
    // Failed writes must not commit result or flags, including partial permissions.
    for (auto op : {std::uint8_t{0x01},std::uint8_t{0x29},std::uint8_t{0x83}}) {
        Memory mem(64); Cpu c; const std::array<std::uint8_t,3> instr{op,0x03,0xff};
        CHECK(mem.write(0,instr) && mem.write(32,bytes(0x12345678)) && mem.protect(34,1,Permission::read));
        c.state.rax=1; *c.state.reg(3)=32; c.state.rflags=0x812;
        CHECK(c.step(mem)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==1 && c.state.rflags==0x812);
        std::array<std::uint8_t,4> data{}; CHECK(mem.read(32,data) && value(data)==0x12345678);
    }
    // Read-only memory is legal for compare, test and register-destination arithmetic.
    for (auto op : {std::uint8_t{0x03},std::uint8_t{0x2b},std::uint8_t{0x39},std::uint8_t{0x3b},std::uint8_t{0x85}}) {
        Memory m(64); Cpu c; const std::array<std::uint8_t,2> code{op,0x03};
        CHECK(m.write(0,code) && m.write(32,bytes(5)) && m.protect(32,4,Permission::read));
        c.state.rax=7; *c.state.reg(3)=32; CHECK(c.step(m)==StepResult::running);
    }
    // Unreadable operands, truncated immediate and invalid extensions.
    { Memory m(64); Cpu c; const std::array<std::uint8_t,2> code{0x03,0x03}; CHECK(m.write(0,code) && m.protect(32,4,Permission::write));
      c.state.rax=1; *c.state.reg(3)=32; c.state.rflags=0x812;
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==1 && c.state.rflags==0x812); }
    { Memory m(2); Cpu c; const std::array<std::uint8_t,2> code{0x83,0x03}; CHECK(m.write(0,code));
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0); }
    { Memory m(2); Cpu c; const std::array<std::uint8_t,2> code{0x83,0x13}; CHECK(m.write(0,code));
      CHECK(c.step(m)==StepResult::invalid_instruction && c.state.rip==0); }
    std::cout << count << " memory/register cross-checks passed\n";
}
