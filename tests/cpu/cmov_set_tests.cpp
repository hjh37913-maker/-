#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(0)
int main() {
 // False CMOV still reads and faults on unreadable memory.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,4> op{0x48,0x0f,0x44,0x03}; CHECK(m.write(0,op)&&m.protect(64,8,Permission::write)); c.state.rax=5; *c.state.reg(3)=64; c.state.rflags=2; const auto before=c.state;
   CHECK(c.step(m)==StepResult::memory_fault && c.state.rax==before.rax && c.state.other_registers==before.other_registers && c.state.rip==0 && c.state.rflags==before.rflags); }
 // False 32-bit CMOV zeroes the high half in 64-bit mode.
 { Memory m(16); Cpu c; const std::array<std::uint8_t,3> op{0x0f,0x44,0xc3}; CHECK(m.write(0,op)); c.state.rax=0xffff000000000007ULL; *c.state.reg(3)=3; c.state.rflags=2;
   CHECK(c.step(m)==StepResult::running && c.state.rax==7 && c.state.rflags==2); }
 // True word CMOV preserves upper register bits.
 { Memory m(16); Cpu c; const std::array<std::uint8_t,5> op{0x66,0x45,0x0f,0x44,0xc1}; CHECK(m.write(0,op)); *c.state.reg(8)=0x123456789abc0000ULL; *c.state.reg(9)=0xabcd; c.state.rflags=0x42;
   CHECK(c.step(m)==StepResult::running && *c.state.reg(8)==0x123456789abcabcdULL && c.state.rflags==0x42); }
 // AH and SPL selected correctly; REX.B extends byte targets.
 { Memory m(16); Cpu c; const std::array<std::uint8_t,3> op{0x0f,0x94,0xc4}; CHECK(m.write(0,op)); c.state.rax=0x12340055; c.state.rflags=0x42;
   CHECK(c.step(m)==StepResult::running && c.state.rax==0x12340155 && c.state.rflags==0x42); }
 { Memory m(16); Cpu c; const std::array<std::uint8_t,4> op{0x40,0x0f,0x94,0xc4}; CHECK(m.write(0,op)); c.state.rax=0x55; *c.state.reg(4)=0x123400ff; c.state.rflags=0x42;
   CHECK(c.step(m)==StepResult::running && *c.state.reg(4)==0x12340001 && c.state.rax==0x55); }
 // SETcc write-only destination is legal and faults do not advance RIP.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,3> op{0x0f,0x94,0x03}; CHECK(m.write(0,op)); *c.state.reg(3)=64; c.state.rflags=0x42; CHECK(m.protect(64,1,Permission::write));
   CHECK(c.step(m)==StepResult::running && c.state.rip==3 && c.state.rflags==0x42); CHECK(m.protect(64,1,Permission::read)); std::array<std::uint8_t,1> data{}; CHECK(m.read(64,data)&&data[0]==1);
   c.state.rip=0; CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && c.state.rflags==0x42); }
 std::cout<<"CMOVcc/SETcc memory, false branch, REX and fault checks passed\n";
}
