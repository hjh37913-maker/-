#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 std::uint64_t rng=0x124e89ab99882345ULL; unsigned cases=0;
 for(unsigned op:{0x89u,0x8bu}) for(unsigned i=0;i<16384;++i) {
  auto reg=next(rng); const auto input=reg; auto val=std::uint16_t(next(rng)); const auto initial=val;
  if(op==0x89) asm volatile("movw %w[r],%[m]" : [m] "+m"(val) : [r] "r"(reg) : "memory");
  else asm volatile("movw %[m],%w[r]" : [r] "+r"(reg) : [m] "m"(val));
  const std::array<std::uint8_t,3> code{0x66,std::uint8_t(op),0x03}; const std::array<std::uint8_t,2> bytes{std::uint8_t(initial),std::uint8_t(initial>>8)};
  Memory m(128); Cpu c; if(!m.write(0,code)||!m.write(64,bytes)) return 1; c.state.rax=input; *c.state.reg(3)=64; c.state.rflags=0x8d7;
  std::array<std::uint8_t,2> got{};
  if(c.step(m)!=StepResult::running || c.state.rip!=3 || c.state.rax!=reg || c.state.rflags!=0x8d7 || !m.read(64,got) || (std::uint16_t(got[0])|(std::uint16_t(got[1])<<8))!=val) { std::cerr<<"MOV16 native mismatch\n"; return 1; }
  ++cases;
 }
 std::cout<<cases<<" MOV16 memory transfers compared with native CPU\n";
}
