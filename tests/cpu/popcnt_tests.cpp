#include "cpu/cpu.h"
#include <array>
#include <bit>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(0)
int main(){for(unsigned width:{16u,32u,64u})for(unsigned i=0;i<1024;++i){
 Memory m(128);Cpu cpu;std::array<std::uint8_t,5> code{0xf3,width==16?std::uint8_t(0x66):width==64?std::uint8_t(0x48):std::uint8_t(0x40),0x0f,0xb8,0xc1};CHECK(m.write(0,code));
 const auto value=std::uint64_t(i)*0x9e3779b97f4a7c15ULL;const auto src=width==16?value&0xffff:width==32?value&0xffffffff:value;
 cpu.state.rax=0xfedcba9876543210ULL;*cpu.state.reg(1)=value;cpu.state.rflags=0xcd7;
 CHECK(cpu.step(m)==StepResult::running && cpu.state.rip==5);
 CHECK(cpu.state.rax==(width==16?(0xfedcba9876540000ULL|std::popcount(src)):static_cast<std::uint64_t>(std::popcount(src))));
 CHECK(cpu.state.rflags==((0xcd7&~0x8d5)|(src?0:0x40)));
 }
 {Memory m(128);Cpu c;std::array<std::uint8_t,6> b{0xf3,0x4d,0x0f,0xb8,0x44,0x24};std::array<std::uint8_t,1> disp{8};std::array<std::uint8_t,8> data{0xff,0,0,0,0,0,0,0x80};CHECK(m.write(0,b)&&m.write(6,disp)&&m.write(72,data));*c.state.reg(12)=64;CHECK(c.step(m)==StepResult::running&&*c.state.reg(8)==9&&c.state.rip==7);}
 {Memory m(64);Cpu c;std::array<std::uint8_t,4> b{0xf3,0x0f,0xb8,0x01};CHECK(m.write(0,b));*c.state.reg(1)=63;c.state.rax=7;c.state.rflags=0x8d7;CHECK(c.step(m)==StepResult::memory_fault&&c.state.rip==0&&c.state.rax==7&&c.state.rflags==0x8d7);}
 {Memory m(64);Cpu c;std::array<std::uint8_t,5> b{0x66,0xf3,0x0f,0xb8,0xc1};CHECK(m.write(0,b));*c.state.reg(1)=0xffff;c.state.rax=0x11110000;CHECK(c.step(m)==StepResult::running&&c.state.rax==0x11110010);}
 std::cout<<"POPCNT widths, flags, prefix order and faults passed\n";}
