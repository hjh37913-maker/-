// Linux-only: compare the test ABI to reads from private native pipes.
#include "runtime/linux_process.h"
#include <array>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <unistd.h>
using namespace nightfall;
using namespace nightfall::runtime;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 std::uint64_t seed=0x434def012abcf987ULL; unsigned cases=0;
 for(unsigned i=0;i<8192;++i) {
  std::array<std::uint8_t,64> bytes{}; for(auto& b:bytes) b=std::uint8_t(next(seed));
  const unsigned len=next(seed)%65, scenario=next(seed)%5, count=scenario==4?0:unsigned(next(seed)%97);
  const bool badfd=scenario==2, badptr=scenario==3 || scenario==4;
  int fd[2]; if(::pipe(fd)!=0) return 1;
  if(len && ::write(fd[1],bytes.data(),len)!=static_cast<ssize_t>(len)) return 1;
  ::close(fd[1]); std::array<std::uint8_t,96> actual{}; errno=0;
  const auto native=::read(badfd?-1:fd[0],badptr?nullptr:actual.data(),count); const int error=errno; ::close(fd[0]);
  Machine m(256); const std::array<std::uint8_t,2> code{0x0f,0x05}; if(!m.memory.write(0,code)) return 1;
  LinuxProcess proc(m); if(!proc.set_stdin(std::span(bytes).first(len))) return 1;
  m.cpu.state.rax=0; *m.cpu.state.reg(7)=badfd?3:scenario==1?0x100000000ULL:0;
  *m.cpu.state.reg(6)=badptr?UINT64_MAX:128; *m.cpu.state.reg(2)=count; m.cpu.state.rflags=0x246;
  auto report=proc.run(1); const auto expected=native<0?std::uint64_t{0}-static_cast<unsigned>(error):static_cast<std::uint64_t>(native);
  if(report.result!=ProcessResult::budget_exhausted || m.cpu.state.rax!=expected || m.cpu.state.rip!=2 || *m.cpu.state.reg(1)!=2 || *m.cpu.state.reg(11)!=0x246) {
   std::cerr<<"read mismatch i="<<i<<" scenario="<<scenario<<" len="<<len<<" count="<<count<<" native="<<native<<" errno="<<error<<" guest="<<m.cpu.state.rax<<'\n'; return 1;
  }
  if(native>0) { std::array<std::uint8_t,96> out{}; if(!m.memory.read(128,std::span(out).first(native))) return 1;
   for(int j=0;j<native;++j) if(out[j]!=actual[j]) return 1; }
  ++cases;
 }
 std::cout<<cases<<" private-pipe native read comparisons passed\n";
}
