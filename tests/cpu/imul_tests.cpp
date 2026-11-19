#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if(!(x)) { std::cerr << __LINE__ << ": " #x "\n"; return 1; } } while(0)
int main() {
 // REX.W IMUL R8, [RIP+disp32], imm8: tests trailing immediate in RIP base.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,8> op{0x4c,0x6b,0x05,0x38,0,0,0,0xfe};
   const std::array<std::uint8_t,8> val{7,0,0,0,0,0,0,0}; CHECK(m.write(0,op)&&m.write(64,val)); c.state.rflags=0x8d7;
   CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==std::uint64_t{0}-14 && c.state.rip==8 && c.state.rflags==(0x8d7&~0x801)); }
 // No result or flag update on truncated instruction or unreadable operand.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,4> op{0x48,0x0f,0xaf,0x03}; CHECK(m.write(0,op));
   c.state.rax=3; *c.state.reg(3)=64; c.state.rflags=0x8d7; CHECK(m.protect(64,8,Permission::write));
   CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==3 && c.state.rflags==0x8d7); }
 { Memory m(4); Cpu c; const std::array<std::uint8_t,4> op{0x48,0x6b,0xc0,0x7f}; CHECK(m.write(0,op)); CHECK(m.protect(3,1,Permission::read)); c.state.rax=3; c.state.rflags=0x8d7;
   CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==3 && c.state.rflags==0x8d7); }
 // 32-bit writes zero extend and extended registers are decoded.
 { Memory m(32); Cpu c; const std::array<std::uint8_t,4> op{0x45,0x0f,0xaf,0xc1}; CHECK(m.write(0,op)); *c.state.reg(8)=0xffffffffffffffffULL; *c.state.reg(9)=2;
   CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==0xfffffffe && (c.state.rflags&0x801)==0); }
 // 16-bit result preserves high bits; negative overflow sets CF and OF.
 { Memory m(32); Cpu c; const std::array<std::uint8_t,5> op{0x66,0x45,0x6b,0xc1,0xfe}; CHECK(m.write(0,op));
   *c.state.reg(8)=0x12340000; *c.state.reg(9)=0x8000;
   CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==0x12340000 && (c.state.rflags&0x801)==0x801); }
 // CWD/CDQ/CQO sign extend while preserving flags and appropriate upper bits.
 for(unsigned kind=0;kind<3;++kind) for(bool neg:{false,true}) {
   Memory m(16); Cpu c; std::array<std::uint8_t,2> op{kind==0?std::uint8_t(0x66):kind==1?std::uint8_t(0x99):std::uint8_t(0x48),0x99};
   const unsigned len=kind==1?1:2; CHECK(m.write(0,std::span(op).first(len)));
   c.state.rax=neg?(kind==0?0x8000ULL:kind==1?0x80000000ULL:0x8000000000000000ULL):0; *c.state.reg(2)=0x123456789abcdef0ULL; c.state.rflags=0x8d7;
   const auto expected=kind==0?(0x123456789abc0000ULL|(neg?0xffffULL:0)):kind==1?(neg?0xffffffffULL:0):(neg?~std::uint64_t{0}:0);
   CHECK(c.step(m)==StepResult::running && *c.state.reg(2)==expected && c.state.rip==len && c.state.rflags==0x8d7);
 }
 // CBW/CWDE/CDQE and untouched flags on signed boundary values.
 for(unsigned kind=0;kind<3;++kind) for(bool neg:{false,true}) {
   Memory m(16); Cpu c; std::array<std::uint8_t,2> op{kind==0?std::uint8_t(0x66):kind==1?std::uint8_t(0x98):std::uint8_t(0x48),0x98};
   const unsigned len=kind==1?1:2; CHECK(m.write(0,std::span(op).first(len)));
   c.state.rax=kind==0?(neg?0x12340080ULL:0x1234007fULL):kind==1?(neg?0x12348000ULL:0x12347fffULL):(neg?0x123480000000ULL:0x12347fffffffULL); c.state.rflags=0x8d7;
   const auto expected=kind==0?(neg?0x1234ff80ULL:0x1234007fULL):kind==1?(neg?0xffff8000ULL:0x7fffULL):(neg?0xffffffff80000000ULL:0x7fffffffULL);
   CHECK(c.step(m)==StepResult::running && c.state.rax==expected && c.state.rip==len && c.state.rflags==0x8d7);
 }
 // All register encodings, aliases and REX extensions for two-operand IMUL.
 for(unsigned width:{16u,32u,64u}) for(unsigned dst=0;dst<16;++dst) for(unsigned src=0;src<16;++src) {
   Memory m(16); Cpu c; std::array<std::uint8_t,5> code{}; unsigned n=0;
   if(width==16) code[n++]=0x66;
   code[n++]=std::uint8_t(0x40|(width==64?8:0)|(dst>=8?4:0)|(src>=8?1:0));
   code[n++]=0x0f; code[n++]=0xaf; code[n++]=std::uint8_t(0xc0|((dst&7)<<3)|(src&7));
   CHECK(m.write(0,std::span(code).first(n)));
   *c.state.reg(src)=7; *c.state.reg(dst)=~std::uint64_t{2}; c.state.rflags=0x8d7;
   const auto product=dst==src?std::uint64_t{9}:std::uint64_t{0}-21;
   const auto expected=width==16?(~std::uint64_t{0xffff}|(product&0xffff)):width==32?product&0xffffffffULL:product;
   CHECK(c.step(m)==StepResult::running && *c.state.reg(dst)==expected && c.state.rip==n && c.state.rflags==(0x8d7&~0x801));
 }
 std::cout << "IMUL portable decoding, faults and RIP-relative cases passed\n";
}
