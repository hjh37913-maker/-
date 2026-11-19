// Native Linux x86-64 REP string oracle on private synthetic host arrays.
#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
#define EXEC(INST) do { \
 if(back) asm volatile("std; rep " INST "; pushfq; popq %[f]; cld" : "+S"(s),"+D"(d),"+c"(c),"+a"(a),[f] "=&r"(flags) : : "cc","memory"); \
 else asm volatile("cld; rep " INST "; pushfq; popq %[f]" : "+S"(s),"+D"(d),"+c"(c),"+a"(a),[f] "=&r"(flags) : : "cc","memory"); \
 } while(0)
void native(unsigned op,unsigned width,bool back,std::uint8_t*& s,std::uint8_t*& d,std::uint64_t& c,std::uint64_t& a,std::uint64_t& flags) {
 if(op==0) { if(width==1)EXEC("movsb"); else if(width==2)EXEC("movsw"); else if(width==4)EXEC("movsl"); else EXEC("movsq"); }
 else if(op==1) { if(width==1)EXEC("stosb"); else if(width==2)EXEC("stosw"); else if(width==4)EXEC("stosl"); else EXEC("stosq"); }
 else { if(width==1)EXEC("lodsb"); else if(width==2)EXEC("lodsw"); else if(width==4)EXEC("lodsl"); else EXEC("lodsq"); }
}
#undef EXEC
int main() {
 std::uint64_t rng=0x1239aebcd5671889ULL; unsigned cases=0;
 for(unsigned op=0;op<3;++op)for(unsigned width:{1u,2u,4u,8u})for(bool back:{false,true})for(unsigned i=0;i<1024;++i) {
  std::array<std::uint8_t,512> host{}; for(auto& x:host)x=std::uint8_t(next(rng));
  const auto original=host; const unsigned count=next(rng)%17; const int delta=int(next(rng)%17)-8;
  const unsigned src=192+(back && count?(count-1)*width:0), dst=192+delta+(back&&count?(count-1)*width:0);
  auto* s=host.data()+src; auto* d=host.data()+dst; std::uint64_t c=count, a=next(rng), flags=0; const auto initial_a=a;
  native(op,width,back,s,d,c,a,flags);
  std::array<std::uint8_t,4> code{}; unsigned n=0; code[n++]=0xf3; if(width==2)code[n++]=0x66;else if(width==8)code[n++]=0x48;
  code[n++]=std::uint8_t(op==0?(width==1?0xa4:0xa5):op==1?(width==1?0xaa:0xab):(width==1?0xac:0xad));
  Memory mem(2048); Cpu cpu; if(!mem.write(0,std::span(code).first(n))||!mem.write(512,original))return 1;
  *cpu.state.reg(6)=512+src; *cpu.state.reg(7)=512+dst; *cpu.state.reg(1)=count; cpu.state.rax=initial_a; cpu.state.rflags=0x8d7|(back?0x400:0);
  const auto max_steps=count?count:1; for(unsigned step=0;step<max_steps;++step) if(cpu.step(mem)!=StepResult::running)return 1;
  std::array<std::uint8_t,512> guest{}; if(!mem.read(512,guest))return 1;
  if(guest!=host || cpu.state.rip!=n || *cpu.state.reg(1)!=c || cpu.state.rax!=a || *cpu.state.reg(6)!=std::uint64_t(512+(s-host.data())) || *cpu.state.reg(7)!=std::uint64_t(512+(d-host.data())) || bool(cpu.state.rflags&0x400)!=bool(flags&0x400) || cpu.state.rflags!=(0x8d7|(back?0x400:0))) {
   std::cerr<<"REP string mismatch op="<<op<<" width="<<width<<" back="<<back<<" i="<<i<<" count="<<count<<"\n"; return 1;
  }
  ++cases;
 }
 std::cout<<cases<<" native REP MOVS/STOS/LODS comparisons including overlap and direction\n";
}
