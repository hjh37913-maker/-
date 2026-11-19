#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
using namespace nightfall;
using namespace nightfall::runtime;
#define CHECK(x) do { if (!(x)) { std::cerr<<__LINE__<<": " #x " failed\n"; return 1; } } while(0)
std::uint64_t le(const std::array<std::uint8_t,8>& v) { std::uint64_t x=0; for(unsigned i=0;i<8;++i) x|=std::uint64_t(v[i])<<(8*i); return x; }
int main() {
 // SYSCALL is a trap, not a host call. Without a dispatcher it preserves CPU state.
 { Machine m(64); const std::array<std::uint8_t,2> code{0x0f,0x05}; CHECK(m.memory.write(0,code)); m.cpu.state.rax=7; *m.cpu.state.reg(1)=99;
   auto r=m.run_detailed(2); CHECK(r.result==RunResult::syscall_trap && r.attempted==1 && r.stop_rip==0 && m.cpu.state.rip==0 && m.cpu.state.rax==7 && *m.cpu.state.reg(1)==99); }
 { Machine m(1); const std::array<std::uint8_t,1> code{0x0f}; CHECK(m.memory.write(0,code)); m.cpu.state.rax=7;
   CHECK(m.cpu.step(m.memory)==StepResult::memory_fault && m.cpu.state.rip==0 && m.cpu.state.rax==7); }
 // Stack layout: argc, argv pointers, NULL, envp NULL, AT_NULL pair, 16-byte alignment.
 { Machine m(8192); const std::array<std::string,2> args{"guest","hello"}; CHECK(prepare_linux_stack(m,4096,4096,args)); const auto rsp=*m.cpu.state.reg(4); CHECK(rsp%16==0);
   std::array<std::uint8_t,8> word{}; CHECK(m.memory.read(rsp,word) && le(word)==2); CHECK(m.memory.read(rsp+8,word)); auto p0=le(word); CHECK(m.memory.read(rsp+16,word)); auto p1=le(word);
   std::array<std::uint8_t,6> a{},b{}; CHECK(m.memory.read(p0,a) && m.memory.read(p1,b)); CHECK((a==std::array<std::uint8_t,6>{'g','u','e','s','t',0})); CHECK((b==std::array<std::uint8_t,6>{'h','e','l','l','o',0}));
   for(unsigned i=3;i<7;++i) CHECK(m.memory.read(rsp+i*8,word) && le(word)==0);
 }
 { Machine m(512); *m.cpu.state.reg(4)=77; const std::array<std::string,1> args{"abc"}; CHECK(m.memory.protect(500,1,Permission::read));
   std::array<std::uint8_t,4> before{}; CHECK(m.memory.read(256,before)); CHECK(!prepare_linux_stack(m,256,256,args)); std::array<std::uint8_t,4> after{};
   CHECK(*m.cpu.state.reg(4)==77 && m.memory.read(256,after) && after==before);
   const std::array<std::string,1> nul{std::string("x\0y",3)}; CHECK(!prepare_linux_stack(m,0,256,nul) && *m.cpu.state.reg(4)==77);
   CHECK(!prepare_linux_stack(m,0,65537,args) && !prepare_linux_stack(m,500,100,args));
 }
 // Each syscall occupies one instruction budget; stdout/stderr are bounded buffers.
 { Machine m(256); const std::array<std::uint8_t,4> code{0x0f,0x05,0x0f,0x05}; const std::array<std::uint8_t,3> text{'a','b','c'}; CHECK(m.memory.write(0,code)&&m.memory.write(128,text));
   LinuxProcess p(m,4); m.cpu.state.rax=1; *m.cpu.state.reg(7)=0x100000001ULL; *m.cpu.state.reg(6)=128; *m.cpu.state.reg(2)=3; m.cpu.state.rflags=0x246;
   auto r=p.run(1); CHECK(r.result==ProcessResult::budget_exhausted && r.attempted==1 && r.stop_rip==2 && m.cpu.state.rax==3 && *m.cpu.state.reg(1)==2 && *m.cpu.state.reg(11)==0x246 && m.cpu.state.rflags==0x246);
   CHECK(p.stdout_bytes().size()==3 && p.stdout_bytes()[0]=='a' && p.stderr_bytes().empty());
   m.cpu.state.rax=60; *m.cpu.state.reg(7)=0x123; r=p.run(1); CHECK(r.result==ProcessResult::exited && r.attempted==1 && r.exit_code==0x23 && r.stop_rip==2 && m.cpu.state.rip==4);
   r=p.run(2); CHECK(r.result==ProcessResult::already_exited && r.attempted==0 && r.exit_code==0x23);
 }
 // Runtime errors are negative Linux numbers; no partial output for invalid memory/limits.
 for(unsigned scenario=0;scenario<7;++scenario) {
   Machine m(256); const std::array<std::uint8_t,2> code{0x0f,0x05}; CHECK(m.memory.write(0,code)); LinuxProcess p(m,3);
   m.cpu.state.rax=scenario==0?999:1; *m.cpu.state.reg(7)=scenario==1?3:scenario==6?2:1; *m.cpu.state.reg(6)=scenario==2?UINT64_MAX:128; *m.cpu.state.reg(2)=scenario==3?4:scenario==4?0:2;
   if(scenario==5) CHECK(m.memory.protect(129,1,Permission::write));
   auto r=p.run(1); CHECK(r.result==ProcessResult::budget_exhausted && r.attempted==1 && m.cpu.state.rip==2);
   const auto expected=scenario==0?std::uint64_t(0)-38:scenario==1?std::uint64_t(0)-9:scenario==2||scenario==5?std::uint64_t(0)-14:scenario==3?std::uint64_t(0)-27:scenario==4?0:2;
   CHECK(m.cpu.state.rax==expected); if(scenario!=6) CHECK(p.stdout_bytes().empty() && p.stderr_bytes().empty()); else CHECK(p.stderr_bytes().size()==2);
 }
 { Machine m(16); const std::array<std::uint8_t,2> code{0x0f,0x05}; CHECK(m.memory.write(0,code)); LinuxProcess p(m); m.cpu.state.rax=231; *m.cpu.state.reg(7)=7;
   auto r=p.run(1); CHECK(r.result==ProcessResult::exited && r.exit_code==7); }
 std::cout<<"Linux test ABI stack, traps, bounded output and exit checks passed\n";
}
