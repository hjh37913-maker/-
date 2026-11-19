#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
using namespace nightfall::runtime;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": " #x " failed\n"; return 1; } } while(0)
void put(std::array<std::uint8_t,48>& v,unsigned offset,std::uint64_t n) { for(unsigned j=0;j<8;++j) v[offset+j]=n>>(8*j); }
int main() {
 const std::array<std::uint8_t,2> code{0x0f,0x05}; const std::array<std::uint8_t,5> data{'h','e','l','l','o'};
 for(unsigned scenario=0;scenario<9;++scenario) {
  Machine m(512); CHECK(m.memory.write(0,code)&&m.memory.write(300,data)); std::array<std::uint8_t,48> iov{};
  put(iov,0,300); put(iov,8,2); put(iov,16,302); put(iov,24,3); put(iov,32,UINT64_MAX); put(iov,40,0);
  if(scenario==2) put(iov,16,UINT64_MAX); // bad second data pointer
  if(scenario==6) { put(iov,8,0x7fffffffffffffffULL); put(iov,24,1); } // signed size overflow
  CHECK(m.memory.write(128,iov)); if(scenario==3) CHECK(m.memory.protect(150,1,Permission::write));
  LinuxProcess p(m,scenario==4?4:8); m.cpu.state.rax=20; *m.cpu.state.reg(7)=scenario==1?3:scenario==8?2:1;
  *m.cpu.state.reg(6)=scenario==7?UINT64_MAX:128; *m.cpu.state.reg(2)=scenario==5?1025:scenario==7?0:3;
  auto r=p.run(1); CHECK(r.result==ProcessResult::budget_exhausted && r.attempted==1 && m.cpu.state.rip==2);
  const auto want=scenario==1?std::uint64_t{0}-9:scenario==2||scenario==3?std::uint64_t{0}-14:
                  scenario==4?std::uint64_t{0}-27:scenario==5||scenario==6?std::uint64_t{0}-22:scenario==7?0:5;
  CHECK(m.cpu.state.rax==want);
  if(scenario==0) CHECK(p.stdout_bytes().size()==5 && std::equal(data.begin(),data.end(),p.stdout_bytes().begin()));
  else if(scenario==8) CHECK(p.stderr_bytes().size()==5 && std::equal(data.begin(),data.end(),p.stderr_bytes().begin()));
  else CHECK(p.stdout_bytes().empty()&&p.stderr_bytes().empty());
 }
 std::cout<<"writev guest iovec, faults, limits and output tests passed\n";
}
