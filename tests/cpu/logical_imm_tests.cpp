#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
 for(bool wide:{false,true}) for(bool memory:{false,true}) for(unsigned opcode:{0x81u,0x83u}) for(unsigned ext:{1u,4u,6u}) {
  std::array<std::uint8_t,11> code{}; unsigned n=0; if(wide) code[n++]=0x48; code[n++]=opcode;
  if(memory) { auto len=(wide?1:0)+2+4+(opcode==0x81?4:1); code[n++]=(ext<<3)|5; code[n++]=64-len; code[n++]=0;code[n++]=0;code[n++]=0; }
  else code[n++]=0xc0|(ext<<3);
  code[n++]=0x80; if(opcode==0x81) { code[n++]=0xff; code[n++]=0xff; code[n++]=0xff; }
  Memory m(128); Cpu c; const std::uint64_t input=0x123456789abcdef0ULL;
  std::array<std::uint8_t,8> data{}; for(unsigned i=0;i<8;++i) data[i]=input>>(8*i);
  CHECK(m.write(0,std::span(code).first(n)) && m.write(64,data)); c.state.rax=input; c.state.rflags=0x812;
  auto rhs=wide?0xffffffffffffff80ULL:0xffffff80ULL; auto start=wide?input:std::uint64_t(std::uint32_t(input));
  const auto expected=ext==1?start|rhs:ext==4?start&rhs:start^rhs;
  CHECK(c.step(m)==StepResult::running && c.state.rip==n && (c.state.rflags&0x10));
  if(memory) { CHECK(c.state.rax==input && m.read(64,data)); std::uint64_t actual=0; for(unsigned i=0;i<8;++i) actual|=std::uint64_t(data[i])<<(8*i); CHECK(actual==(wide?expected:((input&0xffffffff00000000ULL)|expected))); }
  else CHECK(c.state.rax==expected);
  // A partial instruction fetch must not change CPU state.
  for(unsigned len=1;len<n;++len) { Memory partial(len); Cpu f; f.state.rax=input; f.state.rflags=0x812; CHECK(partial.write(0,std::span(code).first(len))); CHECK(f.step(partial)==StepResult::memory_fault && f.state.rip==0 && f.state.rax==input && f.state.rflags==0x812); }
  if(memory) { Memory readonly(128); Cpu f; f.state.rax=input; f.state.rflags=0x812; CHECK(readonly.write(0,std::span(code).first(n)) && readonly.write(64,data) && readonly.protect(64,wide?8:4,Permission::read)); auto before=data;
    CHECK(f.step(readonly)==StepResult::memory_fault && f.state.rip==0 && f.state.rax==input && f.state.rflags==0x812); CHECK(readonly.read(64,data) && data==before); }
 }
}
