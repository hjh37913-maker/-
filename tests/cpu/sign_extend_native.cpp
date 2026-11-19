#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 std::uint64_t seed=0x11132478598aa311ULL; unsigned cases=0;
 for(unsigned kind=0;kind<6;++kind) for(unsigned i=0;i<16384;++i) {
  const auto a=next(seed), d=next(seed); std::uint64_t native_a=a,native_d=d;
  switch(kind) {
   case 0: asm volatile("cbtw" : "+a"(native_a)); break;
   case 1: asm volatile("cwtl" : "+a"(native_a)); break;
   case 2: asm volatile("cltq" : "+a"(native_a)); break;
   case 3: asm volatile("cwtd" : "+d"(native_d) : "a"(native_a)); break;
   case 4: asm volatile("cltd" : "+d"(native_d) : "a"(native_a)); break;
   default: asm volatile("cqto" : "+d"(native_d) : "a"(native_a)); break;
  }
  std::array<std::uint8_t,2> op{}; unsigned n=0;
  if(kind%3==0)op[n++]=0x66; else if(kind%3==2)op[n++]=0x48;
  op[n++]=kind<3?0x98:0x99; Memory mem(16); Cpu cpu;
  if(!mem.write(0,std::span(op).first(n))) return 1;
  cpu.state.rax=a; *cpu.state.reg(2)=d; cpu.state.rflags=0x8d7;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=n || cpu.state.rax!=native_a || *cpu.state.reg(2)!=native_d || cpu.state.rflags!=0x8d7) {
   std::cerr<<"sign extension mismatch kind="<<kind<<" i="<<i<<'\n'; return 1;
  }
  ++cases;
 }
 std::cout<<cases<<" native CBW/CWDE/CDQE/CWD/CDQ/CQO comparisons passed\n";
}
