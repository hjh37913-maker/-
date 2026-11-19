#include "cpu/cpu.h"
#include "core/machine.h"
#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(0)
int main() {
 // Dividend 2^64 / 3: quotient 0x5555..., remainder 1.
 { Machine m(128); const std::array<std::uint8_t,3> op{0x48,0xf7,0xf3}; CHECK(m.memory.write(0,op)); m.cpu.state.rax=0; *m.cpu.state.reg(2)=1; *m.cpu.state.reg(3)=3; m.cpu.state.rflags=0x8d7;
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rax==0x5555555555555555ULL && *m.cpu.state.reg(2)==1 && m.cpu.state.rflags==0x8d7); }
 // Signed division truncates towards zero, remainder has dividend's sign.
 { Machine m(128); const std::array<std::uint8_t,3> op{0x48,0xf7,0xfb}; CHECK(m.memory.write(0,op)); m.cpu.state.rax=std::uint64_t{0}-17; *m.cpu.state.reg(2)=~std::uint64_t{0}; *m.cpu.state.reg(3)=5;
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rax==std::uint64_t{0}-3 && *m.cpu.state.reg(2)==std::uint64_t{0}-2); }
 // Divide by zero and overflow leave the entire state unchanged, distinguish from faults.
 for(bool zero:{false,true}) { Machine m(128); const std::array<std::uint8_t,3> op{0x48,0xf7,0xf3}; CHECK(m.memory.write(0,op)); m.cpu.state.rax=5; *m.cpu.state.reg(2)=3; *m.cpu.state.reg(3)=zero?0:3; m.cpu.state.rflags=0x8d7; auto before=m.cpu.state;
   auto report=m.run_detailed(1); CHECK(report.result==RunResult::divide_error && report.attempted==1 && report.stop_rip==0 && report.fault_byte_count==15);
   CHECK(m.cpu.state.rax==before.rax && m.cpu.state.other_registers==before.other_registers && m.cpu.state.rip==before.rip && m.cpu.state.rflags==before.rflags); }
 // Linux runtime reports divide error; no guest signal implementation is claimed.
 { Machine m(128); const std::array<std::uint8_t,2> op{0xf7,0xf3}; CHECK(m.memory.write(0,op)); runtime::LinuxProcess p(m); auto r=p.run(1); CHECK(r.result==runtime::ProcessResult::divide_error && r.attempted==1 && r.stop_rip==0); }
 // Byte division by legacy AH aliases input and output registers.
 { Machine m(128); const std::array<std::uint8_t,2> op{0xf6,0xf4}; CHECK(m.memory.write(0,op)); m.cpu.state.rax=0x100; auto before=m.cpu.state; CHECK(m.cpu.step(m.memory)==StepResult::divide_error && m.cpu.state.rax==before.rax); }
 // Invalid source memory precedes divide-by-zero evaluation and preserves state.
 { Machine m(128); const std::array<std::uint8_t,3> op{0x48,0xf7,0x33}; CHECK(m.memory.write(0,op)); *m.cpu.state.reg(3)=64; m.cpu.state.rax=7; m.cpu.state.rflags=0x8d7; CHECK(m.memory.protect(64,8,Permission::write));
   CHECK(m.cpu.step(m.memory)==StepResult::memory_fault && m.cpu.state.rax==7 && m.cpu.state.rflags==0x8d7 && m.cpu.state.rip==0); }
 // Exhaust all extended divisor register encodings and accumulator aliases.
 for(unsigned width:{16u,32u,64u}) for(unsigned source=0;source<16;++source) {
   Machine m(32); std::array<std::uint8_t,4> op{}; unsigned n=0; if(width==16)op[n++]=0x66;
   op[n++]=std::uint8_t(0x40|(width==64?8:0)|(source>=8?1:0)); op[n++]=0xf7; op[n++]=std::uint8_t(0xf0|(source&7));
   CHECK(m.memory.write(0,std::span(op).first(n))); m.cpu.state.rax=10; *m.cpu.state.reg(2)=0;
   if(source!=0 && source!=2)*m.cpu.state.reg(source)=3;
   const auto before=m.cpu.state; const auto result=m.cpu.step(m.memory);
   if(source==2) CHECK(result==StepResult::divide_error && m.cpu.state.rax==before.rax && m.cpu.state.other_registers==before.other_registers && m.cpu.state.rip==0);
   else CHECK(result==StepResult::running && m.cpu.state.rax==(source==0?1:3) && *m.cpu.state.reg(2)==(source==0?0:1) && m.cpu.state.rip==n);
 }
 // Every legacy byte source and every REX-selected byte source, including AH
 // vs SPL semantics and aliases with the AX dividend.
 for(bool rex:{false,true}) for(unsigned low=0;low<8;++low) for(bool ext:{false,true}) {
   if(!rex && ext) continue;
   Machine m(32); std::array<std::uint8_t,3> op{}; unsigned n=0; if(rex)op[n++]=std::uint8_t(0x40|(ext?1:0)); op[n++]=0xf6; op[n++]=std::uint8_t(0xf0|low);
   CHECK(m.memory.write(0,std::span(op).first(n))); m.cpu.state.rax=10;
   const bool legacy_high=!rex && low>=4; const unsigned index=legacy_high?low-4:low+(ext?8:0);
   if(index!=0) *m.cpu.state.reg(index)=legacy_high?0x300:3;
   const auto divisor=legacy_high?((*m.cpu.state.reg(index)>>8)&0xff):(*m.cpu.state.reg(index)&0xff);
   const auto before=m.cpu.state; const auto result=m.cpu.step(m.memory);
   if(!divisor) CHECK(result==StepResult::divide_error && m.cpu.state.rax==before.rax && m.cpu.state.other_registers==before.other_registers);
   else { const auto ax=before.rax&0xffff, quotient=ax/divisor, remainder=ax%divisor;
     if(quotient>255) CHECK(result==StepResult::divide_error && m.cpu.state.rax==before.rax);
     else CHECK(result==StepResult::running && m.cpu.state.rax==((before.rax&~0xffffULL)|(remainder<<8)|quotient) && m.cpu.state.rip==n); }
 }
 // SIB and RIP-relative memory forms, signed min/-1 and signed zero quotient.
 { Machine m(128); const std::array<std::uint8_t,7> op{0x48,0xf7,0x35,0x39,0,0,0}; const std::array<std::uint8_t,8> value{3,0,0,0,0,0,0,0};
   CHECK(m.memory.write(0,op) && m.memory.write(64,value)); m.cpu.state.rax=10;
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rax==3 && *m.cpu.state.reg(2)==1 && m.cpu.state.rip==7); }
 { Machine m(128); const std::array<std::uint8_t,4> op{0x4b,0xf7,0x3c,0x91}; const std::array<std::uint8_t,8> value{5,0,0,0,0,0,0,0};
   CHECK(m.memory.write(0,op) && m.memory.write(64,value)); m.cpu.state.rax=std::uint64_t{0}-2; *m.cpu.state.reg(2)=~std::uint64_t{0}; *m.cpu.state.reg(9)=48; *m.cpu.state.reg(10)=4;
   CHECK(m.cpu.step(m.memory)==StepResult::running && m.cpu.state.rax==0 && *m.cpu.state.reg(2)==std::uint64_t{0}-2 && m.cpu.state.rip==4); }
 { Machine m(32); const std::array<std::uint8_t,3> op{0x48,0xf7,0xfb}; CHECK(m.memory.write(0,op)); m.cpu.state.rax=0x8000000000000000ULL; *m.cpu.state.reg(2)=~std::uint64_t{0}; *m.cpu.state.reg(3)=~std::uint64_t{0}; const auto before=m.cpu.state;
   CHECK(m.cpu.step(m.memory)==StepResult::divide_error && m.cpu.state.rax==before.rax && m.cpu.state.other_registers==before.other_registers && m.cpu.state.rip==0); }
 // Captured output survives a subsequent modeled divide error, and the
 // instruction budget includes the faulting operation.
 { Machine m(128); const std::array<std::uint8_t,4> op{0x0f,0x05,0xf7,0xf3}; const std::array<std::uint8_t,1> output{'x'};
   CHECK(m.memory.write(0,op) && m.memory.write(64,output)); runtime::LinuxProcess p(m); m.cpu.state.rax=1; *m.cpu.state.reg(7)=1; *m.cpu.state.reg(6)=64; *m.cpu.state.reg(2)=1;
   CHECK(p.run(1).result==runtime::ProcessResult::budget_exhausted && m.cpu.state.rip==2 && p.stdout_bytes().size()==1 && p.stdout_bytes()[0]=='x');
   m.cpu.state.rax=1; *m.cpu.state.reg(2)=0; *m.cpu.state.reg(3)=0; const auto before=m.cpu.state; auto report=p.run(1);
   CHECK(report.result==runtime::ProcessResult::divide_error && report.attempted==1 && report.stop_rip==2 && !p.exited() && p.stdout_bytes().size()==1 && p.stdout_bytes()[0]=='x');
   CHECK(m.cpu.state.rax==before.rax && m.cpu.state.other_registers==before.other_registers && m.cpu.state.rip==before.rip && m.cpu.state.rflags==before.rflags);
 }
 std::cout<<"DIV/IDIV portable quotient, exception and fault tests passed\n";
}
