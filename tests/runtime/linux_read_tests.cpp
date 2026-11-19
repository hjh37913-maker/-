#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall;
using namespace nightfall::runtime;
#define CHECK(x) do { if (!(x)) { std::cerr<<__LINE__<<": " #x " failed\n"; return 1; } } while(0)
int main() {
 const std::array<std::uint8_t,2> syscall{0x0f,0x05}; const std::array<std::uint8_t,4> input{'a','b','c','d'};
 { Machine m(256); CHECK(m.memory.write(0,syscall)); LinuxProcess p(m); CHECK(p.set_stdin(input));
   m.cpu.state.rax=0; *m.cpu.state.reg(7)=0x100000000ULL; *m.cpu.state.reg(6)=128; *m.cpu.state.reg(2)=2;
   CHECK(p.run(1).result==ProcessResult::budget_exhausted && m.cpu.state.rax==2); std::array<std::uint8_t,4> out{}; CHECK(m.memory.read(128,out) && out[0]=='a' && out[1]=='b');
   CHECK(!p.set_stdin(input)); m.cpu.state.rip=0; m.cpu.state.rax=0; *m.cpu.state.reg(6)=130; *m.cpu.state.reg(2)=UINT64_MAX;
   CHECK(p.run(1).result==ProcessResult::budget_exhausted && m.cpu.state.rax==2 && m.memory.read(128,out) && out==input);
   m.cpu.state.rip=0; m.cpu.state.rax=0; *m.cpu.state.reg(6)=UINT64_MAX; CHECK(p.run(1).result==ProcessResult::budget_exhausted && m.cpu.state.rax==0);
 }
 // A failed multi-byte write leaves memory and cursor unchanged.
 { Machine m(256); CHECK(m.memory.write(0,syscall)); CHECK(m.memory.protect(130,1,Permission::read)); LinuxProcess p(m); CHECK(p.set_stdin(input));
   m.cpu.state.rax=0; *m.cpu.state.reg(7)=0; *m.cpu.state.reg(6)=128; *m.cpu.state.reg(2)=4;
   CHECK(p.run(1).result==ProcessResult::budget_exhausted && m.cpu.state.rax==std::uint64_t{0}-14);
   std::array<std::uint8_t,4> out{}; CHECK(m.memory.read(128,out) && (out==std::array<std::uint8_t,4>{}));
   CHECK(m.memory.protect(130,1,Permission::read|Permission::write)); m.cpu.state.rip=0; m.cpu.state.rax=0;
   CHECK(p.run(1).result==ProcessResult::budget_exhausted && m.cpu.state.rax==4 && m.memory.read(128,out) && out==input);
 }
 { Machine m(16); CHECK(m.memory.write(0,syscall)); LinuxProcess p(m); CHECK(!p.set_stdin(std::vector<std::uint8_t>(65537)) && p.set_stdin(input));
   m.cpu.state.rax=0; *m.cpu.state.reg(7)=1; *m.cpu.state.reg(2)=0;
   CHECK(p.run(1).result==ProcessResult::budget_exhausted && m.cpu.state.rax==std::uint64_t{0}-9);
 }
 // Zero instruction budget does not start or consume the process.
 { Machine m(16); LinuxProcess p(m); CHECK(p.run(0).result==ProcessResult::budget_exhausted && p.set_stdin(input)); }
 std::cout<<"bounded stdin read, EOF, short reads and transactional guest faults passed\n";
}
