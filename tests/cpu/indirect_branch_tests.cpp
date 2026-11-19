#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
static std::array<std::uint8_t,8> le(std::uint64_t v) { std::array<std::uint8_t,8> b{}; for (unsigned i=0;i<8;++i) b[i]=static_cast<std::uint8_t>(v>>(8*i)); return b; }
int main() {
  std::size_t count=0;
  // Register forms, including RSP and all REX.B register extensions.
  for (unsigned ext : {2u,4u,6u}) for (unsigned reg=0;reg<16;++reg) for (unsigned i=0;i<128;++i) {
    Memory m(512); Cpu c; unsigned len=reg<8?2:3; std::array<std::uint8_t,3> code{};
    if(reg<8) { code[0]=0xff; code[1]=static_cast<std::uint8_t>(0xc0|(ext<<3)|reg); }
    else { code[0]=0x41; code[1]=0xff; code[2]=static_cast<std::uint8_t>(0xc0|(ext<<3)|(reg&7)); }
    CHECK(m.write(0,std::span(code).first(len))); *c.state.reg(4)=400; if(reg!=4) *c.state.reg(reg)=100+i;
    const auto operand=*c.state.reg(reg); c.state.rflags=0x8d7;
    CHECK(c.step(m)==StepResult::running && c.state.rflags==0x8d7);
    CHECK(c.state.rip==(ext==6?len:operand)); CHECK(*c.state.reg(4)==(ext==4?400:392));
    if(ext!=4) { std::array<std::uint8_t,8> result{}; CHECK(m.read(392,result) && result==le(ext==2?len:operand)); }
    ++count;
  }
  // Memory operands: SIB [rsp] is read before CALL writes the stack.
  for(unsigned ext : {2u,4u,6u}) { Memory m(256); Cpu c; const std::array<std::uint8_t,3> code{0xff,static_cast<std::uint8_t>((ext<<3)|4),0x24};
    CHECK(m.write(0,code) && m.write(128,le(99))); *c.state.reg(4)=128;
    CHECK(c.step(m)==StepResult::running && c.state.rip==(ext==6?3:99) && *c.state.reg(4)==(ext==4?128:120));
    if(ext!=4) { std::array<std::uint8_t,8> out{}; CHECK(m.read(120,out) && out==le(ext==2?3:99)); } }
  // RIP-relative target, including REX prefix, and extended base with displacement.
  { Memory m(256); Cpu c; const std::array<std::uint8_t,7> code{0x48,0xff,0x25,57,0,0,0}; CHECK(m.write(0,code) && m.write(64,le(123)));
    CHECK(c.step(m)==StepResult::running && c.state.rip==123); }
  { Memory m(256); Cpu c; const std::array<std::uint8_t,4> code{0x41,0xff,0x60,0xf8}; CHECK(m.write(0,code) && m.write(64,le(123))); *c.state.reg(8)=72;
    CHECK(c.step(m)==StepResult::running && c.state.rip==123); }
  // Read fault, stack fault, and truncated instruction leave state unchanged.
  { Memory m(128); Cpu c; const std::array<std::uint8_t,2> code{0xff,0x13}; CHECK(m.write(0,code)); *c.state.reg(3)=64; *c.state.reg(4)=120; c.state.rflags=0x812;
    CHECK(m.protect(64,8,Permission::write)); CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && *c.state.reg(4)==120 && c.state.rflags==0x812); }
  { Memory m(128); Cpu c; const std::array<std::uint8_t,2> code{0xff,0xd0}; CHECK(m.write(0,code)); c.state.rax=42; *c.state.reg(4)=120; c.state.rflags=0x812;
    CHECK(m.protect(115,1,Permission::read)); CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && *c.state.reg(4)==120 && c.state.rflags==0x812);
    std::array<std::uint8_t,8> out{}; CHECK(m.read(112,out) && out==le(0)); }
  { const std::array<std::uint8_t,7> code{0x48,0xff,0x25,57,0,0,0}; for(unsigned n=1;n<code.size();++n) { Memory m(n); Cpu c; CHECK(m.write(0,std::span(code).first(n))); c.state.rax=42;
    CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==42); } }
  // Other FF group extensions remain unsupported.
  for(unsigned ext : {0u,1u,3u,5u,7u}) { Memory m(2); Cpu c; const std::array<std::uint8_t,2> code{0xff,static_cast<std::uint8_t>(0xc0|(ext<<3))}; CHECK(m.write(0,code));
    CHECK(c.step(m)==StepResult::invalid_instruction && c.state.rip==0); }
  std::cout << count << " indirect control/stack register scenarios passed, plus memory and fault checks\n";
}
