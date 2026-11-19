// Deliberate alignment faults are isolated in children; no core files.
#include "core/machine.h"
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
 for(bool integer:{false,true})for(bool store:{false,true}) {
  auto pid=fork();if(pid<0)return 1;
  if(pid==0) {
   const rlimit no_core{0,0}; setrlimit(RLIMIT_CORE,&no_core);std::signal(SIGSEGV,SIG_DFL);
   alignas(16) std::array<std::uint8_t,64> memory{}; auto* p=memory.data()+1;
   if(integer) { if(store)asm volatile("pxor %%xmm0,%%xmm0; movdqa %%xmm0,(%0)" : : "r"(p) : "xmm0","memory");
                 else asm volatile("movdqa (%0),%%xmm0" : : "r"(p) : "xmm0","memory"); }
   else { if(store)asm volatile("pxor %%xmm0,%%xmm0; movaps %%xmm0,(%0)" : : "r"(p) : "xmm0","memory");
          else asm volatile("movaps (%0),%%xmm0" : : "r"(p) : "xmm0","memory"); }
   _exit(0);
  }
  int status=0;if(waitpid(pid,&status,0)!=pid||!WIFSIGNALED(status)||WTERMSIG(status)!=SIGSEGV) {std::cerr<<"native alignment signal missing\n";return 1;}
  Machine m(128);std::array<std::uint8_t,4> code{};unsigned n=0;if(integer)code[n++]=0x66;code[n++]=0x0f;code[n++]=integer?(store?0x7f:0x6f):(store?0x29:0x28);code[n++]=0x03;
  if(!m.memory.write(0,std::span(code).first(n)))return 1;*m.cpu.state.reg(3)=65; m.cpu.state.xmm[0].fill(0xab);const auto before=m.cpu.state;
  if(m.cpu.step(m.memory)!=StepResult::alignment_fault||m.cpu.state.xmm!=before.xmm||m.cpu.state.other_registers!=before.other_registers||m.cpu.state.rip!=0||m.cpu.state.rflags!=before.rflags)return 1;
  ++cases;
 }
 std::cout<<cases<<" isolated native MOVDQA/MOVAPS alignment faults matched model\n";
}
