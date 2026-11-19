#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
using V=std::array<std::uint8_t,16>;
std::uint64_t next(std::uint64_t& x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
#define RUN(OP) asm volatile("movdqu %[a],%%xmm0; movdqu %[b],%%xmm1; " OP " %%xmm1,%%xmm0; movdqu %%xmm0,%[out]" : [out] "=m"(out) : [a] "m"(a),[b] "m"(b) : "xmm0","xmm1","memory")
V native(unsigned op,const V& a,const V& b) {
 V out{}; switch(op) { case 0:RUN("pxor");break;case 1:RUN("pand");break;case 2:RUN("por");break;case 3:RUN("pandn");break;
 case 4:RUN("paddb");break;case 5:RUN("paddw");break;case 6:RUN("paddd");break;case 7:RUN("paddq");break;
 case 8:RUN("psubb");break;case 9:RUN("psubw");break;case 10:RUN("psubd");break;case 11:RUN("psubq");break; }
 return out;
}
#undef RUN
int main() {
 constexpr std::array<std::uint8_t,12> ops{0xef,0xdb,0xeb,0xdf,0xfc,0xfd,0xfe,0xd4,0xf8,0xf9,0xfa,0xfb};
 std::uint64_t rng=0xaee1489355bcf728ULL; unsigned cases=0;
 for(unsigned kind=0;kind<ops.size();++kind) for(bool memory:{false,true}) for(unsigned i=0;i<4096;++i) {
  V a{},b{}; for(auto& v:a)v=next(rng); for(auto& v:b)v=next(rng);
  if(i==0){a.fill(0xff);b.fill(1);} if(i==1){a.fill(0);b.fill(0xff);}
  const auto ref=native(kind,a,b); Memory mem(128); Cpu cpu; const std::array<std::uint8_t,4> code{0x66,0x0f,ops[kind],memory?std::uint8_t(0x03):std::uint8_t(0xc1)};
  if(!mem.write(0,code)||!mem.write(65,b))return 1; cpu.state.xmm[0]=a;cpu.state.xmm[1]=b;*cpu.state.reg(3)=65;cpu.state.rflags=0x8d7;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=4 || cpu.state.xmm[0]!=ref || cpu.state.xmm[1]!=b || cpu.state.rflags!=0x8d7) {
   std::cerr<<"SSE2 mismatch op="<<kind<<" memory="<<memory<<" i="<<i<<'\n';return 1;
  }
  ++cases;
 }
 // Native MOVD/MOVQ and unaligned MOVDQU round trips.
 for(bool wide:{false,true}) for(unsigned i=0;i<8192;++i) {
  const auto value=next(rng); V reference{};
  if(wide) asm volatile("movq %[in],%%xmm0; movdqu %%xmm0,%[out]" : [out] "=m"(reference) : [in] "r"(value) : "xmm0","memory");
  else {const auto d=std::uint32_t(value); asm volatile("movd %[in],%%xmm0; movdqu %%xmm0,%[out]" : [out] "=m"(reference) : [in] "r"(d) : "xmm0","memory");}
  Memory mem(128); Cpu cpu; std::array<std::uint8_t,5> code{}; unsigned n=0; code[n++]=0x66;if(wide)code[n++]=0x48;code[n++]=0x0f;code[n++]=0x6e;code[n++]=0xc3;
  if(!mem.write(0,std::span(code).first(n)))return 1; *cpu.state.reg(3)=value; cpu.state.xmm[0].fill(0xff); cpu.state.rflags=0x8d7;
  if(cpu.step(mem)!=StepResult::running || cpu.state.xmm[0]!=reference || cpu.state.rflags!=0x8d7)return 1;
  std::uint64_t native_out=0;
  if(wide)asm volatile("movdqu %[in],%%xmm0; movq %%xmm0,%[out]" : [out] "=r"(native_out) : [in] "m"(reference) : "xmm0");
  else {std::uint32_t d;asm volatile("movdqu %[in],%%xmm0; movd %%xmm0,%[out]" : [out] "=r"(d) : [in] "m"(reference) : "xmm0");native_out=d;}
  code[n-2]=0x7e; if(!mem.write(0,std::span(code).first(n)))return 1;cpu.state.rip=0;*cpu.state.reg(3)=~std::uint64_t{0};
  if(cpu.step(mem)!=StepResult::running || *cpu.state.reg(3)!=native_out || cpu.state.rflags!=0x8d7)return 1;
 }
 for(unsigned i=0;i<8192;++i) {
  std::array<std::uint8_t,48> raw{};for(auto& v:raw)v=next(rng);V ref{}; const auto* pointer=raw.data()+1;
  asm volatile("movdqu (%[in]),%%xmm0; movdqu %%xmm0,%[out]" : [out] "=m"(ref) : [in] "r"(pointer) : "xmm0","memory");
  Memory mem(128);Cpu cpu; const std::array<std::uint8_t,4> load{0xf3,0x0f,0x6f,0x03},store{0xf3,0x0f,0x7f,0x03};
  if(!mem.write(0,load)||!mem.write(64,raw))return 1;*cpu.state.reg(3)=65;
  if(cpu.step(mem)!=StepResult::running||cpu.state.xmm[0]!=ref)return 1;
  if(!mem.write(0,store))return 1;cpu.state.rip=0;*cpu.state.reg(3)=97;
  if(cpu.step(mem)!=StepResult::running)return 1;V got{};if(!mem.read(97,got)||got!=ref)return 1;
 }
 // Aligned MOVDQA/MOVAPS and unaligned MOVUPS bitwise transfers.
 for(unsigned kind=0;kind<3;++kind) for(unsigned i=0;i<4096;++i) {
  alignas(16) V src{};for(auto& v:src)v=next(rng);V ref{};
  if(kind==0)asm volatile("movdqa %[in],%%xmm0; movdqu %%xmm0,%[out]" : [out] "=m"(ref) : [in] "m"(src) : "xmm0","memory");
  else if(kind==1)asm volatile("movaps %[in],%%xmm0; movdqu %%xmm0,%[out]" : [out] "=m"(ref) : [in] "m"(src) : "xmm0","memory");
  else asm volatile("movups %[in],%%xmm0; movdqu %%xmm0,%[out]" : [out] "=m"(ref) : [in] "m"(src) : "xmm0","memory");
  std::array<std::uint8_t,4> code{};unsigned n=0;if(kind==0)code[n++]=0x66;code[n++]=0x0f;code[n++]=kind==0?0x6f:kind==1?0x28:0x10;code[n++]=0x03;
  Memory mem(128);Cpu cpu;const unsigned address=kind==2?65:64;if(!mem.write(0,std::span(code).first(n))||!mem.write(address,src))return 1;*cpu.state.reg(3)=address;
  if(cpu.step(mem)!=StepResult::running||cpu.state.xmm[0]!=ref)return 1;
  code[n-2]=kind==0?0x7f:kind==1?0x29:0x11;if(!mem.write(0,std::span(code).first(n)))return 1;cpu.state.rip=0;*cpu.state.reg(3)=kind==2?97:96;
  if(cpu.step(mem)!=StepResult::running)return 1;V got{};if(!mem.read(kind==2?97:96,got)||got!=ref)return 1;
 }
 // Native PSHUFD with a real fixed immediate; every possible immediate is
 // additionally checked against an independent lane-index reference.
 for(unsigned imm=0;imm<256;++imm) for(unsigned i=0;i<16;++i) {
  V input{},ref{};for(auto& v:input)v=next(rng);
  for(unsigned lane=0;lane<4;++lane)for(unsigned j=0;j<4;++j)ref[lane*4+j]=input[((imm>>(2*lane))&3)*4+j];
  if(imm==0x1b) { V host{}; asm volatile("pshufd $0x1b,%[in],%%xmm0; movdqu %%xmm0,%[out]" : [out] "=m"(host) : [in] "m"(input) : "xmm0","memory"); if(host!=ref)return 1; }
  Memory mem(32); Cpu cpu; const std::array<std::uint8_t,5> code{0x66,0x0f,0x70,0xc1,std::uint8_t(imm)}; if(!mem.write(0,code))return 1; cpu.state.xmm[1]=input;
  if(cpu.step(mem)!=StepResult::running || cpu.state.xmm[0]!=ref || cpu.state.rip!=5)return 1;
 }
 std::cout<<cases<<" native packed integer comparisons; all 256 PSHUFD selectors checked against lane reference (0x1b also native)\n";
}
