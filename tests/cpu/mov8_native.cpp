#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
int main() {
 std::uint64_t seed=0x1238ab98edaf2981ULL; unsigned count=0;
 for(unsigned op:{0x88u,0x8au}) for(bool high:{false,true}) for(unsigned i=0;i<8192;++i) {
  seed^=seed<<13; seed^=seed>>7; seed^=seed<<17; std::uint64_t ax=seed; const auto initial_ax=ax;
  seed^=seed<<13; seed^=seed>>7; seed^=seed<<17; std::uint8_t byte=seed; const auto initial_byte=byte;
  if(op==0x88) { if(high) asm volatile("movb %%ah,%[mem]" : [mem] "+m"(byte) : "a"(ax) : "memory"); else asm volatile("movb %%al,%[mem]" : [mem] "+m"(byte) : "a"(ax) : "memory"); }
  else { if(high) asm volatile("movb %[mem],%%ah" : "+a"(ax) : [mem] "m"(byte) : "memory"); else asm volatile("movb %[mem],%%al" : "+a"(ax) : [mem] "m"(byte) : "memory"); }
  std::array<std::uint8_t,2> code{std::uint8_t(op),std::uint8_t(high?0x23:0x03)}; const std::array<std::uint8_t,1> data{initial_byte}; Memory mem(128); Cpu cpu;
  if(!mem.write(0,code) || !mem.write(64,data)) return 1;
  cpu.state.rax=initial_ax; *cpu.state.reg(3)=64; cpu.state.rflags=0x8d7; std::array<std::uint8_t,1> actual{};
  if(cpu.step(mem)!=StepResult::running || !mem.read(64,actual) || cpu.state.rax!=ax || actual[0]!=byte || cpu.state.rip!=2 || cpu.state.rflags!=0x8d7) { std::cerr<<"byte MOV mismatch op="<<op<<" high="<<high<<'\n'; return 1; }
  ++count;
 }
 std::cout<<count<<" byte MOV memory cases compared with native CPU, including AH\n";
}
