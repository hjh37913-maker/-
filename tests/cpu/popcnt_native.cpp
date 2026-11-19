#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#if defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif
using namespace nightfall;
int main(){unsigned a,b,c,d;if(!__get_cpuid(1,&a,&b,&c,&d)||!(c&(1u<<23))){std::cout<<"host POPCNT absent; native oracle skipped\n";return 0;}
 std::uint64_t seed=0x786d32bcd81f4581ULL;unsigned count=0;
 for(unsigned width:{16u,32u,64u})for(unsigned i=0;i<32768;++i){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;std::uint64_t source=i%257?seed:0,result=0xfeedcafe12345678ULL,flags=0;
 if(width==16){auto s=std::uint16_t(source),r=std::uint16_t(result);asm volatile("popcntw %2,%0; pushfq; popq %1":"=r"(r),"=r"(flags):"rm"(s):"cc");result=(result&~0xffffULL)|r;source=s;}
 else if(width==32){auto s=std::uint32_t(source),r=std::uint32_t{};asm volatile("popcntl %2,%0; pushfq; popq %1":"=r"(r),"=r"(flags):"rm"(s):"cc");result=r;source=s;}
 else asm volatile("popcntq %2,%0; pushfq; popq %1":"=r"(result),"=r"(flags):"rm"(source):"cc");
 Memory m(16);Cpu cpu;std::array<std::uint8_t,5> code{0xf3,width==16?std::uint8_t(0x66):width==64?std::uint8_t(0x48):std::uint8_t(0x40),0x0f,0xb8,0xc1};if(!m.write(0,code))return 1;cpu.state.rax=0xfeedcafe12345678ULL;*cpu.state.reg(1)=source;cpu.state.rflags=0x8d7;
 if(cpu.step(m)!=StepResult::running || cpu.state.rax!=result || (cpu.state.rflags&0x8d5)!=(flags&0x8d5)){std::cerr<<"POPCNT native mismatch width="<<width<<" i="<<i<<'\n';return 1;}++count;}
 std::cout<<count<<" native POPCNT result and status flag comparisons passed\n";}
