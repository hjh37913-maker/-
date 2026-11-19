#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
 // Every REX combination, both source widths and register encodings including aliases.
 for(unsigned prefix=0;prefix<=16;++prefix) for(unsigned op:{0xb6u,0xb7u}) for(unsigned reg=0;reg<8;++reg) for(unsigned rm=0;rm<8;++rm) {
  bool rex=prefix!=0; std::uint8_t r=rex?0x3f+prefix:0;
  std::array<std::uint8_t,4> code{}; unsigned n=0; if(rex) code[n++]=r; code[n++]=0x0f; code[n++]=op; code[n++]=0xc0|(reg<<3)|rm;
  Memory m(16); CHECK(m.write(0,std::span(code).first(n))); Cpu c; std::array<std::uint64_t,16> before{};
  for(unsigned i=0;i<16;++i) before[i]=*c.state.reg(i)=0xabcdef1234000000ULL+(i*0x1100)+0x80+i;
  unsigned dst=reg+((r&4)?8:0), src=rm+((r&1)?8:0); std::uint64_t want;
  if(op==0xb6 && !rex && rm>=4) want=(before[rm-4]>>8)&255;
  else want=before[src]&(op==0xb6?255:65535);
  c.state.rflags=0x8d7; CHECK(c.step(m)==StepResult::running && c.state.rip==n && c.state.rflags==0x8d7);
  for(unsigned i=0;i<16;++i) CHECK(*c.state.reg(i)==(i==dst?want:before[i]));
  for(unsigned k=1;k<n;++k) { Memory small(k); Cpu f; CHECK(small.write(0,std::span(code).first(k))); f.state.rax=42; f.state.rflags=0x8d7;
    CHECK(f.step(small)==StepResult::memory_fault && f.state.rip==0 && f.state.rax==42 && f.state.rflags==0x8d7); }
 }
 // RIP-relative with signed displacement, bounds and read permissions.
 for(unsigned op:{0xb6u,0xb7u}) for(bool rex:{false,true}) {
  std::array<std::uint8_t,8> code{}; unsigned n=0; if(rex) code[n++]=0x4c; code[n++]=0x0f; code[n++]=op; code[n++]=0x05;
  const auto len=n+4; code[n++]=64-len; code[n++]=0; code[n++]=0; code[n++]=0;
  Memory m(128); std::array<std::uint8_t,2> data{0xab,0xcd}; CHECK(m.write(0,std::span(code).first(n)) && m.write(64,data)); Cpu c; c.state.rflags=0x8d7;
  CHECK(c.step(m)==StepResult::running && *c.state.reg(rex?8:0)==(op==0xb6?0xab:0xcdab) && c.state.rflags==0x8d7);
  CHECK(m.protect(64,2,Permission::write)); Cpu fault; fault.state.rax=123; fault.state.rflags=0x8d7;
  CHECK(fault.step(m)==StepResult::memory_fault && fault.state.rip==0 && fault.state.rax==123 && fault.state.rflags==0x8d7);
 }
}
