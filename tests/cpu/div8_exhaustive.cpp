// Opt-in exhaustive Linux x86-64 native DIV/IDIV byte oracle.
// A wide integer reference filters faults before native instructions execute.
#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
int main() {
 std::uint64_t passed=0,errors=0;
 Memory mem(16); Cpu cpu; const std::array<std::uint8_t,2> unsigned_op{0xf6,0xf3}, signed_op{0xf6,0xfb};
 for(bool is_signed:{false,true}) {
  if(!mem.write(0,is_signed?signed_op:unsigned_op))return 1;
  for(unsigned ax=0;ax<65536;++ax) for(unsigned divisor=0;divisor<256;++divisor) {
   const int dividend=is_signed && (ax&0x8000)?int(ax)-65536:int(ax);
   const int d=is_signed && (divisor&0x80)?int(divisor)-256:int(divisor);
   bool valid=d!=0; int quotient=0;
   if(valid) { quotient=dividend/d; valid=is_signed?(quotient>=-128&&quotient<=127):(quotient<=255); }
   cpu.state.rax=0x123456789abc0000ULL|ax; *cpu.state.reg(3)=divisor; cpu.state.rip=0; cpu.state.rflags=0x8d7;
   auto step=cpu.step(mem);
   if(!valid) {
    if(step!=StepResult::divide_error || cpu.state.rax!=(0x123456789abc0000ULL|ax) || cpu.state.rip!=0 || cpu.state.rflags!=0x8d7) { std::cerr<<"fault mismatch signed="<<is_signed<<" ax="<<ax<<" divisor="<<divisor<<'\n'; return 1; }
    ++errors; continue;
   }
   auto native_ax=static_cast<std::uint16_t>(ax); const auto native_divisor=static_cast<std::uint8_t>(divisor);
   if(is_signed) asm volatile("idivb %[divisor]" : "+a"(native_ax) : [divisor] "m"(native_divisor) : "cc");
   else asm volatile("divb %[divisor]" : "+a"(native_ax) : [divisor] "m"(native_divisor) : "cc");
   if(step!=StepResult::running || cpu.state.rax!=(0x123456789abc0000ULL|native_ax) || cpu.state.rip!=2 || cpu.state.rflags!=0x8d7) {
    std::cerr<<"result mismatch signed="<<is_signed<<" ax="<<ax<<" divisor="<<divisor<<'\n'; return 1;
   }
   ++passed;
  }
 }
 std::cout<<passed<<" successful byte divisions compared to native; "<<errors<<" fault cases classified by bounded independent integer reference\n";
}
