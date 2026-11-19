#include "cpu/cpu.h"
#include "core/machine.h"
#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x "\n"; return 1; } } while(0)
int main() {
 // MOVDQU supports unaligned addresses and extended XMM registers.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,5> code{0xf3,0x45,0x0f,0x6f,0x01}; std::array<std::uint8_t,16> data{}; for(unsigned i=0;i<16;++i)data[i]=i;
   CHECK(m.write(0,code)&&m.write(65,data)); *c.state.reg(9)=65; c.state.rflags=0x8d7;
   CHECK(c.step(m)==StepResult::running && c.state.xmm[8]==data && c.state.rip==5 && c.state.rflags==0x8d7); }
 // Store faults leave all XMM/register state unchanged.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,4> code{0xf3,0x0f,0x7f,0x03}; CHECK(m.write(0,code)&&m.protect(64,16,Permission::read)); *c.state.reg(3)=64; c.state.xmm[0].fill(5); const auto before=c.state;
   CHECK(c.step(m)==StepResult::memory_fault && c.state.xmm==before.xmm && c.state.other_registers==before.other_registers && c.state.rflags==before.rflags && c.state.rip==0); }
 // MOVD and MOVQ to XMM clear upper bytes, MOVD to a GPR zero extends.
 { Memory m(64); Cpu c; const std::array<std::uint8_t,4> code{0x66,0x0f,0x6e,0xc3}; CHECK(m.write(0,code)); *c.state.reg(3)=0x1122334455667788ULL; c.state.xmm[0].fill(0xff);
   CHECK(c.step(m)==StepResult::running && c.state.xmm[0][0]==0x88 && c.state.xmm[0][3]==0x55 && c.state.xmm[0][4]==0); }
 { Memory m(64); Cpu c; const std::array<std::uint8_t,5> code{0x66,0x48,0x0f,0x6e,0xc3}; CHECK(m.write(0,code)); *c.state.reg(3)=0x1122334455667788ULL; c.state.xmm[0].fill(0xff);
   CHECK(c.step(m)==StepResult::running && c.state.xmm[0][0]==0x88 && c.state.xmm[0][7]==0x11 && c.state.xmm[0][8]==0); }
 { Memory m(64); Cpu c; const std::array<std::uint8_t,4> code{0x66,0x0f,0x7e,0xc3}; CHECK(m.write(0,code)); c.state.xmm[0].fill(0xab); *c.state.reg(3)=~std::uint64_t{0};
   CHECK(c.step(m)==StepResult::running && *c.state.reg(3)==0xababababULL); }
 // PSHUFD memory operand uses instruction length including immediate.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,9> code{0x66,0x0f,0x70,0x05,55,0,0,0,0x1b}; std::array<std::uint8_t,16> data{}; for(unsigned i=0;i<16;++i)data[i]=i;
   CHECK(m.write(0,code)&&m.write(64,data)); CHECK(c.step(m)==StepResult::running && c.state.rip==9);
   for(unsigned i=0;i<16;++i)CHECK(c.state.xmm[0][i]==data[(3-i/4)*4+i%4]); }
 // Aligned moves distinguish misalignment from an ordinary memory fault.
 for(bool vector_int:{false,true}) for(bool store:{false,true}) {
   Machine m(128); Cpu& c=m.cpu; std::array<std::uint8_t,4> code{}; unsigned n=0; if(vector_int)code[n++]=0x66; code[n++]=0x0f; code[n++]=vector_int?(store?0x7f:0x6f):(store?0x29:0x28); code[n++]=0x03;
   CHECK(m.memory.write(0,std::span(code).first(n))); *c.state.reg(3)=65; c.state.xmm[0].fill(0xab); c.state.rflags=0x8d7; const auto before=c.state;
   const auto report=m.run_detailed(1); CHECK(report.result==RunResult::alignment_fault && report.attempted==1 && report.stop_rip==0);
   CHECK(c.state.rax==before.rax && c.state.other_registers==before.other_registers && c.state.xmm==before.xmm && c.state.rflags==before.rflags && c.state.rip==0);
   runtime::LinuxProcess p(m); CHECK(p.run(1).result==runtime::ProcessResult::alignment_fault);
 }
 // Aligned register moves do not impose alignment on data or registers.
 { Memory m(16); Cpu c; const std::array<std::uint8_t,4> op{0x66,0x0f,0x6f,0xc1}; CHECK(m.write(0,op)); c.state.xmm[1].fill(0x42);
   CHECK(c.step(m)==StepResult::running && c.state.xmm[0]==c.state.xmm[1]); }
 // Aliased packed operation and shuffle snapshot all input lanes.
 { Memory m(32); Cpu c; const std::array<std::uint8_t,5> op{0x66,0x45,0x0f,0xef,0xc0}; CHECK(m.write(0,op)); c.state.xmm[8].fill(0xff); c.state.rflags=0x8d7;
   CHECK(c.step(m)==StepResult::running && c.state.rflags==0x8d7 && (c.state.xmm[8]==std::array<std::uint8_t,16>{})); }
 { Memory m(32); Cpu c; const std::array<std::uint8_t,5> op{0x66,0x0f,0x70,0xc0,0x1b}; CHECK(m.write(0,op)); for(unsigned i=0;i<16;++i)c.state.xmm[0][i]=i;
   CHECK(c.step(m)==StepResult::running); for(unsigned i=0;i<16;++i)CHECK(c.state.xmm[0][i]==(3-i/4)*4+i%4); }
 // A fault on the final byte of MOVDQU leaves the whole destination unchanged.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,4> op{0xf3,0x0f,0x6f,0x03}; CHECK(m.write(0,op)&&m.protect(80,1,Permission::write)); *c.state.reg(3)=65; c.state.xmm[0].fill(0x42); const auto before=c.state;
   CHECK(c.step(m)==StepResult::memory_fault && c.state.xmm==before.xmm && c.state.other_registers==before.other_registers && c.state.rip==0); }
 // Unaligned float-bit-pattern MOVUPS is a byte transfer, not floating point.
 { Memory m(128); Cpu c; const std::array<std::uint8_t,3> op{0x0f,0x10,0x03}; std::array<std::uint8_t,16> bytes{};for(unsigned i=0;i<16;++i)bytes[i]=i*13;CHECK(m.write(0,op)&&m.write(65,bytes)); *c.state.reg(3)=65;
   CHECK(c.step(m)==StepResult::running && c.state.xmm[0]==bytes); }
 std::cout<<"SSE2 XMM move, fault, REX and shuffle tests passed\n";
}
