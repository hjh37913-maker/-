#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
int main() {
 std::uint64_t rng=0x81feedf023485abdULL; unsigned count=0;
 for(unsigned width:{1u,2u,4u}) for(bool wide:{false,true}) for(unsigned i=0;i<8192;++i) {
  if(width==4 && !wide) continue;
  rng^=rng<<13; rng^=rng>>7; rng^=rng<<17;
  if(i<8) { constexpr std::array<std::uint32_t,8> values{0,1,0x7f,0x80,0x7fff,0x8000,0x7fffffff,0x80000000}; rng=values[i]; }
  const auto byte=std::uint8_t(rng); const auto word=std::uint16_t(rng); const auto dword=std::uint32_t(rng); std::uint64_t reference=0;
  if(wide) {
   if(width==1) asm volatile("movsbq %1,%0" : "=r"(reference) : "m"(byte));
   else if(width==2) asm volatile("movswq %1,%0" : "=r"(reference) : "m"(word));
   else asm volatile("movslq %1,%0" : "=r"(reference) : "m"(dword));
  } else {
   std::uint32_t out;
   if(width==1) asm volatile("movsbl %1,%0" : "=r"(out) : "m"(byte));
   else asm volatile("movswl %1,%0" : "=r"(out) : "m"(word));
   reference=out;
  }
  std::array<std::uint8_t,4> code{}; unsigned n=0; if(wide) code[n++]=0x48;
  if(width==4) code[n++]=0x63; else { code[n++]=0x0f; code[n++]=width==1?0xbe:0xbf; } code[n++]=0x03;
  std::array<std::uint8_t,4> data{}; for(unsigned j=0;j<4;++j) data[j]=rng>>(j*8);
  Memory mem(128); Cpu cpu; if(!mem.write(0,std::span(code).first(n)) || !mem.write(64,data)) return 1;
  cpu.state.rax=~std::uint64_t{0}; *cpu.state.reg(3)=64; cpu.state.rflags=0x8d7;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=n || cpu.state.rax!=reference || cpu.state.rflags!=0x8d7) { std::cerr<<"MOVSX mismatch width="<<width<<" wide="<<wide<<" index="<<i<<'\n'; return 1; }
  ++count;
 }
 std::cout<<count<<" native MOVSX/MOVSXD memory cases passed\n";
}
