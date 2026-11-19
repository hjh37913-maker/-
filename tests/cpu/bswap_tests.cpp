#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(0)
int main() {
 for(bool wide:{false,true}) for(unsigned reg=0;reg<16;++reg) {
  Memory mem(32); Cpu cpu; std::array<std::uint8_t,3> code{std::uint8_t(0x40|(wide?8:0)|(reg>=8?1:0)),0x0f,std::uint8_t(0xc8|(reg&7))};
  CHECK(mem.write(0,code)); *cpu.state.reg(reg)=0x0123456789abcdefULL; cpu.state.rflags=0x8d7;
  CHECK(cpu.step(mem)==StepResult::running && *cpu.state.reg(reg)==(wide?0xefcdab8967452301ULL:0xefcdab89ULL) && cpu.state.rflags==0x8d7 && cpu.state.rip==3);
 }
 // Truncated instruction fetch leaves the CPU state unchanged.
 { Memory mem(2); Cpu cpu; const std::array<std::uint8_t,2> code{0x48,0x0f}; CHECK(mem.write(0,code)); cpu.state.rax=42; cpu.state.rflags=0x8d7;
   CHECK(cpu.step(mem)==StepResult::memory_fault && cpu.state.rax==42 && cpu.state.rflags==0x8d7 && cpu.state.rip==0); }
 std::cout<<"BSWAP extended-register and fault checks passed\n";
}
