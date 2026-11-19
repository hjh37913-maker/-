// Compare limited guest writev with Linux writev into a private pipe.
#include "runtime/linux_process.h"
#include <array>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <sys/uio.h>
#include <unistd.h>
using namespace nightfall;
using namespace nightfall::runtime;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 std::uint64_t seed=0x878811e109afcdeULL; unsigned cases=0; int pipefd[2]; if(::pipe(pipefd)) return 1;
 for(unsigned i=0;i<8192;++i) {
  std::array<std::uint8_t,48> data{}, table{}; for(auto& b:data) b=std::uint8_t(next(seed));
  const unsigned count=next(seed)%4, scenario=next(seed)%5; std::array<iovec,3> host{};
  auto put=[&](unsigned pos,std::uint64_t n){for(unsigned j=0;j<8;++j)table[pos+j]=n>>(8*j);};
  for(unsigned j=0;j<3;++j) { const auto len=next(seed)%17; bool bad=scenario==2 && j==0;
    host[j].iov_base=bad?nullptr:data.data()+j*16; host[j].iov_len=len;
    put(j*16,bad?UINT64_MAX:300+j*16); put(j*16+8,len);
  }
  errno=0; auto native=::writev(scenario==1?-1:pipefd[1],host.data(),count); int error=errno;
  std::array<std::uint8_t,48> received{}; std::size_t got=0;
  while(native>0 && got<static_cast<std::size_t>(native)) { auto n=::read(pipefd[0],received.data()+got,native-got); if(n<=0) return 1; got+=n; }
  Machine m(512); const std::array<std::uint8_t,2> op{0x0f,0x05};
  if(!m.memory.write(0,op)||!m.memory.write(128,table)||!m.memory.write(300,data)) return 1;
  m.cpu.state.rax=20; *m.cpu.state.reg(7)=scenario==1?3:scenario==3?0x100000001ULL:1; *m.cpu.state.reg(6)=128; *m.cpu.state.reg(2)=count;
  LinuxProcess p(m,64); auto report=p.run(1); auto expected=native<0?std::uint64_t{0}-static_cast<unsigned>(error):static_cast<std::uint64_t>(native);
  if(report.result!=ProcessResult::budget_exhausted || m.cpu.state.rax!=expected || p.stdout_bytes().size()!=got) { std::cerr<<"writev mismatch i="<<i<<" native="<<native<<" errno="<<error<<" guest="<<m.cpu.state.rax<<'\n'; return 1; }
  for(unsigned j=0;j<got;++j) if(p.stdout_bytes()[j]!=received[j]) return 1; ++cases;
 }
 ::close(pipefd[0]); ::close(pipefd[1]); std::cout<<cases<<" native private-pipe writev comparisons passed\n";
}
