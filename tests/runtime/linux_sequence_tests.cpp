// Stateful test against an independent in-memory reference, not a Linux oracle.
#include "runtime/linux_process.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall;
using namespace nightfall::runtime;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 constexpr std::uint64_t base=0x8000,limit=0xc000, data=0x2000; constexpr std::size_t output_limit=8192;
 std::uint64_t rng=0x4521913be7263abcULL; unsigned cases=0;
 for(unsigned scene=0;scene<100;++scene) {
  Machine m(0x20000,SparsePages{10}); const std::array<std::uint8_t,2> syscall{0x0f,0x05};
  if(!m.memory.map(0x1000,4096,Permission::read|Permission::write|Permission::execute) || !m.memory.write(0x1000,syscall) ||
     !m.memory.map(data,4096,Permission::read|Permission::write)) return 1;
  LinuxProcess p(m,output_limit); std::array<std::uint8_t,1024> input{}; for(auto& b:input)b=next(rng); if(!p.set_stdin(input)||!p.configure_brk(base,limit)) return 1;
  std::vector<std::uint8_t> model(4096),out,err; std::size_t cursor=0; std::uint64_t current=base;
  for(unsigned step=0;step<500;++step) {
   const unsigned op=next(rng)%4, count=next(rng)%65, offset=next(rng)%4096; const bool badptr=(next(rng)%7)==0,badfd=(next(rng)%9)==0;
   const bool toerr=(next(rng)&1)!=0; std::uint64_t expected=0,fd=0,ptr=badptr?UINT64_MAX:data+offset;
   if(op==0) {
    fd=badfd?3:0; const auto amount=std::min<std::size_t>(count,input.size()-cursor);
    if(badfd) expected=std::uint64_t{0}-9;
    else if(amount && (badptr || amount>model.size()-offset)) expected=std::uint64_t{0}-14;
    else { expected=amount; std::copy_n(input.begin()+cursor,amount,model.begin()+offset); cursor+=amount; }
   } else if(op==1) {
    fd=badfd?3:(toerr?2:1); auto& output=toerr?err:out;
    if(badfd) expected=std::uint64_t{0}-9;
    else if(!count) expected=0;
    else if(count>output_limit-out.size()-err.size()) expected=std::uint64_t{0}-27;
    else if(badptr || count>model.size()-offset) expected=std::uint64_t{0}-14;
    else { expected=count; output.insert(output.end(),model.begin()+offset,model.begin()+offset+count); }
   } else if(op==2) {
    fd=0; auto request=(next(rng)%10==0)?0:base+(next(rng)%(limit-base+4097));
    if(request>=base && request<=limit) current=request;
    expected=current; ptr=request;
   } else expected=std::uint64_t{0}-38;
   m.cpu.state.rip=0x1000; m.cpu.state.rflags=0x246; m.cpu.state.rax=op==0?0:op==1?1:op==2?12:9999;
   *m.cpu.state.reg(7)=op==2?ptr:fd; *m.cpu.state.reg(6)=ptr; *m.cpu.state.reg(2)=count;
   const auto report=p.run(1);
   if(report.result!=ProcessResult::budget_exhausted || report.attempted!=1 || m.cpu.state.rax!=expected || m.cpu.state.rip!=0x1002 ||
      *m.cpu.state.reg(1)!=0x1002 || *m.cpu.state.reg(11)!=0x246 || m.cpu.state.rflags!=0x246 ||
      !std::equal(out.begin(),out.end(),p.stdout_bytes().begin(),p.stdout_bytes().end()) ||
      !std::equal(err.begin(),err.end(),p.stderr_bytes().begin(),p.stderr_bytes().end())) {
     std::cerr<<"model mismatch scene="<<scene<<" step="<<step<<" op="<<op<<" expected="<<expected<<" got="<<m.cpu.state.rax<<'\n'; return 1;
   }
   if(step%25==0) { std::array<std::uint8_t,4096> actual{}; if(!m.memory.read(data,actual)||!std::equal(model.begin(),model.end(),actual.begin())) return 1; }
   ++cases;
  }
 }
 std::cout<<cases<<" stateful read/write/brk sequences compared with bounded software model\n";
}
