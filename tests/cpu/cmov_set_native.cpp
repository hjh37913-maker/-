#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
struct Ref { std::uint64_t dest; std::uint8_t value; };
// Set only user-modifiable arithmetic flags, preserving host IF/other bits.
// Each condition is tested by the actual native CMOVcc and SETcc instructions.
#define DO(CC) do { \
 if(width==16) asm volatile("pushq %[f]; popfq; cmov" #CC "w %w[s],%w[d]; set" #CC " %b[t]" : [d] "+&r"(dest),[t] "=&q"(taken) : [s] "r"(source),[f] "r"(flags) : "cc","memory"); \
 else if(width==32) asm volatile("pushq %[f]; popfq; cmov" #CC "l %k[s],%k[d]; set" #CC " %b[t]" : [d] "+&r"(dest),[t] "=&q"(taken) : [s] "r"(source),[f] "r"(flags) : "cc","memory"); \
 else asm volatile("pushq %[f]; popfq; cmov" #CC "q %[s],%[d]; set" #CC " %b[t]" : [d] "+&r"(dest),[t] "=&q"(taken) : [s] "r"(source),[f] "r"(flags) : "cc","memory"); \
 return {dest,taken}; } while(0)
Ref native(unsigned cc,unsigned width,std::uint64_t dest,std::uint64_t source,std::uint64_t bits) {
 std::uint64_t flags; asm volatile("pushfq; popq %0" : "=r"(flags));
 flags=(flags&~std::uint64_t{0x8c5})|bits; std::uint8_t taken=0;
 switch(cc) {case 0:DO(o); case 1:DO(no); case 2:DO(b); case 3:DO(ae); case 4:DO(e); case 5:DO(ne); case 6:DO(be); case 7:DO(a); case 8:DO(s); case 9:DO(ns); case 10:DO(p); case 11:DO(np); case 12:DO(l); case 13:DO(ge); case 14:DO(le); case 15:DO(g);}
 return {dest,taken};
}
#undef DO
int main() {
 std::uint64_t rng=0x902bfa812c019a78ULL; unsigned count=0;
 for(unsigned cc=0;cc<16;++cc) for(unsigned width:{16u,32u,64u}) for(bool memory:{false,true}) for(unsigned mask=0;mask<32;++mask) for(unsigned j=0;j<16;++j) {
  rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; const auto dest=rng; rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; const auto source=rng;
  const std::uint64_t bits=((mask&1)?1:0)|((mask&2)?4:0)|((mask&4)?0x40:0)|((mask&8)?0x80:0)|((mask&16)?0x800:0);
  const auto ref=native(cc,width,dest,source,bits);
  std::array<std::uint8_t,5> code{}; unsigned n=0; if(width==16)code[n++]=0x66; else if(width==64)code[n++]=0x48;
  code[n++]=0x0f; code[n++]=std::uint8_t(0x40+cc); code[n++]=memory?0x03:0xc3;
  Memory mem(128); Cpu cpu; std::array<std::uint8_t,8> data{}; for(unsigned i=0;i<8;++i)data[i]=source>>(8*i);
  if(!mem.write(0,std::span(code).first(n))||!mem.write(64,data))return 1;
  cpu.state.rax=dest; *cpu.state.reg(3)=memory?64:source; cpu.state.rflags=bits|2;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=n || cpu.state.rax!=ref.dest || cpu.state.rflags!=(bits|2)) {
   std::cerr<<"CMOV mismatch cc="<<cc<<" width="<<width<<" memory="<<memory<<" flags="<<bits<<'\n'; return 1;
  }
  // Register and memory SETcc variants use the same input flag state.
  std::array<std::uint8_t,3> setcode{0x0f,std::uint8_t(0x90+cc),memory?std::uint8_t(0x03):std::uint8_t(0xc0)};
  if(!mem.write(0,setcode))return 1; cpu.state.rip=0; cpu.state.rax=dest; *cpu.state.reg(3)=memory?64:source; cpu.state.rflags=bits|2;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=3 || cpu.state.rflags!=(bits|2))return 1;
  if(memory) {std::array<std::uint8_t,1> out{}; if(!mem.read(64,out)||out[0]!=ref.value||cpu.state.rax!=dest)return 1;}
  else if(cpu.state.rax!=((dest&~0xffULL)|ref.value))return 1;
  ++count;
 }
 std::cout<<count<<" native CMOVcc and SETcc comparisons across all conditions/flag combinations\n";
}
