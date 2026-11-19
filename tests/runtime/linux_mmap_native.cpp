// Linux-only native oracle uses newly-created private anonymous pages only.
// Guest addresses and file descriptors are never passed to the host OS.
#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
using namespace nightfall;
using namespace nightfall::runtime;
std::uint64_t next(std::uint64_t& x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
std::uint64_t call(LinuxProcess& p,Machine& m,std::uint64_t n,std::uint64_t a,std::uint64_t b,std::uint64_t c=0,std::uint64_t flags=0,std::uint64_t fd=0) {
 m.cpu.state.rip=0;m.cpu.state.rax=n;*m.cpu.state.reg(7)=a;*m.cpu.state.reg(6)=b;*m.cpu.state.reg(2)=c;*m.cpu.state.reg(10)=flags;*m.cpu.state.reg(8)=fd;*m.cpu.state.reg(9)=0;
 return p.run(1).result==ProcessResult::budget_exhausted?m.cpu.state.rax:UINT64_MAX;
}
int main() {
 if(sysconf(_SC_PAGESIZE)!=4096) {std::cout<<"native mmap comparison skipped: host pages not 4096\n";return 0;}
 std::uint64_t seed=0x781230aeaabf9287ULL;unsigned cases=0;
 for(unsigned i=0;i<1024;++i) {
  const std::size_t len=1+next(seed)%12288, rounded=(len+4095)&~std::size_t{4095}, offset=next(seed)%len;
  void* ptr=mmap(nullptr,len,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(ptr==MAP_FAILED)return 1;
  auto* bytes=static_cast<std::uint8_t*>(ptr); if(bytes[offset]!=0)return 1; const auto value=std::uint8_t(next(seed)); bytes[offset]=value;
  Machine m(0x10000,SparsePages{16});const std::array<std::uint8_t,2> code{0x0f,0x05};
  if(!m.memory.map(0,4096,Permission::read|Permission::write|Permission::execute)||!m.memory.write(0,code))return 1;
  LinuxProcess p(m);if(!p.configure_mmap(0x4000,0xc000))return 1;
  const auto guest=call(p,m,9,0,len,3,0x22,UINT64_MAX);if(guest!=0x4000)return 1;
  const std::array<std::uint8_t,1> data{value};std::array<std::uint8_t,1> got{};if(!m.memory.write(guest+offset,data))return 1;
  if(mprotect(ptr,len,PROT_READ)!=0 || call(p,m,10,guest,len,1)!=0 || !m.memory.read(guest+offset,got) || got[0]!=bytes[offset] || m.memory.can_access(guest+offset,1,Permission::write))return 1;
  if(mprotect(ptr,len,PROT_NONE)!=0 || call(p,m,10,guest,len,0)!=0 || m.memory.can_access(guest+offset,1,Permission::read))return 1;
  if(mprotect(ptr,len,PROT_READ)!=0 || call(p,m,10,guest,len,1)!=0 || !m.memory.read(guest+offset,got) || got[0]!=bytes[offset])return 1;
  if(munmap(ptr,len)!=0 || call(p,m,11,guest,len)!=0 || m.memory.can_access(guest,rounded,Permission::none))return 1;
  ptr=mmap(nullptr,len,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(ptr==MAP_FAILED)return 1;bytes=static_cast<std::uint8_t*>(ptr);
  const auto remap=call(p,m,9,0,len,3,0x22,UINT64_MAX);if(remap!=guest || !m.memory.read(remap+offset,got) || got[0]!=bytes[offset] || got[0]!=0)return 1;
  if(munmap(ptr,len)!=0)return 1;++cases;
 }
 std::cout<<cases<<" native private anonymous mmap/mprotect/munmap semantic comparisons passed\n";
}
