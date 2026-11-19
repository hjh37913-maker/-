#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    // Boundary values verify immediate width and sign-extension into eight bytes.
    for (const auto imm : {0u, 1u, 127u, 128u, 255u}) {
        Memory m(128); const std::array<std::uint8_t,2> op{0x6a,static_cast<std::uint8_t>(imm)};
        CHECK(m.write(0,op)); Cpu c; *c.state.reg(4)=96; c.state.rflags=0x8d7;
        CHECK(c.step(m)==StepResult::running && c.state.rip==2 && *c.state.reg(4)==88 && c.state.rflags==0x8d7);
        std::array<std::uint8_t,8> out{}; CHECK(m.read(88,out)); CHECK(out[0]==imm);
        for (unsigned i=1;i<8;++i) CHECK(out[i]==(imm>=128?255:0));
    }
    for (const auto imm : {0u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
        Memory m(128); const std::array<std::uint8_t,5> op{0x68,static_cast<std::uint8_t>(imm),static_cast<std::uint8_t>(imm>>8),static_cast<std::uint8_t>(imm>>16),static_cast<std::uint8_t>(imm>>24)};
        CHECK(m.write(0,op)); Cpu c; *c.state.reg(4)=96;
        CHECK(c.step(m)==StepResult::running && c.state.rip==5 && *c.state.reg(4)==88);
        std::array<std::uint8_t,8> out{}; CHECK(m.read(88,out));
        for (unsigned i=0;i<4;++i) CHECK(out[i]==static_cast<std::uint8_t>(imm>>(8*i)));
        for (unsigned i=4;i<8;++i) CHECK(out[i]==(imm>>31?255:0));
    }
    { Memory m(128); const std::array<std::uint8_t,1> op{0xc9}; const std::array<std::uint8_t,8> value{42,0,0,0,0,0,0,0};
      CHECK(m.write(0,op) && m.write(80,value)); Cpu c; *c.state.reg(4)=120; *c.state.reg(5)=80; c.state.rflags=0x8d7;
      CHECK(c.step(m)==StepResult::running && c.state.rip==1 && *c.state.reg(4)==88 && *c.state.reg(5)==42 && c.state.rflags==0x8d7); }
    { Memory m(128); const std::array<std::uint8_t,3> op{0xc2,0x34,0x12}; const std::array<std::uint8_t,8> value{42,0,0,0,0,0,0,0};
      CHECK(m.write(0,op) && m.write(80,value)); Cpu c; *c.state.reg(4)=80; c.state.rflags=0x8d7;
      CHECK(c.step(m)==StepResult::running && c.state.rip==42 && *c.state.reg(4)==88+0x1234 && c.state.rflags==0x8d7); }
    // Truncated instruction fetch and inaccessible stack leave CPU state and memory unchanged.
    for (const auto op : {std::array<std::uint8_t,5>{0x68,1,2,3,4}, std::array<std::uint8_t,5>{0xc2,1,2,0,0}}) {
      const unsigned len=op[0]==0x68?5:3;
      for(unsigned n=1;n<len;++n) { Memory m(n); CHECK(m.write(0,std::span(op).first(n))); Cpu c; *c.state.reg(4)=100;
        CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && *c.state.reg(4)==100); }
    }
    for (const auto opcode : {0x6a,0xc9,0xc2}) {
      Memory m(64); const std::array<std::uint8_t,3> op{static_cast<std::uint8_t>(opcode),0,0}; CHECK(m.write(0,op)); Cpu c;
      *c.state.reg(4)=64; *c.state.reg(5)=64;
      if(opcode==0x6a) CHECK(m.protect(60,1,Permission::read));
      CHECK(c.step(m)==StepResult::memory_fault && c.state.rip==0 && *c.state.reg(4)==64 && *c.state.reg(5)==64);
      std::array<std::uint8_t,8> out{}; CHECK(m.read(56,out)); CHECK((out==std::array<std::uint8_t,8>{}));
    }
}
