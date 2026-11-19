#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 std::uint64_t rng=0x63766443219876dfULL; unsigned cases=0;
 for(bool wide:{false,true}) for(bool memory:{false,true}) for(unsigned ext:{0u,1u}) for(bool carry:{false,true}) for(unsigned i=0;i<4096;++i) {
  auto input=next(rng); if(i<8) { constexpr std::array<std::uint64_t,8> edge{0,1,0x7fffffff,0x80000000,0xffffffff,0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffffffffffffULL}; input=edge[i]; }
  auto result=input; std::uint64_t flags=0;
#define DO(flagop,ins) asm volatile(flagop "\n\t" ins "\n\tpushfq\n\tpopq %[f]" : [v] "+&r"(result), [f] "=&r"(flags) : : "cc","memory")
  if(wide) { if(carry) { if(ext) DO("stc","decq %[v]"); else DO("stc","incq %[v]"); } else { if(ext) DO("clc","decq %[v]"); else DO("clc","incq %[v]"); } }
  else { if(carry) { if(ext) DO("stc","decl %k[v]"); else DO("stc","incl %k[v]"); } else { if(ext) DO("clc","decl %k[v]"); else DO("clc","incl %k[v]"); } }
#undef DO
  std::array<std::uint8_t,7> code{}; unsigned n=0; if(wide) code[n++]=0x48; code[n++]=0xff;
  if(memory) { unsigned len=(wide?1:0)+6; code[n++]=0x05|(ext<<3); code[n++]=64-len; code[n++]=0;code[n++]=0;code[n++]=0; }
  else code[n++]=0xc0|(ext<<3);
  std::array<std::uint8_t,8> data{}; for(unsigned j=0;j<8;++j) data[j]=input>>(8*j); Memory m(128); Cpu c;
  if(!m.write(0,std::span(code).first(n)) || !m.write(64,data)) return 1; c.state.rax=input; c.state.rflags=0x412|carry;
  constexpr std::uint64_t mask=0x8d5;
  if(c.step(m)!=StepResult::running || c.state.rip!=n || (c.state.rflags&mask)!=(flags&mask) || (c.state.rflags&~mask)!=(0x412&~mask)) { std::cerr<<"INC/DEC flags mismatch\n"; return 1; }
  if(memory) { if(!m.read(64,data) || c.state.rax!=input) return 1; std::uint64_t actual=0; for(unsigned j=0;j<8;++j) actual|=std::uint64_t(data[j])<<(8*j); const auto wanted=wide?result:(input&0xffffffff00000000ULL)|result;
    if(actual!=wanted) { std::cerr<<"INC/DEC memory mismatch\n"; return 1; } }
  else if(c.state.rax!=result) { std::cerr<<"INC/DEC register mismatch\n"; return 1; }
  ++cases;
 }
 std::cout<<cases<<" INC/DEC comparisons against native register instructions passed\n";
}
