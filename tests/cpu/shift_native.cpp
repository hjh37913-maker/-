#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <cstdlib>
using namespace nightfall;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main(int argc, char** argv) {
 if(argc!=1 && argc!=3) { std::cerr<<"usage: shift_native [iterations-per-form seed]\n"; return 2; }
 std::uint64_t seed=argc==3?std::strtoull(argv[2],nullptr,0):0x849cdef039217aadULL;
 const unsigned iterations=argc==3?std::strtoul(argv[1],nullptr,0):4096;
 if(!seed || !iterations || iterations>10000000) return 2;
 std::size_t cases=0;
 for(bool wide:{false,true}) for(bool memory:{false,true}) for(unsigned ext:{4u,5u,7u}) for(unsigned op:{0xc1u,0xd1u,0xd3u}) for(unsigned i=0;i<iterations;++i) {
  auto input=next(seed), native=input; unsigned raw=op==0xd1?1:i%256, count=raw&(wide?63:31); std::uint64_t flags=0;
#define REF(instruction) asm volatile(instruction "\n\tpushfq\n\tpopq %[f]" : [v] "+&r"(native), [f] "=&r"(flags) : "c"(raw) : "cc","memory")
  if(wide) { if(ext==4) REF("shlq %%cl,%[v]"); else if(ext==5) REF("shrq %%cl,%[v]"); else REF("sarq %%cl,%[v]"); }
  else { if(ext==4) REF("shll %%cl,%k[v]"); else if(ext==5) REF("shrl %%cl,%k[v]"); else REF("sarl %%cl,%k[v]"); }
#undef REF
  std::array<std::uint8_t,11> code{}; unsigned n=0; if(wide) code[n++]=0x48; code[n++]=op;
  if(memory) { unsigned len=(wide?1:0)+2+4+(op==0xc1?1:0); code[n++]=(ext<<3)|5; code[n++]=64-len; code[n++]=0;code[n++]=0;code[n++]=0; }
  else code[n++]=0xc0|(ext<<3); if(op==0xc1) code[n++]=raw;
  std::array<std::uint8_t,8> data{}; for(unsigned j=0;j<8;++j) data[j]=input>>(8*j); Memory mem(128); Cpu cpu;
  if(!mem.write(0,std::span(code).first(n)) || !mem.write(64,data)) return 1; cpu.state.rax=input; *cpu.state.reg(1)=raw; cpu.state.rflags=0x812;
  if(cpu.step(mem)!=StepResult::running || cpu.state.rip!=n) { std::cerr<<"shift decode mismatch\n"; return 1; }
  constexpr std::uint64_t fixed=0xc5, of=0x800; const auto mask=count ? (fixed|(count==1?of:0)) : 0;
  if((count && (cpu.state.rflags&mask)!=(flags&mask)) || (!count && cpu.state.rflags!=0x812) || (cpu.state.rflags&~mask)!=(0x812&~mask)) { std::cerr<<"shift flags mismatch count="<<count<<"\n"; return 1; }
  if(memory) { if(!mem.read(64,data) || cpu.state.rax!=input) return 1; std::uint64_t result=0; for(unsigned j=0;j<8;++j) result|=std::uint64_t(data[j])<<(8*j);
    const auto expected=wide?native:(input&0xffffffff00000000ULL)|native; if(result!=expected) { std::cerr<<"shift memory mismatch\n"; return 1; } }
  else if(cpu.state.rax!=native) { std::cerr<<"shift register mismatch\n"; return 1; }
  ++cases;
 }
 std::cout<<cases<<" shifts compared with native register shifts using equivalent counts\n";
}
