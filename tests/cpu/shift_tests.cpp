#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
 for(bool wide:{false,true}) for(unsigned ext:{4u,5u,7u}) for(unsigned op:{0xc1u,0xd1u,0xd3u}) for(unsigned raw:{0u,1u,2u,31u,32u,63u,64u,255u}) {
  if(op==0xd1 && raw!=1) continue;
  std::array<std::uint8_t,4> code{}; unsigned n=0; if(wide) code[n++]=0x48; code[n++]=op; code[n++]=0xc0|(ext<<3); if(op==0xc1) code[n++]=raw;
  Memory m(16); CHECK(m.write(0,std::span(code).first(n))); Cpu c;
  const std::uint64_t lhs=0xfedcba9876543210ULL; c.state.rax=lhs; *c.state.reg(1)=raw; c.state.rflags=0x8d7;
  unsigned width=wide?64:32, count=(op==0xd1?1:raw)&(wide?63:31); auto mask=wide?~std::uint64_t{0}:0xffffffffULL, in=lhs&mask, want=in;
  if(count) { if(ext==4) want=(in<<count)&mask; else { want=in>>count; if(ext==7 && (in>>(width-1))) want|=mask^(mask>>count); } }
  CHECK(c.step(m)==StepResult::running && c.state.rip==n && c.state.rax==want);
  if(!count) CHECK(c.state.rflags==0x8d7); else { CHECK((c.state.rflags&0x10)==0x10); CHECK(((c.state.rflags>>6)&1)==(want==0)); }
  for(unsigned j=1;j<n;++j) { Memory part(j); Cpu f; CHECK(part.write(0,std::span(code).first(j))); f.state.rax=lhs; f.state.rflags=0x8d7;
   CHECK(f.step(part)==StepResult::memory_fault && f.state.rip==0 && f.state.rax==lhs && f.state.rflags==0x8d7); }
 }
 // REX extensions select every register correctly; irrelevant X/R bits have no register effect.
 for(unsigned rex=0x40;rex<=0x4f;++rex) for(unsigned ext:{4u,5u,7u}) for(unsigned rm=0;rm<8;++rm) {
  const std::array<std::uint8_t,3> code{std::uint8_t(rex),0xd1,std::uint8_t(0xc0|(ext<<3)|rm)};
  Memory m(8); CHECK(m.write(0,code)); Cpu c; const unsigned index=rm+((rex&1)?8:0); const auto input=0xfedcba9876543210ULL;
  *c.state.reg(index)=input; const unsigned width=(rex&8)?64:32; const auto mask=width==64?~std::uint64_t{0}:0xffffffffULL; const auto lhs=input&mask;
  auto expected=ext==4?(lhs<<1)&mask:lhs>>1; if(ext==7 && (lhs>>(width-1))) expected|=std::uint64_t{1}<<(width-1);
  CHECK(c.step(m)==StepResult::running && c.state.rip==3 && *c.state.reg(index)==expected);
 }
 // SIB address uses extended base and index before the RMW.
 { Memory m(128); const std::array<std::uint8_t,5> code{0x4b,0xd1,0x64,0x4c,0xf8}; const std::array<std::uint8_t,8> data{3,0,0,0,0,0,0,0};
   CHECK(m.write(0,code)&&m.write(64,data)); Cpu c; *c.state.reg(12)=56; *c.state.reg(9)=8;
   CHECK(c.step(m)==StepResult::running && c.state.rip==5); std::array<std::uint8_t,8> out{}; CHECK(m.read(64,out)&&out[0]==6); }
 // Test CL aliases with its own destination; evaluate old CL, then update ECX.
 { Memory m(2); std::array<std::uint8_t,2> code{0xd3,0xe1}; CHECK(m.write(0,code)); Cpu c; *c.state.reg(1)=0x100000002ULL;
   CHECK(c.step(m)==StepResult::running && *c.state.reg(1)==8); }
 // RIP-relative memory shift with immediate uses the complete instruction length.
 { Memory m(128); const std::array<std::uint8_t,7> code{0xc1,0x25,57,0,0,0,1}; const std::array<std::uint8_t,4> data{0x21,0,0,0}; CHECK(m.write(0,code)&&m.write(64,data)); Cpu c; c.state.rflags=0x812;
   CHECK(c.step(m)==StepResult::running && c.state.rip==7); std::array<std::uint8_t,4> out{}; CHECK(m.read(64,out) && out[0]==0x42);
   CHECK(m.protect(64,4,Permission::read)); Cpu f; f.state.rflags=0x812; CHECK(f.step(m)==StepResult::memory_fault && f.state.rip==0 && f.state.rflags==0x812); CHECK(m.read(64,out)&&out[0]==0x42); }
 // Zero-count memory operation still requires write permission in this model.
 { Memory m(64); const std::array<std::uint8_t,3> code{0xc1,0x23,0}; CHECK(m.write(0,code)&&m.protect(32,4,Permission::read)); Cpu c; *c.state.reg(3)=32;
   CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0); }
}
