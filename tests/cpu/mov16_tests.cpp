#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
 for(unsigned prefix=0;prefix<=8;++prefix) for(unsigned op:{0x89u,0x8bu,0xc7u}) for(unsigned reg=0;reg<8;++reg) for(unsigned rm=0;rm<8;++rm) {
  bool has_rex=prefix!=0; std::uint8_t rex=has_rex?0x3f+prefix:0; if(op==0xc7 && reg) continue;
  std::array<std::uint8_t,6> code{}; unsigned n=0; code[n++]=0x66; if(has_rex) code[n++]=rex; code[n++]=op; code[n++]=0xc0|(reg<<3)|rm; if(op==0xc7) { code[n++]=0x34; code[n++]=0x92; }
  Memory m(16); CHECK(m.write(0,std::span(code).first(n))); Cpu c; std::array<std::uint64_t,16> before{};
  for(unsigned i=0;i<16;++i) before[i]=*c.state.reg(i)=0xabcdef1234001000ULL+i*0x101;
  const unsigned r=reg+((rex&4)?8:0), q=rm+((rex&1)?8:0), dst=op==0x8b?r:q, src=op==0x8b?q:r;
  const auto value=op==0xc7?0x9234ULL:before[src]&0xffff; const auto want=(before[dst]&~0xffffULL)|value; c.state.rflags=0x8d7;
  CHECK(c.step(m)==StepResult::running && c.state.rip==n && c.state.rflags==0x8d7);
  for(unsigned i=0;i<16;++i) CHECK(*c.state.reg(i)==(i==dst?want:before[i]));
  for(unsigned k=1;k<n;++k) { Memory part(k); Cpu f; CHECK(part.write(0,std::span(code).first(k))); f.state.rax=42; f.state.rflags=0x8d7;
   CHECK(f.step(part)==StepResult::memory_fault && f.state.rip==0 && f.state.rax==42 && f.state.rflags==0x8d7); }
 }
 // Immediate register opcode with REX.B; 16-bit write preserves high 48 bits.
 { Memory m(5); std::array<std::uint8_t,5> code{0x66,0x41,0xb8,0x34,0x92}; CHECK(m.write(0,code)); Cpu c; *c.state.reg(8)=0x123456789abcdef0ULL; c.state.rflags=0x8d7;
   CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==0x123456789abc9234ULL && c.state.rflags==0x8d7); }
 for(unsigned op:{0x89u,0x8bu,0xc7u}) {
  std::array<std::uint8_t,10> code{}; unsigned n=0; code[n++]=0x66; code[n++]=op; code[n++]=0x05; unsigned len=7+(op==0xc7?2:0); code[n++]=64-len; code[n++]=0; code[n++]=0; code[n++]=0; if(op==0xc7) { code[n++]=0x55; code[n++]=0xaa; }
  Memory m(128); const std::array<std::uint8_t,2> data{0x34,0x12}; CHECK(m.write(0,std::span(code).first(n)) && m.write(64,data)); Cpu c; c.state.rax=0x12345678abcdef99ULL; c.state.rflags=0x8d7;
  CHECK(c.step(m)==StepResult::running && c.state.rip==n && c.state.rflags==0x8d7); std::array<std::uint8_t,2> out{}; CHECK(m.read(64,out));
  if(op==0x8b) CHECK(c.state.rax==0x12345678abcd1234ULL && out==data); else CHECK(out[0]==(op==0x89?0x99:0x55) && out[1]==(op==0x89?0xef:0xaa));
  // Fault on second byte; no partial write or register update.
  CHECK(m.protect(65,1,op==0x8b?Permission::write:Permission::read)); Cpu f; f.state.rax=0x12345678abcdef99ULL; f.state.rflags=0x8d7;
  CHECK(f.step(m)==StepResult::memory_fault && f.state.rip==0 && f.state.rax==0x12345678abcdef99ULL && f.state.rflags==0x8d7); CHECK(m.read(64,out)==(op!=0x8b));
 }
}
