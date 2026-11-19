#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
unsigned idx(unsigned low,bool rex,bool ext) { return !rex && low>=4 ? low-4 : low+(ext?8:0); }
unsigned shift(unsigned low,bool rex) { return !rex && low>=4 ? 8 : 0; }
int main() {
 for(unsigned prefix=0;prefix<=16;++prefix) for(unsigned op:{0x88u,0x8au,0xc6u}) for(unsigned r=0;r<8;++r) for(unsigned rm=0;rm<8;++rm) {
  bool rex=prefix!=0; std::uint8_t p=rex?0x3f+prefix:0; if(op==0xc6 && r!=0) continue;
  std::array<std::uint8_t,5> code{}; unsigned n=0; if(rex) code[n++]=p; code[n++]=op; code[n++]=0xc0|(r<<3)|rm; if(op==0xc6) code[n++]=0xa5;
  Memory m(16); CHECK(m.write(0,std::span(code).first(n))); Cpu c; std::array<std::uint64_t,16> before{};
  for(unsigned i=0;i<16;++i) before[i]=*c.state.reg(i)=0xfedcba9876543000ULL+(i*0x101)+0x23;
  const bool dest_reg=op==0x8a; const unsigned dl=dest_reg?r:rm, sl=dest_reg?rm:r;
  const unsigned di=idx(dl,rex,dest_reg?(p&4):(p&1)), si=idx(sl,rex,dest_reg?(p&1):(p&4));
  const unsigned ds=shift(dl,rex), ss=shift(sl,rex); const auto val=op==0xc6?0xa5:((before[si]>>ss)&255);
  auto want=(before[di]&~(std::uint64_t{255}<<ds))|(val<<ds); c.state.rflags=0x8d7;
  CHECK(c.step(m)==StepResult::running && c.state.rip==n && c.state.rflags==0x8d7);
  for(unsigned i=0;i<16;++i) CHECK(*c.state.reg(i)==(i==di?want:before[i]));
  for(unsigned k=1;k<n;++k) { Memory small(k); Cpu f; CHECK(small.write(0,std::span(code).first(k))); f.state.rax=42; f.state.rflags=0x8d7;
    CHECK(f.step(small)==StepResult::memory_fault && f.state.rip==0 && f.state.rax==42 && f.state.rflags==0x8d7); }
 }
 // RIP-relative addressing, including immediate byte after displacement.
 for(unsigned op:{0x88u,0x8au,0xc6u}) {
  std::array<std::uint8_t,7> code{}; unsigned n=0; code[n++]=op; code[n++]=0x05; unsigned len=6+(op==0xc6?1:0); code[n++]=64-len; code[n++]=0; code[n++]=0; code[n++]=0; if(op==0xc6) code[n++]=0x95;
  Memory m(128); const std::array<std::uint8_t,1> val{0xf1}; CHECK(m.write(0,std::span(code).first(n)) && m.write(64,val)); Cpu c; c.state.rax=0x1234; c.state.rflags=0x8d7;
  CHECK(c.step(m)==StepResult::running && c.state.rip==n && c.state.rflags==0x8d7);
  std::array<std::uint8_t,1> got{}; CHECK(m.read(64,got));
  CHECK(op==0x8a ? (c.state.rax==0x12f1 && got[0]==0xf1) : (c.state.rax==0x1234 && got[0]==(op==0x88?0x34:0x95)));
  CHECK(m.protect(64,1,op==0x8a?Permission::write:Permission::read)); Cpu f; f.state.rax=0x1234; f.state.rflags=0x8d7;
  CHECK(f.step(m)==StepResult::memory_fault && f.state.rip==0 && f.state.rax==0x1234 && f.state.rflags==0x8d7);
 }
 // C6 /1 is not MOV and must not execute.
 { std::array<std::uint8_t,3> code{0xc6,0xc8,1}; Memory m(3); Cpu c; CHECK(m.write(0,code)); CHECK(c.step(m)==StepResult::invalid_instruction && c.state.rip==0); }
}
