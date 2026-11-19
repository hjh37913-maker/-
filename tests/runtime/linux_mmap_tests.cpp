#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
using namespace nightfall::runtime;
#define CHECK(x) do { if(!(x)) {std::cerr<<__LINE__<<": " #x "\n";return 1;} } while(0)
std::uint64_t call(LinuxProcess& p,Machine& m,std::uint64_t n,std::uint64_t a,std::uint64_t b,std::uint64_t c=0,std::uint64_t flags=0,std::uint64_t fd=0,std::uint64_t off=0) {
 m.cpu.state.rip=0; m.cpu.state.rax=n; *m.cpu.state.reg(7)=a; *m.cpu.state.reg(6)=b; *m.cpu.state.reg(2)=c; *m.cpu.state.reg(10)=flags; *m.cpu.state.reg(8)=fd; *m.cpu.state.reg(9)=off;
 return p.run(1).result==ProcessResult::budget_exhausted?m.cpu.state.rax:UINT64_MAX;
}
int main() {
 constexpr auto base=0x4000ULL,limit=0xc000ULL,page=4096ULL; constexpr auto inval=std::uint64_t{0}-22, nomem=std::uint64_t{0}-12, nosys=std::uint64_t{0}-38;
 const std::array<std::uint8_t,2> code{0x0f,0x05};
 { Machine m(0x10000,SparsePages{9}); CHECK(m.memory.map(0,page,Permission::read|Permission::write|Permission::execute)&&m.memory.write(0,code)); LinuxProcess p(m);
   CHECK(!p.configure_mmap(3,limit)&&!p.configure_mmap(base,limit+1)&&!p.configure_mmap(0,0x500000)&&p.configure_mmap(base,limit)&&!p.configure_mmap(base,limit));
   CHECK(call(p,m,9,0,0,3,0x22,UINT64_MAX)==inval);
   CHECK(call(p,m,9,0,UINT64_MAX,3,0x22,UINT64_MAX)==inval);
   CHECK(call(p,m,9,0,1,8,0x22,UINT64_MAX)==inval);
   CHECK(call(p,m,9,0,1,3,0x22,UINT64_MAX,UINT64_MAX)==inval);
   CHECK(call(p,m,10,UINT64_MAX,1,3)==inval);
   CHECK(call(p,m,11,base,UINT64_MAX)==inval);
   CHECK(call(p,m,9,0,1,3,0x20,UINT64_MAX)==inval);
   CHECK(call(p,m,9,1,1,3,0x22,UINT64_MAX)==inval);
   CHECK(call(p,m,9,0,1,3,0x22,3)==inval);
   CHECK(call(p,m,9,0,1,3,0x22,UINT64_MAX,1)==inval);
   CHECK(call(p,m,9,0,4097,3,0x22,UINT64_MAX)==base);
   CHECK(call(p,m,9,0,1,0,0x22,UINT64_MAX)==base+2*page);
   std::array<std::uint8_t,1> v{42},out{}; CHECK(m.memory.write(base,v));
   CHECK(call(p,m,10,base,1,1)==0 && m.memory.read(base,out) && out==v && !m.memory.write(base,v));
   CHECK(call(p,m,10,base,4097,5)==0 && m.memory.fetch(base,out)&&out==v && !m.memory.write(base,v));
   CHECK(call(p,m,10,base+1,1,3)==inval && call(p,m,10,base+3*page,1,3)==nomem);
   CHECK(call(p,m,11,base+page,1)==0 && !m.memory.read(base+page,out));
   CHECK(call(p,m,11,base+page,1)==0); // already-unmapped hole
   CHECK(call(p,m,9,0,1,3,0x22,UINT64_MAX)==base+page && m.memory.read(base+page,out)&&out[0]==0);
   CHECK(call(p,m,11,base,3*page)==0 && !m.memory.read(base,out));
   CHECK(call(p,m,9,0,1,3,0x22,UINT64_MAX)==base && m.memory.read(base,out)&&out[0]==0);
   CHECK(call(p,m,11,base+1,1)==inval && call(p,m,11,limit,1)==inval && call(p,m,11,base,0)==inval);
   CHECK(!p.configure_mmap(base,limit));
 }
 // Existing non-owned page is skipped by allocation and cannot be unmapped.
 { Machine m(0x10000,SparsePages{5}); CHECK(m.memory.map(0,page,Permission::read|Permission::write|Permission::execute)&&m.memory.write(0,code)); CHECK(m.memory.map(base,page,Permission::read|Permission::write)); LinuxProcess p(m); CHECK(p.configure_mmap(base,limit));
   CHECK(call(p,m,9,0,1,3,0x22,UINT64_MAX)==base+page); CHECK(call(p,m,11,base,2*page)==0 && m.memory.can_access(base,page,Permission::read)); }
 // Sparse allocation cap fails without partial pages.
 { Machine m(0x10000,SparsePages{2}); CHECK(m.memory.map(0,page,Permission::read|Permission::write|Permission::execute)&&m.memory.write(0,code)); LinuxProcess p(m); CHECK(p.configure_mmap(base,limit));
   CHECK(call(p,m,9,0,2*page,3,0x22,UINT64_MAX)==nomem && m.memory.allocated_pages()==1); }
 { Machine m(16);CHECK(m.memory.write(0,code));LinuxProcess p(m);CHECK(call(p,m,9,0,1,3,0x22,UINT64_MAX)==nosys); }
 std::cout<<"bounded anonymous mmap, protection, ownership, zeroing and fault tests passed\n";
}
