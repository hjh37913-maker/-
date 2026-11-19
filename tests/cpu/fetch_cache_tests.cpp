#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(0)
int main() {
 // A new step re-fetches modified executable bytes; no cache spans steps.
 { Memory m(32); Cpu c; const std::array<std::uint8_t,7> code{0x48,0xc7,0xc0,1,0,0,0}; CHECK(m.write(0,code));
   CHECK(c.step(m)==StepResult::running && c.state.rax==1); const std::array<std::uint8_t,1> two{2}; CHECK(m.write(3,two)); c.state.rip=0;
   CHECK(c.step(m)==StepResult::running && c.state.rax==2); c.state.rip=0; CHECK(m.protect(3,1,Permission::read|Permission::write));
   const auto before=c.state; CHECK(c.step(m)==StepResult::memory_fault && c.state.rax==before.rax && c.state.rip==0 && c.state.rflags==before.rflags); }
 // REP restart re-fetches its opcode on every step, by design.
 { Memory m(64); Cpu c; const std::array<std::uint8_t,2> code{0xf3,0xaa}; CHECK(m.write(0,code)); *c.state.reg(1)=2; *c.state.reg(7)=32; c.state.rax=0x42;
   CHECK(c.step(m)==StepResult::running && c.state.rip==0 && *c.state.reg(1)==1);
   CHECK(m.protect(1,1,Permission::read|Permission::write)); const auto before=c.state;
   CHECK(c.step(m)==StepResult::memory_fault && c.state.rax==before.rax && c.state.other_registers==before.other_registers && c.state.rip==0); }
 std::cout<<"per-step fetch cache respects modified bytes and permissions\n";
}
