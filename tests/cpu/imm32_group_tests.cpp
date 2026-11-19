#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    // Sign extension in the 64-bit register form, and preservation on CMP.
    { Memory m(7); Cpu c; const std::array<std::uint8_t,7> code{0x49,0x81,0xc0,0,0,0,0x80}; CHECK(m.write(0,code));
      *c.state.reg(8)=0x80000000ULL; CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==0 && (c.state.rflags&0x40)); }
    { Memory m(6); Cpu c; const std::array<std::uint8_t,6> code{0x81,0xf8,0,0,0,0x80}; CHECK(m.write(0,code));
      c.state.rax=0xabcd000080000000ULL; CHECK(c.step(m)==StepResult::running && c.state.rax==0xabcd000080000000ULL && (c.state.rflags&0x40)); }
    // RIP-relative m32 destination: immediate is part of instruction length.
    { Memory m(128); Cpu c; const std::array<std::uint8_t,10> code{0x81,0x05,54,0,0,0,0x78,0x56,0x34,0x12};
      const std::array<std::uint8_t,4> start{1,0,0,0}, want{0x79,0x56,0x34,0x12}; CHECK(m.write(0,code) && m.write(64,start));
      CHECK(c.step(m)==StepResult::running && c.state.rip==10); std::array<std::uint8_t,4> data{}; CHECK(m.read(64,data) && data==want); }
    // Read-only memory permits CMP but rejects ADD without changing flags or bytes.
    for (unsigned ext : {0u,7u}) { Memory m(128); Cpu c; const std::array<std::uint8_t,11> code{0x48,0x81,static_cast<std::uint8_t>((ext<<3)|5),53,0,0,0,1,0,0,0};
      const std::array<std::uint8_t,8> before{1,0,0,0,0,0,0,0}; CHECK(m.write(0,code) && m.write(64,before) && m.protect(64,8,Permission::read)); c.state.rflags=0x812;
      const auto status=c.step(m); if(ext==0) CHECK(status==StepResult::memory_fault && c.state.rip==0 && c.state.rflags==0x812);
      else CHECK(status==StepResult::running && c.state.rip==11 && (c.state.rflags&0x40));
      std::array<std::uint8_t,8> after{}; CHECK(m.read(64,after) && after==before); }
    // Every truncation of the RIP-relative instruction must leave state untouched.
    { const std::array<std::uint8_t,11> code{0x48,0x81,0x05,53,0,0,0,1,2,3,4};
      for(unsigned n=1;n<code.size();++n) { Memory m(n); Cpu c; CHECK(m.write(0,std::span(code).first(n))); c.state.rax=17; c.state.rflags=0x812;
        CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==17 && c.state.rflags==0x812); } }
    // Unsupported group extension still fails without touching state.
    for (auto wide : {false,true}) { Memory m(3); Cpu c; const std::array<std::uint8_t,3> code=wide?std::array<std::uint8_t,3>{0x48,0x81,0xd0}:std::array<std::uint8_t,3>{0x81,0xd0,0};
      CHECK(m.write(0,code) && c.step(m)==StepResult::invalid_instruction && c.state.rip==0); }
    std::cout << "portable imm32 group tests passed\n";
}
