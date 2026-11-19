#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
using namespace nightfall::runtime;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x " failed\n"; return 1; } } while(0)
void put(std::array<std::uint8_t,48>& v,unsigned offset,std::uint64_t n) { for(unsigned j=0;j<8;++j) v[offset+j]=n>>(8*j); }
int main() {
 const std::array<std::uint8_t,2> code{0x0f,0x05}; const std::array<std::uint8_t,5> input{'h','e','l','l','o'};
 for(unsigned scenario=0;scenario<9;++scenario) {
  Machine m(512); CHECK(m.memory.write(0,code)); std::array<std::uint8_t,48> iov{};
  put(iov,0,300); put(iov,8,2); put(iov,16,302); put(iov,24,3); put(iov,32,UINT64_MAX); put(iov,40,0);
  if(scenario==2) put(iov,16,UINT64_MAX); // bad second destination
  if(scenario==6) { put(iov,8,0x7fffffffffffffffULL); put(iov,24,1); }
  CHECK(m.memory.write(128,iov)); if(scenario==3) CHECK(m.memory.protect(150,1,Permission::write));
  LinuxProcess p(m,8); CHECK(p.set_stdin(input)); m.cpu.state.rax=19; *m.cpu.state.reg(7)=scenario==1?1:0;
  *m.cpu.state.reg(6)=scenario==7?UINT64_MAX:128; *m.cpu.state.reg(2)=scenario==5?1025:scenario==7?0:3;
  if(scenario==4) CHECK(m.memory.protect(304,1,Permission::read));
  auto r=p.run(1); CHECK(r.result==ProcessResult::budget_exhausted && r.attempted==1 && m.cpu.state.rip==2);
  const auto want=scenario==1?std::uint64_t{0}-9:scenario==2||scenario==3||scenario==4?std::uint64_t{0}-14:
                  scenario==5||scenario==6?std::uint64_t{0}-22:scenario==7?0:5;
  CHECK(m.cpu.state.rax==want); std::array<std::uint8_t,5> out{}; CHECK(m.memory.read(300,out));
  if(scenario==0||scenario==8) CHECK(out==input); else CHECK((out==std::array<std::uint8_t,5>{}));
 }
 // A later invalid vector is ignored when EOF limits the read to earlier vectors.
 { Machine m(512); CHECK(m.memory.write(0,code)); std::array<std::uint8_t,48> iov{}; put(iov,0,300); put(iov,8,2); put(iov,16,UINT64_MAX); put(iov,24,3); CHECK(m.memory.write(128,iov));
   LinuxProcess p(m); CHECK(p.set_stdin(std::span(input).first(2))); m.cpu.state.rax=19; *m.cpu.state.reg(6)=128; *m.cpu.state.reg(2)=2;
   CHECK(p.run(1).result==ProcessResult::budget_exhausted && m.cpu.state.rax==2); std::array<std::uint8_t,2> out{}; CHECK(m.memory.read(300,out)&&out[0]=='h'&&out[1]=='e'); }
 // A failed vector read must leave the input cursor and destinations unchanged.
 { Machine m(512); CHECK(m.memory.write(0,code)); std::array<std::uint8_t,48> iov{};
   put(iov,0,300); put(iov,8,2); put(iov,16,UINT64_MAX); put(iov,24,3); CHECK(m.memory.write(128,iov));
   LinuxProcess p(m); CHECK(p.set_stdin(input)); m.cpu.state.rax=19; *m.cpu.state.reg(6)=128; *m.cpu.state.reg(2)=2;
   CHECK(p.run(1).result==ProcessResult::budget_exhausted && m.cpu.state.rax==std::uint64_t{0}-14);
   m.cpu.state.rip=0; m.cpu.state.rax=0; *m.cpu.state.reg(7)=0; *m.cpu.state.reg(6)=300; *m.cpu.state.reg(2)=5;
   CHECK(p.run(1).result==ProcessResult::budget_exhausted && m.cpu.state.rax==5);
   std::array<std::uint8_t,5> out{}; CHECK(m.memory.read(300,out) && out==input); }
 // Vector table is snapshotted even if a destination aliases its next entry.
 { Machine m(512); CHECK(m.memory.write(0,code)); std::array<std::uint8_t,48> iov{};
   put(iov,0,144); put(iov,8,2); put(iov,16,300); put(iov,24,3); CHECK(m.memory.write(128,iov));
   LinuxProcess p(m); CHECK(p.set_stdin(input)); m.cpu.state.rax=19; *m.cpu.state.reg(6)=128; *m.cpu.state.reg(2)=2;
   CHECK(p.run(1).result==ProcessResult::budget_exhausted && m.cpu.state.rax==5);
   std::array<std::uint8_t,3> out{}; CHECK(m.memory.read(300,out) && (out==std::array<std::uint8_t,3>{'l','l','o'})); }
 std::cout<<"readv guest iovec, short reads and atomic faults passed\n";
}
