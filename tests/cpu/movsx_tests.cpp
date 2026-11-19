#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
std::uint64_t expected(std::uint64_t v,unsigned bits,bool wide) {
 const auto mask=(std::uint64_t{1}<<bits)-1; v&=mask;
 if(v&(std::uint64_t{1}<<(bits-1))) v|=~mask;
 return wide?v:std::uint32_t(v);
}
int main() {
 for(unsigned prefix=0;prefix<=16;++prefix) for(unsigned width:{1u,2u,4u}) for(unsigned reg=0;reg<8;++reg) for(unsigned rm=0;rm<8;++rm) for(bool negative:{false,true}) {
  bool rex=prefix!=0; std::uint8_t r=rex?0x3f+prefix:0; if(width==4 && !(r&8)) continue;
  std::array<std::uint8_t,4> code{}; unsigned n=0; if(rex) code[n++]=r;
  if(width==4) code[n++]=0x63; else { code[n++]=0x0f; code[n++]=width==1?0xbe:0xbf; } code[n++]=0xc0|(reg<<3)|rm;
  Memory m(16); CHECK(m.write(0,std::span(code).first(n))); Cpu c; std::array<std::uint64_t,16> before{};
  for(unsigned i=0;i<16;++i) before[i]=*c.state.reg(i)=negative ? 0xfabcdeff89abcdefULL-i : 0x1234560071234567ULL+i;
  unsigned dst=reg+((r&4)?8:0),src=rm+((r&1)?8:0); auto v=before[src]; if(width==1 && !rex && rm>=4) v=before[rm-4]>>8;
  auto want=expected(v,width*8,(r&8)!=0); c.state.rflags=0x8d7;
  CHECK(c.step(m)==StepResult::running && c.state.rip==n && c.state.rflags==0x8d7);
  for(unsigned i=0;i<16;++i) CHECK(*c.state.reg(i)==(i==dst?want:before[i]));
  for(unsigned k=1;k<n;++k) { Memory p(k); Cpu f; CHECK(p.write(0,std::span(code).first(k))); f.state.rax=42; f.state.rflags=0x8d7;
   CHECK(f.step(p)==StepResult::memory_fault && f.state.rip==0 && f.state.rax==42 && f.state.rflags==0x8d7); }
 }
 // RIP-relative operands, including a read fault on only the final byte.
 for(unsigned width:{1u,2u,4u}) for(bool negative:{false,true}) {
  std::array<std::uint8_t,8> code{}; unsigned n=0; code[n++]=0x4c;
  if(width==4) code[n++]=0x63; else { code[n++]=0x0f; code[n++]=width==1?0xbe:0xbf; } code[n++]=0x05;
  const auto len=n+4; code[n++]=64-len; code[n++]=0; code[n++]=0; code[n++]=0;
  std::array<std::uint8_t,4> data{0x81,0x80,0x80,0x80}; if(!negative) data[width-1]=0x70;
  Memory m(128); CHECK(m.write(0,std::span(code).first(n)) && m.write(64,data)); Cpu c; c.state.rflags=0x8d7;
  std::uint64_t value=0; for(unsigned i=0;i<width;++i) value|=std::uint64_t(data[i])<<(8*i);
  CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==expected(value,width*8,true) && c.state.rflags==0x8d7);
  CHECK(m.protect(64+width-1,1,Permission::write)); Cpu f; *f.state.reg(8)=999; f.state.rflags=0x8d7;
  CHECK(f.step(m)==StepResult::memory_fault && f.state.rip==0 && *f.state.reg(8)==999 && f.state.rflags==0x8d7);
 }
 // The non-REX.W 63 encoding is intentionally unsupported, not silently MOVSXD.
 { Memory m(2); std::array<std::uint8_t,2> code{0x63,0xc0}; CHECK(m.write(0,code)); Cpu c; CHECK(c.step(m)==StepResult::invalid_instruction && c.state.rip==0); }
}
