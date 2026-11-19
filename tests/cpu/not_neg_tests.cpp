#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
 for(unsigned prefix=0;prefix<=16;++prefix) for(unsigned ext:{2u,3u}) for(unsigned reg=0;reg<8;++reg) for(auto val:{0ULL,1ULL,0x7fffffffULL,0x80000000ULL,0xffffffffULL,0x7fffffffffffffffULL,0x8000000000000000ULL,0xffffffffffffffffULL}) {
  bool rex=prefix!=0; std::uint8_t p=rex?0x3f+prefix:0; bool wide=(p&8)!=0;
  std::array<std::uint8_t,3> code{}; unsigned n=0; if(rex) code[n++]=p; code[n++]=0xf7; code[n++]=0xc0|(ext<<3)|reg;
  Memory m(8); CHECK(m.write(0,std::span(code).first(n))); Cpu c; unsigned dest=reg+((p&1)?8:0); *c.state.reg(dest)=val; c.state.rflags=0x812;
  auto want=ext==2?~val:0ULL-val; if(!wide) want=std::uint32_t(want);
  CHECK(c.step(m)==StepResult::running && c.state.rip==n && *c.state.reg(dest)==want);
  if(ext==2) CHECK(c.state.rflags==0x812); else CHECK(bool(c.state.rflags&1)==bool(wide?val:std::uint32_t(val)));
  for(unsigned i=1;i<n;++i) { Memory partial(i); Cpu f; CHECK(partial.write(0,std::span(code).first(i))); *f.state.reg(dest)=val; f.state.rflags=0x812;
   CHECK(f.step(partial)==StepResult::memory_fault && f.state.rip==0 && *f.state.reg(dest)==val && f.state.rflags==0x812); }
 }
 for(bool wide:{false,true}) for(unsigned ext:{2u,3u}) {
  std::array<std::uint8_t,7> code{}; unsigned n=0; if(wide) code[n++]=0x48; code[n++]=0xf7; code[n++]=0x05|(ext<<3); unsigned len=n+4; code[n++]=64-len; code[n++]=0; code[n++]=0; code[n++]=0;
  Memory m(128); std::array<std::uint8_t,8> data{3,0,0,0,0,0,0,0}; CHECK(m.write(0,std::span(code).first(n))&&m.write(64,data)); Cpu c; c.state.rflags=0x812;
  CHECK(c.step(m)==StepResult::running && c.state.rip==n); CHECK(m.read(64,data) && data[0]==(ext==2?0xfc:0xfd));
  CHECK(m.protect(64,wide?8:4,Permission::read)); Cpu f; f.state.rflags=0x812; auto before=data;
  CHECK(f.step(m)==StepResult::memory_fault && f.state.rip==0 && f.state.rflags==0x812); CHECK(m.read(64,data) && data==before);
 }
}
