#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
 for(bool wide:{false,true}) for(unsigned op:{0x0du,0x25u,0x35u}) for(std::uint32_t imm:{0u,1u,0x7fffffffu,0x80000000u,0xffffffffu}) {
  std::array<std::uint8_t,6> code{}; unsigned n=0; if(wide) code[n++]=0x48; code[n++]=op;
  for(unsigned i=0;i<4;++i) code[n++]=imm>>(8*i);
  const std::uint64_t lhs=0xfedcba9876543210ULL, rhs=wide && (imm&0x80000000u)?std::uint64_t(imm)|0xffffffff00000000ULL:imm;
  auto want=op==0x0d?lhs|rhs:op==0x25?lhs&rhs:lhs^rhs; if(!wide) want=std::uint32_t(want);
  Memory m(16); Cpu c; CHECK(m.write(0,std::span(code).first(n))); c.state.rax=lhs; c.state.rflags=0x812;
  CHECK(c.step(m)==StepResult::running && c.state.rax==want && c.state.rip==n && (c.state.rflags&0x10)==0x10 && (c.state.rflags&0x801)==0);
  for(unsigned j=1;j<n;++j) { Memory part(j); Cpu f; CHECK(part.write(0,std::span(code).first(j))); f.state.rax=lhs; f.state.rflags=0x812;
    CHECK(f.step(part)==StepResult::memory_fault && f.state.rip==0 && f.state.rax==lhs && f.state.rflags==0x812); }
 }
}
