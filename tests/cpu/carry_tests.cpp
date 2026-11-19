#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(0)
int main() {
 // ADD with carry crosses a 64-bit limb; SBB consumes that carry.
 { Memory m(32); Cpu c; const std::array<std::uint8_t,6> op{0x48,0x11,0xd8,0x48,0x19,0xc9}; CHECK(m.write(0,op)); c.state.rax=~std::uint64_t{0}; *c.state.reg(3)=0; *c.state.reg(1)=7; c.state.rflags=3;
   CHECK(c.step(m)==StepResult::running && c.state.rax==0 && (c.state.rflags&1));
   CHECK(c.step(m)==StepResult::running && *c.state.reg(1)==~std::uint64_t{0} && (c.state.rflags&1)); }
 // AH target when no REX, SPL target when REX is present.
 { Memory m(16); Cpu c; const std::array<std::uint8_t,2> op{0x10,0xc4}; CHECK(m.write(0,op)); c.state.rax=0x102; c.state.rflags=3;
   CHECK(c.step(m)==StepResult::running && c.state.rax==0x402); }
 { Memory m(16); Cpu c; const std::array<std::uint8_t,3> op{0x40,0x10,0xc4}; CHECK(m.write(0,op)); c.state.rax=2; *c.state.reg(4)=1; c.state.rflags=3;
   CHECK(c.step(m)==StepResult::running && c.state.rax==2 && *c.state.reg(4)==4); }
 // Immediate RIP-relative memory write and atomic write fault.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,8> op{0x48,0x83,0x15,0x38,0,0,0,0xff}; const std::array<std::uint8_t,8> val{10,0,0,0,0,0,0,0}; CHECK(m.write(0,op)&&m.write(64,val)); c.state.rflags=3;
   CHECK(c.step(m)==StepResult::running && c.state.rip==8); std::array<std::uint8_t,8> got{}; CHECK(m.read(64,got)&&got==val);
   c.state.rip=0; c.state.rflags=3; CHECK(m.protect(64,8,Permission::read)); const auto before=c.state;
   CHECK(c.step(m)==StepResult::memory_fault && c.state.rax==before.rax && c.state.other_registers==before.other_registers && c.state.rflags==before.rflags && c.state.rip==before.rip); }
 // A truncated immediate is a fetch fault without modification.
 { Memory m(4); Cpu c; const std::array<std::uint8_t,4> op{0x48,0x83,0xd0,0xff}; CHECK(m.write(0,op)); CHECK(m.protect(3,1,Permission::read)); c.state.rax=7; c.state.rflags=3;
   CHECK(c.step(m)==StepResult::memory_fault && c.state.rax==7 && c.state.rflags==3 && c.state.rip==0); }
 std::cout<<"ADC/SBB portable carry, aliases and atomic fault checks passed\n";
}
