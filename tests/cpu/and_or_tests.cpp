#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
 for (bool wide : {false,true}) for (unsigned op : {0x09,0x0b,0x21,0x23}) for (bool memory : {false,true}) {
   const bool to_reg=op==0x0b || op==0x23, is_or=op==0x09 || op==0x0b;
   std::array<std::uint8_t,3> code{}; unsigned len=0; if(wide) code[len++]=0x48; code[len++]=op; code[len++]=memory?0x03:0xc3;
   Memory m(128); CHECK(m.write(0,std::span(code).first(len))); Cpu c;
   const std::uint64_t lhs=0xf0f0f0f0f0f0f0f0ULL, rhs=0x0f0f0f0f0f0f0f0fULL;
   c.state.rax=rhs; *c.state.reg(3)=memory?64:lhs; c.state.rflags=0x812;
   std::array<std::uint8_t,8> data{}; for (unsigned i=0;i<8;++i) data[i]=lhs>>(8*i); CHECK(m.write(64,data));
   const auto result=is_or ? lhs|rhs : lhs&rhs;
   CHECK(c.step(m)==StepResult::running && c.state.rip==len);
   CHECK((c.state.rflags&0x8c5)==(is_or?0x84:0x44)); // PF and SF, or PF and ZF.
   CHECK((c.state.rflags&0x10)==0x10); // AF is undefined; this model preserves it.
   const auto expected=wide?result:std::uint64_t(std::uint32_t(result));
   if(to_reg) CHECK(c.state.rax==expected);
   else if(memory) { CHECK(m.read(64,data)); std::uint64_t got=0; for(unsigned i=0;i<8;++i) got|=std::uint64_t(data[i])<<(8*i); CHECK(got==(wide?result:((lhs&0xffffffff00000000ULL)|expected))); }
   else CHECK(*c.state.reg(3)==expected);
 }
}
