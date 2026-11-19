#include "core/machine.h"
#include <array>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while(0)
int main() {
  { Machine m(3); const std::array<std::uint8_t,3> code{0x90,0x0f,0x0b}; CHECK(m.memory.write(0,code));
    const auto r=m.run_detailed(9); CHECK(r.result==RunResult::invalid_instruction && r.attempted==2 && r.stop_rip==1 && m.cpu.state.rip==1);
    CHECK(r.fault_byte_count==2 && r.fault_bytes[0]==0x0f && r.fault_bytes[1]==0x0b); }
  { Machine m(16); const std::array<std::uint8_t,2> code{0xff,0x13}; CHECK(m.memory.write(0,code));
    *m.cpu.state.reg(3)=32; const auto r=m.run_detailed(9); CHECK(r.result==RunResult::memory_fault && r.stop_rip==0 && r.attempted==1 && r.fault_byte_count==15); }
  { Machine m(4); CHECK(m.memory.protect(0,4,Permission::read)); auto r=m.run_detailed(2);
    CHECK(r.result==RunResult::memory_fault && r.stop_rip==0 && r.attempted==1 && r.fault_byte_count==0); }
  { Machine m(2); const std::array<std::uint8_t,2> code{0x90,0xf4}; CHECK(m.memory.write(0,code));
    auto r=m.run_detailed(1); CHECK(r.result==RunResult::budget_exhausted && r.attempted==1 && r.stop_rip==1 && r.fault_byte_count==0);
    r=m.run_detailed(1); CHECK(r.result==RunResult::halted && r.attempted==1 && r.stop_rip==1 && m.cpu.state.rip==2); }
  { Machine m(1); m.cpu.state.rip=UINT64_MAX; auto r=m.run_detailed(0);
    CHECK(r.result==RunResult::budget_exhausted && r.attempted==0 && r.stop_rip==UINT64_MAX);
    r=m.run_detailed(1); CHECK(r.result==RunResult::memory_fault && r.attempted==1 && r.fault_byte_count==0); }
  std::cout << "run diagnostics passed\n";
}
