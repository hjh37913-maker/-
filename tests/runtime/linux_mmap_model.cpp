// Deterministic stateful test against a separate page-allocation model.
#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
using namespace nightfall::runtime;
std::uint64_t next(std::uint64_t& x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
std::uint64_t call(LinuxProcess& p,Machine& m,std::uint64_t n,std::uint64_t a,std::uint64_t b,std::uint64_t c=0,std::uint64_t flags=0,std::uint64_t fd=0) {
 m.cpu.state.rip=0;m.cpu.state.rax=n;*m.cpu.state.reg(7)=a;*m.cpu.state.reg(6)=b;*m.cpu.state.reg(2)=c;*m.cpu.state.reg(10)=flags;*m.cpu.state.reg(8)=fd;*m.cpu.state.reg(9)=0;
 return p.run(1).result==ProcessResult::budget_exhausted?m.cpu.state.rax:UINT64_MAX;
}
struct Page {bool owned=false,external=false;unsigned perm=0;std::uint8_t value=0;};
int main() {
 constexpr std::uint64_t base=0x4000,page=4096,nomem=std::uint64_t{0}-12;
 std::uint64_t seed=0x98cbd7512a0fe415ULL;unsigned cases=0;
 for(unsigned scene=0;scene<50;++scene) {
  Machine m(0x10000,SparsePages{16});const std::array<std::uint8_t,2> code{0x0f,0x05};
  if(!m.memory.map(0,page,Permission::read|Permission::write|Permission::execute)||!m.memory.write(0,code))return 1;
  std::array<Page,8> model{};const unsigned foreign=next(seed)%8;
  if(!m.memory.map(base+foreign*page,page,Permission::read|Permission::write))return 1;model[foreign].external=true;model[foreign].perm=3;
  LinuxProcess p(m);if(!p.configure_mmap(base,base+8*page))return 1;
  for(unsigned step=0;step<200;++step) {
   const auto op=next(seed)%3, size=1+next(seed)%3, first=next(seed)%(9-size), perm=next(seed)%8;
   const auto len=(size-1)*page+1;std::uint64_t expected=0,actual=0;
   if(op==0) {
    expected=nomem;
    for(unsigned k=0;k+size<=8;++k) {
     bool available=true;for(unsigned j=0;j<size;++j) if(model[k+j].owned||model[k+j].external)available=false;
     if(available) {expected=base+k*page;for(unsigned j=0;j<size;++j){model[k+j].owned=true;model[k+j].perm=perm;model[k+j].value=0;}break;}
    }
    actual=call(p,m,9,0,len,perm,0x22,UINT64_MAX);
   } else if(op==1) {
    bool valid=true;for(unsigned j=0;j<size;++j) if(!model[first+j].owned)valid=false;
    expected=valid?0:nomem;if(valid)for(unsigned j=0;j<size;++j)model[first+j].perm=perm;
    actual=call(p,m,10,base+first*page,len,perm);
   } else {
    for(unsigned j=0;j<size;++j) if(model[first+j].owned)model[first+j]=Page{};
    actual=call(p,m,11,base+first*page,len);
   }
   if(actual!=expected) {std::cerr<<"mmap model result mismatch scene="<<scene<<" step="<<step<<" op="<<op<<" expected="<<expected<<" actual="<<actual<<'\n';return 1;}
   // Verify permissions and observed first byte for each modeled page.
   for(unsigned k=0;k<8;++k) {
    auto& pg=model[k];const auto addr=base+k*page;const bool mapped=pg.owned||pg.external;
    if(m.memory.can_access(addr,1,Permission::none)!=mapped || m.memory.can_access(addr,1,Permission::read)!=(mapped&&bool(pg.perm&1)) ||
       m.memory.can_access(addr,1,Permission::write)!=(mapped&&bool(pg.perm&2)) || m.memory.can_access(addr,1,Permission::execute)!=(mapped&&bool(pg.perm&4)))return 1;
    if(mapped && (pg.perm&1)) {std::array<std::uint8_t,1> got{};if(!m.memory.read(addr,got)||got[0]!=pg.value)return 1;}
    if(pg.owned && (pg.perm&2) && (next(seed)%13==0)) {pg.value=std::uint8_t(next(seed)); const std::array<std::uint8_t,1> v{pg.value};if(!m.memory.write(addr,v))return 1;}
   }
   ++cases;
  }
 }
 std::cout<<cases<<" stateful mmap/mprotect/munmap operations compared to a separate bounded software model\n";
}
