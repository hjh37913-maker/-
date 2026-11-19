#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
#define EXEC(PREFIX,INST) asm volatile("pushq %[initial]; popfq; " PREFIX " " INST "; pushfq; popq %[out]; cld" : "+S"(s),"+D"(d),"+c"(c),"+a"(a),[out] "=&r"(flags) : [initial] "r"(initial) : "cc","memory")
void native(unsigned op,unsigned width,bool equal,std::uint8_t*& s,std::uint8_t*& d,std::uint64_t& c,std::uint64_t& a,std::uint64_t& flags,std::uint64_t initial) {
 if(op==0) {if(equal) {if(width==1)EXEC("repe","cmpsb"); else if(width==2)EXEC("repe","cmpsw"); else if(width==4)EXEC("repe","cmpsl"); else EXEC("repe","cmpsq");}
   else {if(width==1)EXEC("repne","cmpsb"); else if(width==2)EXEC("repne","cmpsw"); else if(width==4)EXEC("repne","cmpsl"); else EXEC("repne","cmpsq");}}
 else {if(equal) {if(width==1)EXEC("repe","scasb"); else if(width==2)EXEC("repe","scasw"); else if(width==4)EXEC("repe","scasl"); else EXEC("repe","scasq");}
   else {if(width==1)EXEC("repne","scasb"); else if(width==2)EXEC("repne","scasw"); else if(width==4)EXEC("repne","scasl"); else EXEC("repne","scasq");}}
}
#undef EXEC
int main() {
 std::uint64_t rng=0x4b9f10ae3292c5d3ULL; unsigned cases=0;
 for(unsigned op=0;op<2;++op)for(unsigned width:{1u,2u,4u,8u})for(bool back:{false,true})for(bool equal:{false,true})for(unsigned i=0;i<1024;++i) {
  std::array<std::uint8_t,512> host{}; for(auto& x:host)x=std::uint8_t(next(rng));
  const unsigned count=next(rng)%17, src=128+(back&&count?(count-1)*width:0), dst=320+(back&&count?(count-1)*width:0);
  std::uint64_t a=next(rng); const auto initial_a=a;
  // Mix equal runs with mismatches at different positions.
  if(i%3==0) for(unsigned k=0;k<count*width;++k) host[320+k]=op==0?host[128+k]:std::uint8_t(a>>((k%width)*8));
  if(i%3==0 && count && i%2==0)host[320+(next(rng)%(count*width))]^=1;
  auto original=host; auto* s=host.data()+src; auto* d=host.data()+dst; std::uint64_t c=count,flags=0,initial=0;
  asm volatile("pushfq; popq %0" : "=r"(initial)); initial=(initial&~std::uint64_t{0xcd5})|0x8d5|(back?0x400:0);
  native(op,width,equal,s,d,c,a,flags,initial);
  std::array<std::uint8_t,4> code{}; unsigned n=0; code[n++]=equal?0xf3:0xf2; if(width==2)code[n++]=0x66;else if(width==8)code[n++]=0x48;
  code[n++]=std::uint8_t(op==0?(width==1?0xa6:0xa7):(width==1?0xae:0xaf));
  Memory mem(2048); Cpu cpu; if(!mem.write(0,std::span(code).first(n))||!mem.write(512,original))return 1;
  *cpu.state.reg(6)=512+src; *cpu.state.reg(7)=512+dst; *cpu.state.reg(1)=count; cpu.state.rax=initial_a; cpu.state.rflags=0x8d7|(back?0x400:0);
  unsigned steps=0; do { if(cpu.step(mem)!=StepResult::running || ++steps>count+1)return 1; } while(cpu.state.rip==0);
  if(cpu.state.rip!=n || *cpu.state.reg(1)!=c || cpu.state.rax!=a || *cpu.state.reg(6)!=std::uint64_t(512+(s-host.data())) || *cpu.state.reg(7)!=std::uint64_t(512+(d-host.data())) || (cpu.state.rflags&0xcd5)!=(flags&0xcd5)) {
   std::cerr<<"REP compare mismatch op="<<op<<" width="<<width<<" back="<<back<<" equal="<<equal<<" i="<<i<<" count="<<count<<" gflags="<<cpu.state.rflags<<" nflags="<<flags<<'\n'; return 1;
  }
  ++cases;
 }
 std::cout<<cases<<" native REPE/REPNE CMPS/SCAS comparisons, including stop conditions and flags\n";
}
