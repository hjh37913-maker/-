#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
int main() {
 std::uint64_t rng=0x176abcd938762ULL; unsigned count=0;
 for(unsigned width : {1u,2u}) for(unsigned i=0;i<8192;++i) {
  rng^=rng<<13; rng^=rng>>7; rng^=rng<<17;
  std::array<std::uint8_t,2> data{static_cast<std::uint8_t>(rng),static_cast<std::uint8_t>(rng>>8)};
  std::uint32_t reference;
  if(width==1) asm volatile("movzbl %1, %0" : "=r"(reference) : "m"(data[0]));
  else { const std::uint16_t value=std::uint16_t(data[0])|(std::uint16_t(data[1])<<8); asm volatile("movzwl %1, %0" : "=r"(reference) : "m"(value)); }
  std::array<std::uint8_t,3> code{0x0f,static_cast<std::uint8_t>(width==1?0xb6:0xb7),0x03}; Memory m(128); Cpu c;
  if(!m.write(0,code) || !m.write(64,data)) return 1; c.state.rax=~std::uint64_t{0}; *c.state.reg(3)=64; c.state.rflags=0x8d7;
  if(c.step(m)!=StepResult::running || c.state.rip!=3 || c.state.rax!=reference || c.state.rflags!=0x8d7) { std::cerr<<"MOVZX mismatch\n"; return 1; } ++count;
 }
 std::cout<<count<<" MOVZX memory cases compared with native CPU\n";
}
