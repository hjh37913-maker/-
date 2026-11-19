#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
using namespace nightfall::runtime;
#define CHECK(x) do { if (!(x)) { std::cerr<<__LINE__<<": " #x " failed\n"; return 1; } } while(0)
std::uint64_t call(LinuxProcess& process,Machine& m,std::uint64_t request) {
 m.cpu.state.rip=0; m.cpu.state.rax=12; *m.cpu.state.reg(7)=request;
 auto report=process.run(1); if(report.result!=ProcessResult::budget_exhausted) return UINT64_MAX; return m.cpu.state.rax;
}
int main() {
 const std::array<std::uint8_t,2> code{0x0f,0x05}; const std::uint64_t base=0x4000, limit=0x8000;
 { Machine m(0x10000,SparsePages{5}); CHECK(m.memory.map(0,4096,Permission::read|Permission::write|Permission::execute) && m.memory.write(0,code)); LinuxProcess p(m);
   CHECK(!p.configure_brk(3,limit) && !p.configure_brk(base,limit+1) && !p.configure_brk(base,0x200000) && p.configure_brk(base,limit));
   CHECK(!p.configure_brk(base,limit) && call(p,m,0)==base && call(p,m,base+1)==base+1);
   CHECK(m.memory.stats().mapped==8192); std::array<std::uint8_t,1> data{42},out{}; CHECK(m.memory.write(base+4000,data));
   CHECK(call(p,m,base+4097)==base+4097 && m.memory.stats().mapped==12288);
   CHECK(call(p,m,limit+1)==base+4097 && call(p,m,base-1)==base+4097);
   CHECK(call(p,m,base+10)==base+10 && m.memory.stats().mapped==8192 && !m.memory.read(base+4096,out));
   CHECK(m.memory.read(base+4000,out) && out==data); CHECK(call(p,m,base)==base && m.memory.stats().mapped==4096 && !m.memory.read(base,out));
   CHECK(call(p,m,base+1)==base+1 && m.memory.read(base+4000,out) && out[0]==0);
 }
 // Out-of-pages and overlapping mappings return old break with no partial mappings.
 { Machine m(0x10000,SparsePages{1}); CHECK(m.memory.map(0,4096,Permission::read|Permission::write|Permission::execute) && m.memory.write(0,code)); LinuxProcess p(m); CHECK(p.configure_brk(base,limit));
   CHECK(call(p,m,base+1)==base && m.memory.allocated_pages()==1 && m.memory.stats().mapped==4096); }
 { Machine m(0x10000); CHECK(m.memory.write(0,code)); LinuxProcess p(m); CHECK(p.configure_brk(base,limit)); CHECK(call(p,m,base+1)==base); }
 { Machine m(16); CHECK(m.memory.write(0,code)); LinuxProcess p(m); CHECK(call(p,m,0)==std::uint64_t{0}-38); CHECK(!p.configure_brk(0,4096)); }
 std::cout<<"bounded brk growth, shrink, zeroing, overlap and resource behavior passed\n";
}
