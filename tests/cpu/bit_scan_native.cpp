#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <cstdlib>
using namespace nightfall;
int main(int argc,char** argv){std::uint64_t seed=argc>1?std::strtoull(argv[1],nullptr,0):0x281fa733acc51619ULL;if(!seed)seed=1;unsigned count=0;
 for(unsigned width:{16u,32u,64u})for(bool reverse:{false,true})for(unsigned i=0;i<32768;++i){
  seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;
  std::uint64_t source=i%257?seed:0, result=0xdeadbeefaa55cc99ULL; unsigned char z=0;
  if(width==16){auto s=std::uint16_t(source),r=std::uint16_t(result);if(reverse)asm volatile("bsrw %2,%0;setz %1":"+r"(r),"=qm"(z):"rm"(s):"cc");else asm volatile("bsfw %2,%0;setz %1":"+r"(r),"=qm"(z):"rm"(s):"cc");result=(result&~0xffffULL)|r;source=s;}
  else if(width==32){auto s=std::uint32_t(source),r=std::uint32_t(result);if(reverse)asm volatile("bsrl %2,%0;setz %1":"+r"(r),"=qm"(z):"rm"(s):"cc");else asm volatile("bsfl %2,%0;setz %1":"+r"(r),"=qm"(z):"rm"(s):"cc");result=r;source=s;}
  else {if(reverse)asm volatile("bsrq %2,%0;setz %1":"+r"(result),"=qm"(z):"rm"(source):"cc");else asm volatile("bsfq %2,%0;setz %1":"+r"(result),"=qm"(z):"rm"(source):"cc");}
  Memory m(16);Cpu cpu;std::array<std::uint8_t,4> code{width==16?std::uint8_t(0x66):width==64?std::uint8_t(0x48):std::uint8_t(0x40),0x0f,reverse?std::uint8_t(0xbd):std::uint8_t(0xbc),0xc1};
  if(!m.write(0,code))return 1;cpu.state.rax=0xdeadbeefaa55cc99ULL;*cpu.state.reg(1)=source;cpu.state.rflags=0x8d7;
  if(cpu.step(m)!=StepResult::running || bool(cpu.state.rflags&0x40)!=bool(z) || (source && cpu.state.rax!=result)){std::cerr<<"native scan mismatch width="<<width<<" reverse="<<reverse<<" i="<<i<<'\n';return 1;}++count;
 }std::cout<<count<<" native BSF/BSR defined-result and ZF comparisons passed\n";}
