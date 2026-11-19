// Each deliberate fault occurs in an isolated child with core dumps disabled.
#include "cpu/cpu.h"
#include <array>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace nightfall;
int main() {
 unsigned cases=0;
 for(unsigned width:{8u,16u,32u,64u}) for(bool sign:{false,true}) for(bool zero:{false,true}) {
  const auto mask=width==64?~std::uint64_t{0}:(std::uint64_t{1}<<width)-1;
  const auto lo=sign?(std::uint64_t{1}<<(width-1)):0, hi=sign?mask:1, divisor=zero?0:sign?mask:1;
  const pid_t pid=fork(); if(pid<0)return 1;
  if(pid==0) {
   const rlimit no_core{0,0}; setrlimit(RLIMIT_CORE,&no_core); std::signal(SIGFPE,SIG_DFL);
   if(width==8) { auto ax=std::uint16_t((hi<<8)|lo); auto d=std::uint8_t(divisor); if(sign)asm volatile("idivb %[d]" : "+a"(ax) : [d] "m"(d) : "cc"); else asm volatile("divb %[d]" : "+a"(ax) : [d] "m"(d) : "cc"); }
   else if(width==16) { auto ax=std::uint16_t(lo), dx=std::uint16_t(hi), d=std::uint16_t(divisor); if(sign)asm volatile("idivw %[d]" : "+a"(ax), "+d"(dx) : [d] "m"(d) : "cc"); else asm volatile("divw %[d]" : "+a"(ax), "+d"(dx) : [d] "m"(d) : "cc"); }
   else if(width==32) { auto ax=std::uint32_t(lo), dx=std::uint32_t(hi), d=std::uint32_t(divisor); if(sign)asm volatile("idivl %[d]" : "+a"(ax), "+d"(dx) : [d] "m"(d) : "cc"); else asm volatile("divl %[d]" : "+a"(ax), "+d"(dx) : [d] "m"(d) : "cc"); }
   else { auto ax=lo,dx=hi; if(sign)asm volatile("idivq %[d]" : "+a"(ax), "+d"(dx) : [d] "m"(divisor) : "cc"); else asm volatile("divq %[d]" : "+a"(ax), "+d"(dx) : [d] "m"(divisor) : "cc"); }
   _exit(0);
  }
  int status=0; if(waitpid(pid,&status,0)!=pid || !WIFSIGNALED(status) || WTERMSIG(status)!=SIGFPE) { std::cerr<<"native fault missing width="<<width<<" signed="<<sign<<" zero="<<zero<<'\n'; return 1; }
  Memory mem(32); Cpu cpu; std::array<std::uint8_t,4> op{}; unsigned n=0; if(width==16)op[n++]=0x66; else if(width==64)op[n++]=0x48;
  op[n++]=width==8?0xf6:0xf7; op[n++]=std::uint8_t(0xc0|(sign?0x38:0x30)|3); if(!mem.write(0,std::span(op).first(n)))return 1;
  cpu.state.rax=width==8?(hi<<8)|lo:lo; *cpu.state.reg(2)=hi; *cpu.state.reg(3)=divisor; auto before=cpu.state;
  if(cpu.step(mem)!=StepResult::divide_error || cpu.state.rax!=before.rax || cpu.state.other_registers!=before.other_registers || cpu.state.rip || cpu.state.rflags!=before.rflags) { std::cerr<<"guest fault missing width="<<width<<" signed="<<sign<<" zero="<<zero<<'\n'; return 1; }
  ++cases;
 }
 std::cout<<cases<<" isolated native divide-zero/overflow SIGFPE cases matched guest divide_error\n";
}
