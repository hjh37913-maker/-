#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do {if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(0)
int main(){
 for(unsigned width:{16u,32u,64u}) for(unsigned op:{0xbcu,0xbdu}) for(unsigned bit=0;bit<width;++bit) {
  Memory m(128); Cpu cpu;std::array<std::uint8_t,4> code{width==16?std::uint8_t(0x66):width==64?std::uint8_t(0x48):std::uint8_t(0x40),0x0f,std::uint8_t(op),0xc1};
  CHECK(m.write(0,code));cpu.state.rax=0xfeedcafeaabbccddULL;*cpu.state.reg(1)=std::uint64_t{1}<<bit;cpu.state.rflags=0x8d7;
  CHECK(cpu.step(m)==StepResult::running && cpu.state.rip==4 && cpu.state.rflags==(0x8d7&~0x40));
  CHECK(cpu.state.rax==(width==16?0xfeedcafeaabb0000ULL|bit:bit));
 }
 { Memory m(128); Cpu cpu; std::array<std::uint8_t,3> code{0x0f,0xbc,0xc1};CHECK(m.write(0,code));cpu.state.rax=0xdeadbeef12345678ULL;cpu.state.rflags=2;CHECK(cpu.step(m)==StepResult::running && cpu.state.rax==0xdeadbeef12345678ULL && cpu.state.rflags==0x42); }
 // extended registers, indexed memory and precise fault
 { Memory m(128); Cpu cpu;std::array<std::uint8_t,5> code{0x4d,0x0f,0xbd,0x44,0x24};CHECK(m.write(0,code));std::array<std::uint8_t,1> disp{8};CHECK(m.write(5,disp));std::array<std::uint8_t,8> data{0,0,0,0,0,0,0,0x80};CHECK(m.write(72,data));*cpu.state.reg(12)=64;CHECK(cpu.step(m)==StepResult::running && *cpu.state.reg(8)==63 && cpu.state.rip==6); }
 { Memory m(128); Cpu cpu;std::array<std::uint8_t,3> code{0x0f,0xbc,0x01};CHECK(m.write(0,code));*cpu.state.reg(1)=127;cpu.state.rax=55;cpu.state.rflags=0x8d7;CHECK(cpu.step(m)==StepResult::memory_fault && cpu.state.rip==0 && cpu.state.rax==55 && cpu.state.rflags==0x8d7); }
 std::cout<<"BSF/BSR width, single-bit, zero, extended memory and fault tests passed\n";
}
