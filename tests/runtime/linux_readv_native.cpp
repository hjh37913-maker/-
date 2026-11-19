// Linux-only: native readv oracle uses only a private pipe and synthetic bytes.
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
 std::uint64_t seed=0x123fcaa9081da12ULL; unsigned cases=0;
 for(unsigned i=0;i<8192;++i) {
  std::array<std::uint8_t,48> input{}, host_out{},table{}, guest_out{}; for(auto& b:input) b=std::uint8_t(next(seed));
  host_out.fill(0xa5); guest_out.fill(0xa5); const unsigned input_size=next(seed)%49, count=next(seed)%4, scenario=next(seed)%5;
  std::array<iovec,3> host{}; auto put=[&](unsigned off,std::uint64_t v){ for(unsigned k=0;k<8;++k)table[off+k]=v>>(8*k); };
  for(unsigned j=0;j<3;++j) { auto len=next(seed)%17; bool bad=scenario==2 && j==0; host[j].iov_base=bad?nullptr:host_out.data()+j*16; host[j].iov_len=len;
   put(j*16,bad?UINT64_MAX:300+j*16); put(j*16+8,len); }
  int fd[2]; if(::pipe(fd)) return 1; if(input_size && ::write(fd[1],input.data(),input_size)!=static_cast<ssize_t>(input_size)) return 1; ::close(fd[1]); errno=0;
  auto native=::readv(scenario==1?-1:fd[0],host.data(),count); auto e=errno; ::close(fd[0]);
  Machine m(512); const std::array<std::uint8_t,2> op{0x0f,0x05};
  if(!m.memory.write(0,op)||!m.memory.write(128,table)||!m.memory.write(300,guest_out)) return 1;
  m.cpu.state.rax=19; *m.cpu.state.reg(7)=scenario==1?3:scenario==3?0x100000000ULL:0;
  *m.cpu.state.reg(6)=count?128:UINT64_MAX; *m.cpu.state.reg(2)=count;
  LinuxProcess p(m); if(!p.set_stdin(std::span(input).first(input_size))) return 1;
  auto report=p.run(1); auto expected=native<0?std::uint64_t{0}-static_cast<unsigned>(e):static_cast<std::uint64_t>(native);
  if(report.result!=ProcessResult::budget_exhausted || m.cpu.state.rax!=expected || !m.memory.read(300,guest_out) || guest_out!=host_out) {
    std::cerr<<"readv mismatch i="<<i<<" scenario="<<scenario<<" count="<<count<<" input="<<input_size<<" native="<<native<<" errno="<<e<<" guest="<<m.cpu.state.rax<<'\n'; return 1; }
  ++cases;
 }
 std::cout<<cases<<" native private-pipe readv comparisons passed\n";
}
