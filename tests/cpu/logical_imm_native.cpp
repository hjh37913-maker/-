#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
struct Result { std::uint64_t value,flags; };
Result native(std::uint64_t lhs,std::uint64_t rhs,bool wide,unsigned ext) {
 std::uint64_t flags=0;
#define DO(ins, value, operand) asm volatile(ins "\n\tpushfq\n\tpopq %[f]" : [v] "+&r"(value), [f] "=&r"(flags) : [r] "r"(operand) : "cc","memory")
 if (wide) { if(ext==1) DO("orq %[r],%[v]",lhs,rhs); else if(ext==4) DO("andq %[r],%[v]",lhs,rhs); else DO("xorq %[r],%[v]",lhs,rhs); }
 else { auto l=static_cast<std::uint32_t>(lhs),r=static_cast<std::uint32_t>(rhs); if(ext==1) DO("orl %[r],%[v]",l,r); else if(ext==4) DO("andl %[r],%[v]",l,r); else DO("xorl %[r],%[v]",l,r); lhs=l; }
#undef DO
 return {lhs,flags};
}
int main() {
 std::uint64_t rng=0x1239875432198765ULL; unsigned count=0;
 for(bool wide:{false,true}) for(bool mem:{false,true}) for(unsigned op:{0x81u,0x83u}) for(unsigned ext:{1u,4u,6u}) for(unsigned i=0;i<2048;++i) {
  const auto lhs=next(rng); const auto imm=op==0x83 ? i%256 : static_cast<unsigned>(next(rng));
  std::uint64_t rhs=imm; if(op==0x83 && (imm&0x80)) rhs|=0xffffffffffffff00ULL; if(op==0x81 && (imm&0x80000000u)) rhs|=0xffffffff00000000ULL;
  const auto ref=native(lhs,rhs,wide,ext); const unsigned dest=i%(wide?16:8); Cpu c; Memory m(128); std::array<std::uint8_t,11> code{}; unsigned pos=0;
  if(wide) code[pos++]=0x48|(!mem && dest>=8?1:0); code[pos++]=op;
  if(mem) { unsigned length=(wide?1:0)+2+4+(op==0x81?4:1); code[pos++]=(ext<<3)|5; code[pos++]=64-length; code[pos++]=0;code[pos++]=0;code[pos++]=0; }
  else code[pos++]=0xc0|(ext<<3)|(dest&7);
  const unsigned n=op==0x81?4:1; for(unsigned j=0;j<n;++j) code[pos++]=imm>>(8*j);
  std::array<std::uint8_t,8> data{}; for(unsigned j=0;j<8;++j) data[j]=lhs>>(8*j);
  if(!m.write(0,std::span(code).first(pos)) || !m.write(64,data)) return 1;
  *c.state.reg(dest)=lhs; c.state.rflags=0x412;
  if(c.step(m)!=StepResult::running || c.state.rip!=pos || (c.state.rflags&0x8c5)!=(ref.flags&0x8c5) || (c.state.rflags&~0x8c5)!=(0x412&~0x8c5)) { std::cerr<<"decode or flags mismatch\n"; return 1; }
  if(mem) { if(!m.read(64,data) || *c.state.reg(dest)!=lhs) return 1; std::uint64_t got=0; for(unsigned j=0;j<8;++j) got|=std::uint64_t(data[j])<<(8*j); const auto want=wide?ref.value:(lhs&0xffffffff00000000ULL)|ref.value; if(got!=want) { std::cerr<<"memory mismatch\n"; return 1; } }
  else if(*c.state.reg(dest)!=ref.value) { std::cerr<<"register mismatch\n"; return 1; }
  ++count;
 }
 std::cout<<count<<" group logical immediate comparisons against native register operations passed\n";
}
