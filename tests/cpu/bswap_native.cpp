#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
int main() {
 std::uint64_t seed=0x6ef3e95a1bac3281ULL; unsigned cases=0;
 for(bool wide:{false,true}) for(unsigned i=0;i<65536;++i) {
  seed^=seed<<13; seed^=seed>>7; seed^=seed<<17; auto reference=seed;
  if(wide)asm volatile("bswapq %0" : "+r"(reference));
  else { auto word=static_cast<std::uint32_t>(reference); asm volatile("bswapl %0" : "+r"(word)); reference=word; }
  Memory mem(16); Cpu cpu; std::array<std::uint8_t,3> code{0x48,0x0f,0xc8}; const auto n=wide?3u:2u;
  if(!mem.write(0,wide?std::span(code):std::span(code).subspan(1)))return 1;
  cpu.state.rax=seed; cpu.state.rflags=0x8d7;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=n || cpu.state.rax!=reference || cpu.state.rflags!=0x8d7) { std::cerr<<"BSWAP mismatch wide="<<wide<<" i="<<i<<'\n'; return 1; }
  ++cases;
 }
 std::cout<<cases<<" native BSWAP comparisons passed\n";
}
