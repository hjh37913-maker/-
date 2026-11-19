#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 std::uint64_t seed=0x786349b1301fedbaULL; unsigned cases=0;
 constexpr std::array<std::uint64_t,12> edges{0,1,2,3,0x7fffffffULL,0x80000000ULL,0xffffffffULL,
  0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffffffffffffULL,0xfffffffffffffffeULL,0x100000000ULL};
 for(unsigned width:{16u,32u,64u}) for(bool signed_op:{false,true}) for(bool memory:{false,true}) for(unsigned i=0;i<16384;++i) {
  auto a=next(seed), b=next(seed); if(i<edges.size()*edges.size()){a=edges[i/edges.size()]; b=edges[i%edges.size()];}
  std::uint64_t low=0,high=0,flags=0;
  if(width==64) {
   low=a;
   if(signed_op) asm volatile("imulq %[src]; pushfq; popq %[flags]" : "+a"(low), "=&d"(high), [flags] "=r"(flags) : [src] "m"(b) : "cc");
   else asm volatile("mulq %[src]; pushfq; popq %[flags]" : "+a"(low), "=&d"(high), [flags] "=r"(flags) : [src] "m"(b) : "cc");
  } else if(width==32) {
   auto x=std::uint32_t(a), y=std::uint32_t(b); std::uint32_t hi;
   if(signed_op) asm volatile("imull %[src]; pushfq; popq %[flags]" : "+a"(x), "=&d"(hi), [flags] "=r"(flags) : [src] "m"(y) : "cc");
   else asm volatile("mull %[src]; pushfq; popq %[flags]" : "+a"(x), "=&d"(hi), [flags] "=r"(flags) : [src] "m"(y) : "cc");
   low=x; high=hi;
  } else {
   auto x=std::uint16_t(a), y=std::uint16_t(b); std::uint16_t hi;
   if(signed_op) asm volatile("imulw %[src]; pushfq; popq %[flags]" : "+a"(x), "=&d"(hi), [flags] "=r"(flags) : [src] "m"(y) : "cc");
   else asm volatile("mulw %[src]; pushfq; popq %[flags]" : "+a"(x), "=&d"(hi), [flags] "=r"(flags) : [src] "m"(y) : "cc");
   low=(a&~0xffffULL)|x; high=(0x123456789abcdef0ULL&~0xffffULL)|hi;
  }
  std::array<std::uint8_t,4> code{}; unsigned n=0;
  if(width==16)code[n++]=0x66; else if(width==64)code[n++]=0x48;
  code[n++]=0xf7; code[n++]=std::uint8_t((memory?0:0xc0)|(signed_op?0x28:0x20)|3);
  Memory mem(128); Cpu cpu; std::array<std::uint8_t,8> bytes{}; for(unsigned j=0;j<8;++j)bytes[j]=b>>(j*8);
  if(!mem.write(0,std::span(code).first(n)) || !mem.write(64,bytes)) return 1;
  cpu.state.rax=a; *cpu.state.reg(2)=0x123456789abcdef0ULL; *cpu.state.reg(3)=memory?64:b; cpu.state.rflags=0x2d6;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=n || cpu.state.rax!=low || *cpu.state.reg(2)!=high || (cpu.state.rflags&0x801)!=(flags&0x801) || (cpu.state.rflags&~std::uint64_t{0x801})!=(0x2d6&~0x801)) {
   std::cerr<<"MUL mismatch width="<<width<<" signed="<<signed_op<<" memory="<<memory<<" i="<<i<<" a="<<a<<" b="<<b<<" low="<<cpu.state.rax<<" reference="<<low<<" high="<<*cpu.state.reg(2)<<" refhigh="<<high<<'\n'; return 1;
  }
  ++cases;
 }
 for(bool signed_op:{false,true}) for(bool memory:{false,true}) for(unsigned i=0;i<65536;++i) {
  const auto a=std::uint8_t(i), b=std::uint8_t(i>>8); std::uint16_t result=a; std::uint64_t flags=0;
  if(signed_op) asm volatile("imulb %[src]; pushfq; popq %[flags]" : "+a"(result), [flags] "=r"(flags) : [src] "m"(b) : "cc");
  else asm volatile("mulb %[src]; pushfq; popq %[flags]" : "+a"(result), [flags] "=r"(flags) : [src] "m"(b) : "cc");
  Memory mem(128); Cpu cpu; const std::array<std::uint8_t,2> code{0xf6,std::uint8_t((memory?0:0xc0)|(signed_op?0x28:0x20)|3)};
  const std::array<std::uint8_t,1> value{b}; if(!mem.write(0,code)||!mem.write(64,value))return 1;
  cpu.state.rax=0x123456789abc0000ULL|a; *cpu.state.reg(3)=memory?64:b; cpu.state.rflags=0x2d6;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=2 || cpu.state.rax!=(0x123456789abc0000ULL|result) || (cpu.state.rflags&0x801)!=(flags&0x801) || (cpu.state.rflags&~std::uint64_t{0x801})!=(0x2d6&~0x801)) {
   std::cerr<<"byte MUL mismatch signed="<<signed_op<<" memory="<<memory<<" i="<<i<<'\n'; return 1;
  }
  ++cases;
 }
 std::cout<<cases<<" native MUL/IMUL full-product comparisons passed (defined CF/OF)\n";
}
