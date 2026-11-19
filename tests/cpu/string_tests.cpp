#include "cpu/cpu.h"
#include "core/machine.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(0)
int main() {
 // REP MOVSB is resumable: each budget unit commits at most one byte.
 { Machine m(128); const std::array<std::uint8_t,3> code{0xf3,0xa4,0xf4}; const std::array<std::uint8_t,3> src{'a','b','c'}; CHECK(m.memory.write(0,code)&&m.memory.write(32,src));
   *m.cpu.state.reg(1)=3; *m.cpu.state.reg(6)=32; *m.cpu.state.reg(7)=64; m.cpu.state.rflags=0x8d7;
   auto a=m.run_detailed(2); CHECK(a.result==RunResult::budget_exhausted && a.attempted==2 && m.cpu.state.rip==0 && *m.cpu.state.reg(1)==1 && *m.cpu.state.reg(6)==34 && *m.cpu.state.reg(7)==66);
   auto b=m.run_detailed(2); CHECK(b.result==RunResult::halted && b.attempted==2 && *m.cpu.state.reg(1)==0 && m.cpu.state.rflags==0x8d7); std::array<std::uint8_t,3> out{}; CHECK(m.memory.read(64,out)&&out==src); }
 // Zero count never touches source or destination, including invalid pointers.
 { Machine m(16); const std::array<std::uint8_t,2> code{0xf3,0xa4}; CHECK(m.memory.write(0,code)); *m.cpu.state.reg(1)=0; *m.cpu.state.reg(6)=UINT64_MAX; *m.cpu.state.reg(7)=UINT64_MAX;
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rip==2); }
 // A second-iteration destination fault preserves the completed first byte.
 { Machine m(128); const std::array<std::uint8_t,2> code{0xf3,0xa4}; const std::array<std::uint8_t,2> src{4,5}; CHECK(m.memory.write(0,code)&&m.memory.write(32,src)); CHECK(m.memory.protect(65,1,Permission::read));
   *m.cpu.state.reg(1)=2; *m.cpu.state.reg(6)=32; *m.cpu.state.reg(7)=64; CHECK(m.cpu.step(m.memory)==StepResult::running); auto before=m.cpu.state;
   CHECK(m.cpu.step(m.memory)==StepResult::memory_fault && m.cpu.state.rax==before.rax && m.cpu.state.other_registers==before.other_registers && m.cpu.state.rip==0 && m.cpu.state.rflags==before.rflags);
   std::array<std::uint8_t,2> out{}; CHECK(m.memory.read(64,out) && out[0]==4 && out[1]==0); }
 // Non-REP backward move does not change RCX and advances the instruction.
 { Machine m(128); const std::array<std::uint8_t,3> code{0xfd,0xa4,0xfc}; const std::array<std::uint8_t,1> src{8}; CHECK(m.memory.write(0,code)&&m.memory.write(32,src));
   *m.cpu.state.reg(1)=9; *m.cpu.state.reg(6)=32; *m.cpu.state.reg(7)=64; m.cpu.state.rflags=0x8d7;
   CHECK(m.cpu.step(m.memory)==StepResult::running && (m.cpu.state.rflags&0x400)); CHECK(m.cpu.step(m.memory)==StepResult::running && *m.cpu.state.reg(1)==9 && *m.cpu.state.reg(6)==31 && *m.cpu.state.reg(7)==63);
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rflags==0x8d7); }
 // REP STOSQ and REP LODSD preserve appropriate registers and widths.
 { Machine m(128); const std::array<std::uint8_t,3> code{0xf3,0x48,0xab}; CHECK(m.memory.write(0,code)); m.cpu.state.rax=0x1122334455667788ULL; *m.cpu.state.reg(1)=2; *m.cpu.state.reg(7)=64;
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rip==0); CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rip==3 && *m.cpu.state.reg(7)==80);
   std::array<std::uint8_t,16> out{}; CHECK(m.memory.read(64,out)&&out[0]==0x88&&out[8]==0x88&&out[7]==0x11&&out[15]==0x11); }
 { Machine m(128); const std::array<std::uint8_t,2> code{0xf3,0xad}; const std::array<std::uint8_t,8> src{1,2,3,4,5,6,7,8}; CHECK(m.memory.write(0,code)&&m.memory.write(32,src)); m.cpu.state.rax=~std::uint64_t{0}; *m.cpu.state.reg(1)=2; *m.cpu.state.reg(6)=32;
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rax==0x04030201); CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rax==0x08070605 && m.cpu.state.rip==2); }
 // REPE CMPSB stops on the first mismatch and keeps the compared flags.
 { Machine m(128); const std::array<std::uint8_t,2> code{0xf3,0xa6}; const std::array<std::uint8_t,3> a{1,2,3}, b{1,9,3}; CHECK(m.memory.write(0,code)&&m.memory.write(32,a)&&m.memory.write(64,b));
   *m.cpu.state.reg(1)=3; *m.cpu.state.reg(6)=32; *m.cpu.state.reg(7)=64; m.cpu.state.rflags=2;
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rip==0 && (m.cpu.state.rflags&0x40));
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rip==2 && !(m.cpu.state.rflags&0x40) && (m.cpu.state.rflags&1) && *m.cpu.state.reg(1)==1); }
 // REPNE SCASB finds the matching byte and leaves count/pointers after it.
 { Machine m(128); const std::array<std::uint8_t,2> code{0xf2,0xae}; const std::array<std::uint8_t,3> src{1,4,9}; CHECK(m.memory.write(0,code)&&m.memory.write(64,src));
   m.cpu.state.rax=4; *m.cpu.state.reg(1)=3; *m.cpu.state.reg(7)=64;
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rip==0 && !(m.cpu.state.rflags&0x40));
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rip==2 && (m.cpu.state.rflags&0x40) && *m.cpu.state.reg(1)==1 && *m.cpu.state.reg(7)==66); }
 // A comparison's second read fault changes neither flags nor registers.
 { Machine m(128); const std::array<std::uint8_t,2> code{0xf3,0xa6}; CHECK(m.memory.write(0,code)&&m.memory.protect(64,1,Permission::write));
   *m.cpu.state.reg(1)=2; *m.cpu.state.reg(6)=32; *m.cpu.state.reg(7)=64; m.cpu.state.rflags=0x8d7; const auto before=m.cpu.state;
   CHECK(m.cpu.step(m.memory)==StepResult::memory_fault && m.cpu.state.other_registers==before.other_registers && m.cpu.state.rax==before.rax && m.cpu.state.rip==0 && m.cpu.state.rflags==before.rflags); }
 // REPNE MOVSB is intentionally rejected by this subset.
 { Machine m(16); const std::array<std::uint8_t,2> code{0xf2,0xa4}; CHECK(m.memory.write(0,code)); CHECK(m.cpu.step(m.memory)==StepResult::invalid_instruction && m.cpu.state.rip==0); }
 // Huge repeat counts cannot bypass the host's bounded step budget.
 { Machine m(128); const std::array<std::uint8_t,2> code{0xf3,0xaa}; CHECK(m.memory.write(0,code)); *m.cpu.state.reg(1)=UINT64_MAX; *m.cpu.state.reg(7)=64; m.cpu.state.rax=0x5a;
   auto report=m.run_detailed(4); CHECK(report.result==RunResult::budget_exhausted && report.attempted==4 && m.cpu.state.rip==0 && *m.cpu.state.reg(1)==UINT64_MAX-4 && *m.cpu.state.reg(7)==68);
   std::array<std::uint8_t,5> bytes{}; CHECK(m.memory.read(64,bytes) && (bytes==std::array<std::uint8_t,5>{0x5a,0x5a,0x5a,0x5a,0})); }
 // Fault on the first element commits no pointers, count, flags or bytes.
 { Machine m(128); const std::array<std::uint8_t,2> code{0xf3,0xa4}; CHECK(m.memory.write(0,code)&&m.memory.protect(32,1,Permission::write)); *m.cpu.state.reg(1)=3; *m.cpu.state.reg(6)=32; *m.cpu.state.reg(7)=64; m.cpu.state.rflags=0x8d7; const auto before=m.cpu.state;
   CHECK(m.cpu.step(m.memory)==StepResult::memory_fault && m.cpu.state.rax==before.rax && m.cpu.state.other_registers==before.other_registers && m.cpu.state.rflags==before.rflags && m.cpu.state.rip==0); }
 std::cout<<"resumable string instructions, direction and transactional faults passed\n";
}
