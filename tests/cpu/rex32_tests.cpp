#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(0)
std::uint64_t next(std::uint64_t& x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
int main() {
 std::uint64_t rng=0xaab13388e196c2d7ULL; unsigned cases=0;
 for(auto op:{0x01u,0x03u,0x29u,0x2bu,0x31u,0x33u,0x09u,0x0bu,0x21u,0x23u,0x39u,0x3bu,0x85u,0x89u,0x8bu})
 for(unsigned reg=0;reg<16;++reg) for(unsigned rm=0;rm<16;++rm) for(unsigned trial=0;trial<8;++trial) {
  Memory a(64),b(64); Cpu baseline,extended; const bool same=reg==rm; const auto x=next(rng), y=next(rng);
  const std::array<std::uint8_t,2> code{std::uint8_t(op),std::uint8_t(0xc0|((same?0:3)<<3))};
  const std::array<std::uint8_t,3> rex{std::uint8_t(0x40|(reg>=8?4:0)|(rm>=8?1:0)),std::uint8_t(op),std::uint8_t(0xc0|((reg&7)<<3)|(rm&7))};
  CHECK(a.write(0,code)&&b.write(0,rex)); baseline.state.rax=x; if(!same)*baseline.state.reg(3)=y;
  *extended.state.reg(rm)=x; if(!same)*extended.state.reg(reg)=y; baseline.state.rflags=extended.state.rflags=0x8d7;
  CHECK(baseline.step(a)==StepResult::running && extended.step(b)==StepResult::running);
  CHECK(extended.state.rip==3 && baseline.state.rip==2 && baseline.state.rflags==extended.state.rflags && *baseline.state.reg(0)==*extended.state.reg(rm));
  if(!same)CHECK(*baseline.state.reg(3)==*extended.state.reg(reg)); ++cases;
 }
 // Extended addressing: R12 base, R9 index, R10D destination, scale 2.
 { Memory m(256); Cpu c; const std::array<std::uint8_t,5> op{0x47,0x03,0x54,0x4c,0xf8}; const std::array<std::uint8_t,4> value{9,0,0,0};
   CHECK(m.write(0,op)&&m.write(64,value)); *c.state.reg(12)=56; *c.state.reg(9)=8; *c.state.reg(10)=0xffffffff00000004ULL;
   CHECK(c.step(m)==StepResult::running && *c.state.reg(10)==13 && c.state.rip==5); }
 // Memory RMW must not commit flags or register state on failed write.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,3> op{0x45,0x01,0x01}; const std::array<std::uint8_t,4> value{3,0,0,0};
   CHECK(m.write(0,op)&&m.write(64,value)&&m.protect(64,4,Permission::read)); *c.state.reg(9)=64; *c.state.reg(8)=4; c.state.rflags=0x8d7; const auto before=c.state;
   CHECK(c.step(m)==StepResult::memory_fault && c.state.rax==before.rax && c.state.other_registers==before.other_registers && c.state.rip==before.rip && c.state.rflags==before.rflags); }
 // Prefix R is ignored for group extensions, while B extends destination.
 { Memory m(32); Cpu c; const std::array<std::uint8_t,4> op{0x45,0x83,0xc0,0xff}; CHECK(m.write(0,op)); *c.state.reg(8)=2;
   CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==1 && c.state.rip==4); }
 // Multi-byte NOP decodes displacement/SIB without accessing target memory.
 { const std::array<std::uint8_t,8> op{0x0f,0x1f,0x84,0x00,0,0,0,0}; Memory m(8); Cpu c; CHECK(m.write(0,op)); c.state.rax=UINT64_MAX; c.state.rflags=0x8d7;
   CHECK(c.step(m)==StepResult::running && c.state.rip==8 && c.state.rax==UINT64_MAX && c.state.rflags==0x8d7);
   for(unsigned n=1;n<op.size();++n){Memory t(n);Cpu d;CHECK(t.write(0,std::span(op).first(n))); d.state.rax=3;CHECK(d.step(t)==StepResult::memory_fault && d.state.rip==0 && d.state.rax==3);} }
 { Memory m(4); Cpu c; const std::array<std::uint8_t,4> op{0x0f,0x1f,0xc8,0}; CHECK(m.write(0,op)); CHECK(c.step(m)==StepResult::invalid_instruction && c.state.rip==0); }
 std::cout<<cases<<" REX32 and unprefixed register equivalence cases, plus address and NOP tests passed\n";
}
