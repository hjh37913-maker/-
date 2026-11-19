#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
  std::uint64_t seed=0x123456789abcdefULL; unsigned count=0;
  for (bool wide : {false,true}) for (unsigned op : {0x09,0x0b,0x21,0x23})
    for (unsigned reg=0;reg<(wide?16u:8u);++reg) for (unsigned rm=0;rm<(wide?16u:8u);++rm) for (int i=0;i<16;++i) {
      std::array<std::uint64_t,16> before{}; Cpu c; for(unsigned j=0;j<16;++j) *c.state.reg(j)=before[j]=next(seed);
      bool toreg=(op==0x0b || op==0x23); unsigned dst=toreg?reg:rm, src=toreg?rm:reg;
      std::uint64_t result=before[dst], rhs=before[src], flags=0;
      if(wide) {
        if(op==0x09 || op==0x0b) asm volatile("orq %[r],%[v]; pushfq; popq %[f]" : [v] "+&r"(result),[f] "=&r"(flags) : [r] "r"(rhs) : "cc","memory");
        else asm volatile("andq %[r],%[v]; pushfq; popq %[f]" : [v] "+&r"(result),[f] "=&r"(flags) : [r] "r"(rhs) : "cc","memory");
      } else {
        auto value=static_cast<std::uint32_t>(result), operand=static_cast<std::uint32_t>(rhs);
        if(op==0x09 || op==0x0b) asm volatile("orl %[r],%[v]; pushfq; popq %[f]" : [v] "+&r"(value),[f] "=&r"(flags) : [r] "r"(operand) : "cc","memory");
        else asm volatile("andl %[r],%[v]; pushfq; popq %[f]" : [v] "+&r"(value),[f] "=&r"(flags) : [r] "r"(operand) : "cc","memory");
        result=value;
      }
      std::array<std::uint8_t,3> code{}; unsigned len=0;
      if(wide) code[len++]=0x48|(reg>=8?4:0)|(rm>=8?1:0);
      code[len++]=op; code[len++]=0xc0|((reg&7)<<3)|(rm&7);
      Memory m(8); if(!m.write(0,std::span(code).first(len))) return 1; c.state.rflags=0x412;
      if(c.step(m)!=StepResult::running || c.state.rip!=len || *c.state.reg(dst)!=result || (c.state.rflags&0x8c5)!=(flags&0x8c5) || (c.state.rflags&~0x8c5)!=(0x412&~0x8c5)) { std::cerr<<"register mismatch "<<op<<"\n"; return 1; }
      for(unsigned j=0;j<16;++j) if(j!=dst && *c.state.reg(j)!=before[j]) return 1;
      ++count;
    }
  // Memory operand with both destinations, widths, including write fault atomicity.
  for(bool wide : {false,true}) for(unsigned op : {0x09,0x0b,0x21,0x23}) for(unsigned i=0;i<2048;++i) {
    auto mem=next(seed), reg=next(seed); const auto original_mem=mem, original_reg=reg;
    bool toreg=op==0x0b || op==0x23, isor=op==0x09 || op==0x0b; std::uint64_t flags;
#define DO64(ins) asm volatile(ins "\n\tpushfq\n\tpopq %[f]" : [m] "+m"(mem),[r] "+&r"(reg),[f] "=&r"(flags) : : "cc","memory")
#define DO32(ins) asm volatile(ins "\n\tpushfq\n\tpopq %[f]" : [m] "+m"(mem32),[r] "+&r"(reg32),[f] "=&r"(flags) : : "cc","memory")
    if(wide) { if(isor) { if(toreg) DO64("orq %[m],%[r]"); else DO64("orq %[r],%[m]"); } else { if(toreg) DO64("andq %[m],%[r]"); else DO64("andq %[r],%[m]"); } }
    else { std::uint32_t mem32=mem, reg32=reg; if(isor) { if(toreg) DO32("orl %[m],%[r]"); else DO32("orl %[r],%[m]"); } else { if(toreg) DO32("andl %[m],%[r]"); else DO32("andl %[r],%[m]"); } mem=(mem&0xffffffff00000000ULL)|mem32; reg=toreg?reg32:reg; }
#undef DO64
#undef DO32
    Memory m(128); Cpu c; std::array<std::uint8_t,3> code{}; unsigned len=0; if(wide) code[len++]=0x48; code[len++]=op; code[len++]=0x03;
    std::array<std::uint8_t,8> bytes{}; for(unsigned j=0;j<8;++j) bytes[j]=original_mem>>(8*j);
    if(!m.write(0,std::span(code).first(len)) || !m.write(64,bytes)) return 1;
    c.state.rax=original_reg; *c.state.reg(3)=64; c.state.rflags=0x412;
    if(c.step(m)!=StepResult::running || c.state.rip!=len || c.state.rax!=reg || !m.read(64,bytes)) return 1;
    std::uint64_t actual=0; for(unsigned j=0;j<8;++j) actual|=std::uint64_t(bytes[j])<<(8*j);
    if(actual!=mem || (c.state.rflags&0x8c5)!=(flags&0x8c5) || (c.state.rflags&~0x8c5)!=(0x412&~0x8c5)) { std::cerr<<"memory mismatch "<<op<<"\n"; return 1; }
    if(!toreg) { Cpu f; f.state.rax=original_reg; *f.state.reg(3)=64; f.state.rflags=0x412; if(!m.protect(64,wide?8:4,Permission::read)) return 1;
      if(f.step(m)!=StepResult::memory_fault || f.state.rip || f.state.rflags!=0x412 || f.state.rax!=original_reg) return 1;
      std::array<std::uint8_t,8> after{}; if(!m.read(64,after) || after!=bytes) return 1; }
    ++count;
  }
  std::cout<<count<<" AND/OR native register and memory comparisons passed\n";
}
