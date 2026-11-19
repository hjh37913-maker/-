#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
static std::array<std::uint8_t,8> bytes(std::uint64_t v) { std::array<std::uint8_t,8> out{}; for(unsigned i=0;i<8;++i) out[i]=static_cast<std::uint8_t>(v>>(i*8)); return out; }
int main() {
  // Every read-modify-write must fault atomically if one byte is read-only.
  for (auto op : {0x01,0x29,0x31,0x83}) {
    Memory m(128); Cpu c; const std::array<std::uint8_t,4> code{0x48,static_cast<std::uint8_t>(op),0x03,0xff};
    CHECK(m.write(0,code) && m.write(64,bytes(0x123456789abcdef0ULL)) && m.protect(68,1,Permission::read));
    c.state.rax=42; *c.state.reg(3)=64; c.state.rflags=0x812;
    CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==42 && c.state.rflags==0x812 && *c.state.reg(3)==64);
    std::array<std::uint8_t,8> data{}; CHECK(m.read(64,data) && data==bytes(0x123456789abcdef0ULL));
  }
  // Source-only operands need read but not write access.
  for (auto op : {0x03,0x2b,0x33,0x39,0x3b,0x85}) {
    Memory m(128); Cpu c; const std::array<std::uint8_t,3> code{0x48,static_cast<std::uint8_t>(op),0x03};
    CHECK(m.write(0,code) && m.write(64,bytes(5)) && m.protect(64,8,Permission::read));
    *c.state.reg(3)=64; c.state.rax=7; CHECK(c.step(m)==StepResult::running);
  }
  // RIP-relative immediate uses end of full instruction including the immediate.
  for (unsigned ext : {0u,5u,7u}) for (unsigned imm=0;imm<256;++imm) {
    Memory m(128), ref(4); Cpu c, r;
    const std::array<std::uint8_t,8> code{0x48,0x83,static_cast<std::uint8_t>(0x05|(ext<<3)),56,0,0,0,static_cast<std::uint8_t>(imm)};
    const std::array<std::uint8_t,4> regcode{0x48,0x83,static_cast<std::uint8_t>(0xc0|(ext<<3)),static_cast<std::uint8_t>(imm)};
    CHECK(m.write(0,code) && m.write(64,bytes(0x8000000000000042ULL)) && ref.write(0,regcode));
    c.state.rflags=r.state.rflags=0x412; r.state.rax=0x8000000000000042ULL;
    CHECK(c.step(m)==StepResult::running && r.step(ref)==StepResult::running && c.state.rip==8 && c.state.rflags==r.state.rflags);
    std::array<std::uint8_t,8> data{}; CHECK(m.read(64,data) && data==bytes(r.state.rax));
  }
  // MOV imm32 sign-extends, with RIP-relative addressing including the immediate.
  { Memory m(128); Cpu c; const std::array<std::uint8_t,11> code{0x48,0xc7,0x05,53,0,0,0,0,0,0,0x80};
    CHECK(m.write(0,code)); c.state.rflags=0x812; CHECK(c.step(m)==StepResult::running && c.state.rip==11 && c.state.rflags==0x812);
    std::array<std::uint8_t,8> data{}; CHECK(m.read(64,data) && data==bytes(0xffffffff80000000ULL)); }
  { Memory m(7); Cpu c; const std::array<std::uint8_t,7> code{0x49,0xc7,0xc0,0xff,0xff,0xff,0xff};
    CHECK(m.write(0,code) && c.step(m)==StepResult::running && *c.state.reg(8)==UINT64_MAX); }
  { Memory m(128); Cpu c; const std::array<std::uint8_t,7> code{0x48,0xc7,0x03,1,2,3,4};
    CHECK(m.write(0,code) && m.write(64,bytes(5)) && m.protect(68,1,Permission::read)); *c.state.reg(3)=64; c.state.rflags=0x812;
    CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rflags==0x812);
    std::array<std::uint8_t,8> data{}; CHECK(m.read(64,data) && data==bytes(5)); }
  { Memory m(3); Cpu c; const std::array<std::uint8_t,3> code{0x48,0xc7,0xc8}; CHECK(m.write(0,code));
    CHECK(c.step(m)==StepResult::invalid_instruction && c.state.rip==0); }
  // All truncated byte lengths fault before any operand access or state change.
  { const std::array<std::uint8_t,11> code{0x48,0xc7,0x05,53,0,0,0,0,0,0,0x80};
    for (unsigned n=1;n<code.size();++n) { Memory m(n); Cpu c; CHECK(m.write(0,std::span(code).first(n))); c.state.rax=17; c.state.rflags=0x812;
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==17 && c.state.rflags==0x812); } }
  // Address is computed before writing a register that is also the base.
  { Memory m(128); Cpu c; const std::array<std::uint8_t,3> code{0x48,0x03,0x00};
    CHECK(m.write(0,code) && m.write(64,bytes(9))); c.state.rax=64;
    CHECK(c.step(m)==StepResult::running && c.state.rax==73); }
  // Extended REX.R/B and SIB base/index are interpreted from original registers.
  { Memory m(128); Cpu c; const std::array<std::uint8_t,4> code{0x4f,0x03,0x44,0x4c}; // incomplete disp8
    CHECK(m.write(0,code)); CHECK(m.protect(4,124,Permission::read | Permission::write));
    c.state.rflags=0x812; CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rflags==0x812); }
  { Memory m(128); Cpu c; const std::array<std::uint8_t,5> code{0x4f,0x03,0x44,0x4c,0xf8};
    CHECK(m.write(0,code) && m.write(64,bytes(9))); *c.state.reg(12)=56; *c.state.reg(9)=8; *c.state.reg(8)=4;
    CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==13 && *c.state.reg(12)==56 && *c.state.reg(9)==8); }
  { Memory m(128); Cpu c; const std::array<std::uint8_t,3> code{0x48,0x85,0x03}; CHECK(m.write(0,code) && m.protect(64,8,Permission::write));
    c.state.rax=1; *c.state.reg(3)=64; c.state.rflags=0x812; CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rflags==0x812); }
  { Memory m(3); Cpu c; const std::array<std::uint8_t,3> code{0x48,0x83,0x0b}; CHECK(m.write(0,code)); CHECK(c.step(m)==StepResult::invalid_instruction && c.state.rip==0); }
  std::cout << "768 immediate cross-checks and 64-bit memory fault/address tests passed\n";
}
