#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
int main() {
 std::uint64_t seed=0x123a8c9f12381122ULL; unsigned count=0;
 for(bool wide:{false,true}) for(unsigned op:{0x0du,0x25u,0x35u}) for(unsigned i=0;i<8192;++i) {
  seed^=seed<<13; seed^=seed>>7; seed^=seed<<17; auto lhs=seed; const auto initial=lhs;
  seed^=seed<<13; seed^=seed>>7; seed^=seed<<17; auto imm=std::uint32_t(seed);
  const auto rhs=wide && (imm&0x80000000u)?std::uint64_t(imm)|0xffffffff00000000ULL:imm; std::uint64_t flags;
#define EXEC(instruction, destination, source) asm volatile(instruction "\n\tpushfq\n\tpopq %[f]" : [v] "+&r"(destination),[f] "=&r"(flags) : [r] "r"(source) : "cc","memory")
  if(wide) { if(op==0x0d) EXEC("orq %[r],%[v]",lhs,rhs); else if(op==0x25) EXEC("andq %[r],%[v]",lhs,rhs); else EXEC("xorq %[r],%[v]",lhs,rhs); }
  else { auto d=std::uint32_t(lhs), r=std::uint32_t(rhs); if(op==0x0d) EXEC("orl %[r],%[v]",d,r); else if(op==0x25) EXEC("andl %[r],%[v]",d,r); else EXEC("xorl %[r],%[v]",d,r); lhs=d; }
#undef EXEC
  std::array<std::uint8_t,6> code{}; unsigned n=0; if(wide) code[n++]=0x48; code[n++]=op; for(unsigned j=0;j<4;++j) code[n++]=imm>>(8*j);
  Memory m(16); Cpu c; if(!m.write(0,std::span(code).first(n))) return 1;
  c.state.rax=initial; c.state.rflags=0x412;
  if(c.step(m)!=StepResult::running || c.state.rip!=n || c.state.rax!=lhs || (c.state.rflags&0x8c5)!=(flags&0x8c5) || (c.state.rflags&~0x8c5)!=(0x412&~0x8c5)) { std::cerr<<"accumulator immediate mismatch\n"; return 1; }
  ++count;
 }
 std::cout<<count<<" accumulator immediate comparisons against equivalent native register operations passed\n";
}
