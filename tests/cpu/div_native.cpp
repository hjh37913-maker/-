// Linux x86-64 oracle: successful executions run native DIV/IDIV.
// Invalid input is independently classified with unsigned __int128; separate
// process tests exercise the host exception. Never execute invalid input here.
#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
using U128=unsigned __int128;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 std::uint64_t seed=0xbc8419253a896773ULL; unsigned success=0, error=0;
 constexpr std::array<std::uint64_t,11> edges{0,1,2,0x7f,0x80,0x7fff,0x8000,0x7fffffff,0x80000000,0x7fffffffffffffff,0x8000000000000000};
 for(unsigned width:{8u,16u,32u,64u}) for(bool sign:{false,true}) for(bool memory:{false,true}) for(unsigned i=0;i<16384;++i) {
  auto rawlo=next(seed),rawhi=next(seed),rawdiv=next(seed); if(i<edges.size()*edges.size()){rawlo=edges[i/edges.size()];rawdiv=edges[i%edges.size()];rawhi=i&1?0:~std::uint64_t{0};}
  const auto mask=width==64?~std::uint64_t{0}:(std::uint64_t{1}<<width)-1, signbit=std::uint64_t{1}<<(width-1);
  const auto lo=rawlo&mask, hi=rawhi&mask, divisor=rawdiv&mask;
  U128 num=(U128(hi)<<width)|lo; const U128 wide_mask=width==64?~U128(0):(U128(1)<<(2*width))-1;
  const bool negative=sign && (hi&signbit), div_negative=sign && (divisor&signbit);
  if(negative)num=(~num+1)&wide_mask;
  auto mag=div_negative?((~divisor+1)&mask):divisor;
  bool valid=mag!=0; U128 q=0; if(valid) {q=num/mag; valid=q<=(sign?(negative!=div_negative?U128(signbit):U128(signbit-1)):U128(mask));}
  Memory mem(128); Cpu cpu; std::array<std::uint8_t,4> code{}; unsigned n=0;
  if(width==16)code[n++]=0x66; else if(width==64)code[n++]=0x48;
  code[n++]=width==8?0xf6:0xf7; code[n++]=std::uint8_t((memory?0:0xc0)|(sign?0x38:0x30)|3);
  std::array<std::uint8_t,8> bytes{}; for(unsigned j=0;j<8;++j)bytes[j]=divisor>>(j*8);
  if(!mem.write(0,std::span(code).first(n)) || !mem.write(64,bytes))return 1;
  cpu.state.rax=width==8?(rawlo&~0xffffULL)|(hi<<8)|lo:rawlo;
  *cpu.state.reg(2)=rawhi; *cpu.state.reg(3)=memory?64:rawdiv; cpu.state.rflags=0x8d7;
  auto before=cpu.state; auto step=cpu.step(mem);
  if(!valid) {
   if(step!=StepResult::divide_error || cpu.state.rax!=before.rax || cpu.state.other_registers!=before.other_registers || cpu.state.rip!=0 || cpu.state.rflags!=before.rflags) {
    std::cerr<<"DIV fault mismatch width="<<width<<" signed="<<sign<<" i="<<i<<'\n'; return 1; }
   ++error; continue;
  }
  std::uint64_t native_lo=0,native_hi=0;
  if(width==8) {
   auto ax=static_cast<std::uint16_t>((hi<<8)|lo); auto d=std::uint8_t(divisor);
   if(sign)asm volatile("idivb %[d]" : "+a"(ax) : [d] "m"(d) : "cc");
   else asm volatile("divb %[d]" : "+a"(ax) : [d] "m"(d) : "cc");
   native_lo=(before.rax&~0xffffULL)|ax; native_hi=before.other_registers[1];
  } else if(width==16) {
   auto ax=std::uint16_t(lo), dx=std::uint16_t(hi), d=std::uint16_t(divisor);
   if(sign)asm volatile("idivw %[d]" : "+a"(ax), "+d"(dx) : [d] "m"(d) : "cc");
   else asm volatile("divw %[d]" : "+a"(ax), "+d"(dx) : [d] "m"(d) : "cc");
   native_lo=(before.rax&~0xffffULL)|ax; native_hi=(rawhi&~0xffffULL)|dx;
  } else if(width==32) {
   auto ax=std::uint32_t(lo), dx=std::uint32_t(hi), d=std::uint32_t(divisor);
   if(sign)asm volatile("idivl %[d]" : "+a"(ax), "+d"(dx) : [d] "m"(d) : "cc");
   else asm volatile("divl %[d]" : "+a"(ax), "+d"(dx) : [d] "m"(d) : "cc");
   native_lo=ax; native_hi=dx;
  } else {
   native_lo=lo; native_hi=hi;
   if(sign)asm volatile("idivq %[d]" : "+a"(native_lo), "+d"(native_hi) : [d] "m"(divisor) : "cc");
   else asm volatile("divq %[d]" : "+a"(native_lo), "+d"(native_hi) : [d] "m"(divisor) : "cc");
  }
  if(step!=StepResult::running || cpu.state.rip!=n || cpu.state.rax!=native_lo || *cpu.state.reg(2)!=native_hi || cpu.state.rflags!=0x8d7) {
   std::cerr<<"DIV result mismatch width="<<width<<" signed="<<sign<<" memory="<<memory<<" i="<<i<<" guest="<<cpu.state.rax<<" native="<<native_lo<<" guest_rem="<<*cpu.state.reg(2)<<" native_rem="<<native_hi<<'\n'; return 1;
  }
  ++success;
 }
 std::cout<<success<<" native successful DIV/IDIV comparisons; "<<error<<" error cases classified by independent integer reference\n";
}
