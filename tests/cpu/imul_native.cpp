#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 std::uint64_t seed=0x194712ec33ba7845ULL; unsigned cases=0;
 constexpr std::array<std::uint64_t,12> edges{0,1,2,3,0x7fffffffULL,0x80000000ULL,0xffffffffULL,
  0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffffffffffffULL,0xfffffffffffffffeULL,0x100000000ULL};
 for(bool wide:{false,true}) for(unsigned form=0;form<4;++form) for(unsigned i=0;i<16384;++i) {
  auto a=next(seed), b=next(seed); if(i<edges.size()*edges.size()){a=edges[i/edges.size()]; b=edges[i%edges.size()];}
  std::uint64_t reference=0, flags=0;
  if(wide) {
   if(form==0 || form==1) { reference=a; asm volatile("imulq %[src],%[dst]; pushfq; popq %[flags]" : [dst] "+r"(reference), [flags] "=r"(flags) : [src] "m"(b) : "cc"); }
   else if(form==2) asm volatile("imulq $-7,%[src],%[dst]; pushfq; popq %[flags]" : [dst] "=r"(reference), [flags] "=r"(flags) : [src] "r"(b) : "cc");
   else asm volatile("imulq $0x71234567,%[src],%[dst]; pushfq; popq %[flags]" : [dst] "=r"(reference), [flags] "=r"(flags) : [src] "r"(b) : "cc");
  } else {
   auto src=std::uint32_t(b); std::uint32_t result=std::uint32_t(a);
   if(form==0 || form==1) asm volatile("imull %[src],%[dst]; pushfq; popq %[flags]" : [dst] "+r"(result), [flags] "=r"(flags) : [src] "m"(src) : "cc");
   else if(form==2) asm volatile("imull $-7,%[src],%[dst]; pushfq; popq %[flags]" : [dst] "=r"(result), [flags] "=r"(flags) : [src] "r"(src) : "cc");
   else asm volatile("imull $0x71234567,%[src],%[dst]; pushfq; popq %[flags]" : [dst] "=r"(result), [flags] "=r"(flags) : [src] "r"(src) : "cc");
   reference=result;
  }
  Memory mem(128); Cpu cpu; std::array<std::uint8_t,8> code{}, data{}; unsigned n=0;
  if(wide) code[n++]=0x48;
  if(form<2) { code[n++]=0x0f; code[n++]=0xaf; code[n++]=form==0?0xc3:0x03; }
  else { code[n++]=form==2?0x6b:0x69; code[n++]=0xc3; if(form==2)code[n++]=0xf9; else {code[n++]=0x67;code[n++]=0x45;code[n++]=0x23;code[n++]=0x71;} }
  for(unsigned j=0;j<8;++j)data[j]=b>>(j*8);
  if(!mem.write(0,std::span(code).first(n))||!mem.write(64,data))return 1;
  cpu.state.rax=a; *cpu.state.reg(3)=form==1?64:b; cpu.state.rflags=0x2d6;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=n || cpu.state.rax!=reference || (cpu.state.rflags&0x801)!=(flags&0x801) || (cpu.state.rflags&~std::uint64_t{0x801})!=(0x2d6&~0x801)) {
   std::cerr<<"IMUL mismatch wide="<<wide<<" form="<<form<<" i="<<i<<" a="<<a<<" b="<<b<<" result="<<cpu.state.rax<<" ref="<<reference<<" flags="<<cpu.state.rflags<<" native flags="<<flags<<'\n'; return 1;
  }
  ++cases;
 }
 // Native word IMUL, including a 16-bit immediate and preservation of upper RAX.
 for(unsigned form=0;form<4;++form) for(unsigned i=0;i<16384;++i) {
  auto a=next(seed), b=next(seed); if(i<edges.size()*edges.size()){a=edges[i/edges.size()];b=edges[i%edges.size()];}
  std::uint16_t native_a=static_cast<std::uint16_t>(a), native_b=static_cast<std::uint16_t>(b), result=native_a;
  std::uint64_t flags=0;
  if(form<2) asm volatile("imulw %[src],%[dst]; pushfq; popq %[flags]" : [dst] "+r"(result), [flags] "=r"(flags) : [src] "m"(native_b) : "cc");
  else if(form==2) asm volatile("imulw $-7,%[src],%[dst]; pushfq; popq %[flags]" : [dst] "=r"(result), [flags] "=r"(flags) : [src] "r"(native_b) : "cc");
  else asm volatile("imulw $0x7123,%[src],%[dst]; pushfq; popq %[flags]" : [dst] "=r"(result), [flags] "=r"(flags) : [src] "r"(native_b) : "cc");
  std::array<std::uint8_t,8> code{}; unsigned n=0; code[n++]=0x66;
  if(form<2) {code[n++]=0x0f; code[n++]=0xaf; code[n++]=form==0?0xc3:0x03;}
  else { code[n++]=form==2?0x6b:0x69; code[n++]=0xc3; if(form==2) code[n++]=0xf9; else {code[n++]=0x23; code[n++]=0x71;} }
  Memory mem(128); Cpu cpu; std::array<std::uint8_t,2> bytes{std::uint8_t(b),std::uint8_t(b>>8)};
  if(!mem.write(0,std::span(code).first(n))||!mem.write(64,bytes))return 1;
  cpu.state.rax=a; *cpu.state.reg(3)=form==1?64:b; cpu.state.rflags=0x2d6;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=n || cpu.state.rax!=((a&~0xffffULL)|result) || (cpu.state.rflags&0x801)!=(flags&0x801) || (cpu.state.rflags&~std::uint64_t{0x801})!=(0x2d6&~0x801)) {
   std::cerr<<"word IMUL mismatch form="<<form<<" i="<<i<<'\n'; return 1;
  }
  ++cases;
 }
 std::cout<<cases<<" native IMUL comparisons passed (defined CF/OF, selected forms)\n";
}
