#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
struct Result { std::uint64_t value,flags; };
#define RUN(INS) asm volatile("movq %[a],%%r8; movq %[b],%%r9; " INS " %%r9d,%%r8d; pushfq; popq %[f]; movq %%r8,%[v]" : [v] "=&r"(out),[f] "=&r"(flags) : [a] "r"(a),[b] "r"(b) : "r8","r9","cc","memory")
Result native(unsigned op,std::uint64_t a,std::uint64_t b) {
 std::uint64_t out=0,flags=0;
 switch(op) {case 0:RUN("addl");break;case 1:RUN("subl");break;case 2:RUN("xorl");break;case 3:RUN("orl");break;case 4:RUN("andl");break;case 5:RUN("cmpl");break;case 6:RUN("testl");break;}
 return {out,flags};
}
#undef RUN
int main() {
 std::uint64_t seed=0xc811ba1c693dac34ULL; unsigned count=0;
 constexpr std::array<std::uint8_t,7> op{0x01,0x29,0x31,0x09,0x21,0x39,0x85};
 constexpr std::array<std::uint8_t,7> rev{0x03,0x2b,0x33,0x0b,0x23,0x3b,0x85};
 for(unsigned kind=0;kind<op.size();++kind)for(bool direction:{false,true})for(unsigned i=0;i<8192;++i) {
  auto a=next(seed),b=next(seed); if(i<8) {constexpr std::array<std::uint64_t,8> edge{0,1,0xffffffff,0x7fffffff,0x80000000,0x10,0xff,0x100000000};a=edge[i];b=edge[7-i];}
  auto ref=native(kind,a,b); Cpu cpu; Memory mem(32); std::array<std::uint8_t,3> code{0x45,direction?rev[kind]:op[kind],std::uint8_t(direction&&kind!=6?0xc1:0xc8)};
  if(!mem.write(0,code))return 1; *cpu.state.reg(8)=a; *cpu.state.reg(9)=b; cpu.state.rflags=0x212;
  const auto mask=kind>=2 && kind<=4 || kind==6?0x8c5ULL:0x8d5ULL;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=3 || *cpu.state.reg(8)!=ref.value || *cpu.state.reg(9)!=b || (cpu.state.rflags&mask)!=(ref.flags&mask) || (cpu.state.rflags&~0x8d5ULL)!=(0x212&~0x8d5ULL)) {
   std::cerr<<"REX32 native mismatch kind="<<kind<<" direction="<<direction<<" i="<<i<<'\n'; return 1;
  }
  ++count;
 }
 std::cout<<count<<" native REX32 r8d/r9d arithmetic comparisons passed\n";
}
