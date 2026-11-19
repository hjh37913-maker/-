#include "cpu/cpu.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
int main() {
 constexpr std::size_t count=500000;
 constexpr std::array<std::uint8_t,3> op{0x48,0xf7,0xf3};
 nightfall::Memory memory(op.size()); if(!memory.write(0,op))return 1;
 for(bool high:{false,true}) {
  nightfall::Cpu cpu; *cpu.state.reg(3)=3; std::uint64_t checksum=0;
  const auto start=std::chrono::steady_clock::now();
  for(std::size_t i=0;i<count;++i) {
   cpu.state.rip=0; cpu.state.rax=i; *cpu.state.reg(2)=high?1:0;
   if(cpu.step(memory)!=nightfall::StepResult::running)return 1;
   checksum+=cpu.state.rax+*cpu.state.reg(2);
  }
  const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
  std::cout<<"Synthetic DIV RDX:RAX / 3 high="<<high<<" count="<<count<<" ns="<<elapsed<<" checksum="<<checksum<<'\n';
 }
}
