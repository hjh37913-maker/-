#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
struct Ref { std::uint64_t result,flags; };
#define NATIVE(T,SUFFIX) do { T l=static_cast<T>(lhs),r=static_cast<T>(rhs); \
 if(sub) asm volatile("btq $0,%[c]; sbb" SUFFIX " %[r],%[l]; pushfq; popq %[f]" : [l] "+&r"(l), [f] "=&r"(flags) : [r] "r"(r),[c] "r"(carry) : "cc","memory"); \
 else asm volatile("btq $0,%[c]; adc" SUFFIX " %[r],%[l]; pushfq; popq %[f]" : [l] "+&r"(l), [f] "=&r"(flags) : [r] "r"(r),[c] "r"(carry) : "cc","memory"); \
 return {l,flags}; } while(0)
Ref native(unsigned width,bool sub,std::uint64_t lhs,std::uint64_t rhs,std::uint64_t carry) {
 std::uint64_t flags=0; if(width==8) NATIVE(std::uint8_t,"b"); if(width==16) NATIVE(std::uint16_t,"w"); if(width==32) NATIVE(std::uint32_t,"l"); NATIVE(std::uint64_t,"q");
}
#undef NATIVE
int main() {
 std::uint64_t rng=0x184acf93a19f3d5bULL; unsigned count=0;
 constexpr std::array<std::uint64_t,12> edge{0,1,2,0xf,0x7f,0x80,0xff,0xffff,0x7fffffff,0x80000000,0x7fffffffffffffff,0xffffffffffffffff};
 for(unsigned width:{8u,16u,32u,64u}) for(bool sub:{false,true}) for(bool carry:{false,true}) for(unsigned form=0;form<6;++form) for(unsigned i=0;i<4096;++i) {
  auto lhs=next(rng),rhs=next(rng); if(i<edge.size()*edge.size()){lhs=edge[i/edge.size()];rhs=edge[i%edge.size()];}
  std::array<std::uint8_t,12> code{}; unsigned n=0;
  if(width==16)code[n++]=0x66; else if(width==64)code[n++]=0x48;
  const auto base=std::uint8_t(sub?0x18:0x10);
  unsigned imm=0;
  if(form<3) { code[n++]=std::uint8_t(base+(width==8?0:1)+(form==2?2:0)); code[n++]=form==0?0xd8:0x03; }
  else if(form==5) { code[n++]=std::uint8_t(base+(width==8?4:5)); imm=width==8?1:width==16?2:4; }
  else { code[n++]=width==8?0x80:form==3?0x83:0x81; code[n++]=std::uint8_t(0xc0|((sub?3:2)<<3)); imm=width==8||form==3?1:width==16?2:4; }
  if(imm) { for(unsigned j=0;j<imm;++j)code[n++]=rhs>>(8*j);
    if(imm==1 && width>8 && form==3) rhs=(rhs&0x80)?(rhs&0xff)|~0xffULL:rhs&0xff;
    else if(imm==4 && width==64) rhs=(rhs&0x80000000ULL)?(rhs&0xffffffffULL)|0xffffffff00000000ULL:rhs&0xffffffffULL;
    else rhs&=imm==1?0xffULL:imm==2?0xffffULL:0xffffffffULL;
  }
  const auto reference=native(width,sub,lhs,rhs,carry);
  Memory mem(128); Cpu cpu; std::array<std::uint8_t,8> bytes{}; const auto data=form==1?lhs:rhs;
  for(unsigned j=0;j<8;++j)bytes[j]=data>>(j*8);
  if(!mem.write(0,std::span(code).first(n))||!mem.write(64,bytes))return 1;
  cpu.state.rax=form==1?rhs:lhs; *cpu.state.reg(3)=form==1||form==2?64:rhs; cpu.state.rflags=0x202|carry;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=n || (cpu.state.rflags&0x8d5)!=(reference.flags&0x8d5) || (cpu.state.rflags&~0x8d5)!=(0x202&~0x8d5)) {
   std::cerr<<"ADC/SBB flags/decode mismatch width="<<width<<" sub="<<sub<<" carry="<<carry<<" form="<<form<<" i="<<i<<" guestflags="<<cpu.state.rflags<<" nativeflags="<<reference.flags<<'\n'; return 1;
  }
  if(form==1) {
   std::array<std::uint8_t,8> got{}; if(!mem.read(64,std::span(got).first(width/8)))return 1;
   for(unsigned j=0;j<width/8;++j)if(got[j]!=std::uint8_t(reference.result>>(8*j))){std::cerr<<"memory result mismatch\n";return 1;}
   if(cpu.state.rax!=rhs)return 1;
  } else {
   const auto mask=width==64?~std::uint64_t{0}:(std::uint64_t{1}<<width)-1;
   const auto expected=width==32||width==64?reference.result:(lhs&~mask)|reference.result;
   if(cpu.state.rax!=expected) {std::cerr<<"register result mismatch width="<<width<<" form="<<form<<" i="<<i<<'\n';return 1;}
  }
  ++count;
 }
 std::cout<<count<<" native ADC/SBB comparisons passed (register arithmetic oracle; flags and selected guest forms)\n";
}
