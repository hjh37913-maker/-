#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(0)
int main() {
 // Unsigned RDX:RAX product, source aliases RDX: source must be snapshotted.
 { Memory m(32); Cpu c; const std::array<std::uint8_t,3> op{0x48,0xf7,0xe2}; CHECK(m.write(0,op)); c.state.rax=~std::uint64_t{0}; *c.state.reg(2)=2; c.state.rflags=0x2d6;
   CHECK(c.step(m)==StepResult::running && c.state.rax==0xfffffffffffffffeULL && *c.state.reg(2)==1 && (c.state.rflags&0x801)==0x801 && c.state.rip==3); }
 // 16-bit signed multiplication keeps upper register bits and sets overflow.
 { Memory m(32); Cpu c; const std::array<std::uint8_t,3> op{0x66,0xf7,0xeb}; CHECK(m.write(0,op)); c.state.rax=0x12348000; *c.state.reg(2)=0x56780000; *c.state.reg(3)=2;
   CHECK(c.step(m)==StepResult::running && c.state.rax==0x12340000 && *c.state.reg(2)==0x5678ffff && (c.state.rflags&0x801)==0x801); }
 // Faults in operand and instruction fetch must not update architectural state.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,3> op{0x48,0xf7,0x2b}; CHECK(m.write(0,op)); c.state.rax=3; *c.state.reg(2)=7; *c.state.reg(3)=64; c.state.rflags=0x8d7;
   CHECK(m.protect(64,8,Permission::write)); CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==3 && *c.state.reg(2)==7 && c.state.rflags==0x8d7); }
 { Memory m(2); Cpu c; const std::array<std::uint8_t,2> op{0x48,0xf7}; CHECK(m.write(0,op)); c.state.rax=3; c.state.rflags=0x8d7;
   CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rax==3 && c.state.rflags==0x8d7); }
 // Legacy AH source aliases the result; a REX prefix selects SPL instead.
 { Memory m(32); Cpu c; const std::array<std::uint8_t,2> op{0xf6,0xe4}; CHECK(m.write(0,op)); c.state.rax=0x12340203; c.state.rflags=2;
   CHECK(c.step(m)==StepResult::running && c.state.rax==0x12340006 && (c.state.rflags&0x801)==0); }
 { Memory m(32); Cpu c; const std::array<std::uint8_t,3> op{0x40,0xf6,0xec}; CHECK(m.write(0,op)); c.state.rax=0x12340080; *c.state.reg(4)=0xff;
   CHECK(c.step(m)==StepResult::running && c.state.rax==0x12340080 && (c.state.rflags&0x801)==0x801); }
 // RIP-relative memory MUL and a scaled-index SIB source with REX.X/B.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,7> op{0x48,0xf7,0x25,0x39,0,0,0}; const std::array<std::uint8_t,8> val{4,0,0,0,0,0,0,0};
   CHECK(m.write(0,op) && m.write(64,val)); c.state.rax=3;
   CHECK(c.step(m)==StepResult::running && c.state.rax==12 && *c.state.reg(2)==0 && c.state.rip==7); }
 { Memory m(128); Cpu c; const std::array<std::uint8_t,4> op{0x4b,0xf7,0x24,0x91}; const std::array<std::uint8_t,8> val{6,0,0,0,0,0,0,0};
   CHECK(m.write(0,op) && m.write(64,val)); c.state.rax=7; *c.state.reg(9)=48; *c.state.reg(10)=4;
   CHECK(c.step(m)==StepResult::running && c.state.rax==42 && *c.state.reg(2)==0 && c.state.rip==4); }
 // An execute-only operand cannot be read; preserve the complete CPU state.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,3> op{0x48,0xf7,0x23}; CHECK(m.write(0,op));
   c.state.rax=5; *c.state.reg(2)=9; *c.state.reg(3)=64; c.state.rflags=0x8d7; const auto before=c.state;
   CHECK(m.protect(64,8,Permission::execute)); CHECK(c.step(m)==StepResult::memory_fault);
   CHECK(c.state.rax==before.rax && c.state.other_registers==before.other_registers && c.state.rip==before.rip && c.state.rflags==before.rflags); }
 std::cout<<"MUL/IMUL full product, register aliases and fault checks passed\n";
}
