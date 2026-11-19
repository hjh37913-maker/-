// Linux-only differential check. The native side writes only to a private pipe;
// no guest pointer, descriptor, pathname or syscall is forwarded to the host.
#include "runtime/linux_process.h"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
using namespace nightfall;
using namespace nightfall::runtime;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main(int argc,char** argv) {
 if(argc!=1 && argc!=3) return 2;
 const unsigned iterations=argc==3?std::strtoul(argv[1],nullptr,0):8192;
 std::uint64_t seed=argc==3?std::strtoull(argv[2],nullptr,0):0x19aacc7733dd11eeULL;
 if(!iterations || iterations>1000000 || !seed) return 2;
 int pipefd[2]; if(::pipe(pipefd)!=0) return 1;
 for(unsigned i=0;i<iterations;++i) {
  std::array<std::uint8_t,64> bytes{}; for(auto& b:bytes) b=std::uint8_t(next(seed));
  const unsigned scenario=next(seed)%6, count=scenario==4?0:unsigned(next(seed)%65);
  const bool badfd=scenario==2, badptr=scenario==3 || scenario==4, stderr_channel=scenario==1;
  errno=0; auto native=::write(badfd?-1:pipefd[1],badptr?nullptr:bytes.data(),count); const int error=errno;
  std::array<std::uint8_t,64> actual{}; std::size_t got=0;
  while(native>0 && got<static_cast<std::size_t>(native)) {
   auto n=::read(pipefd[0],actual.data()+got,static_cast<std::size_t>(native)-got);
   if(n<=0) { std::cerr<<"pipe read failed\n"; return 1; } got+=static_cast<std::size_t>(n);
  }
  Machine m(256); const std::array<std::uint8_t,2> code{0x0f,0x05};
  if(!m.memory.write(0,code)||!m.memory.write(128,bytes)) return 1;
  m.cpu.state.rax=1; *m.cpu.state.reg(7)=badfd?3:stderr_channel?2:scenario==5?0x100000001ULL:1;
  *m.cpu.state.reg(6)=badptr?UINT64_MAX:128; *m.cpu.state.reg(2)=count; m.cpu.state.rflags=0x246;
  LinuxProcess proc(m,64); auto r=proc.run(1); const auto expected=native<0?std::uint64_t{0}-static_cast<unsigned>(error):static_cast<std::uint64_t>(native);
  const auto output=stderr_channel?proc.stderr_bytes():proc.stdout_bytes();
  if(r.result!=ProcessResult::budget_exhausted || r.attempted!=1 || m.cpu.state.rax!=expected || m.cpu.state.rip!=2 ||
     *m.cpu.state.reg(1)!=2 || *m.cpu.state.reg(11)!=0x246 || m.cpu.state.rflags!=0x246 || output.size()!=got) {
    std::cerr<<"write mismatch iteration="<<i<<" scenario="<<scenario<<" count="<<count<<" native="<<native<<" errno="<<error<<" guest="<<m.cpu.state.rax<<'\n'; return 1;
  }
  for(std::size_t j=0;j<got;++j) if(output[j]!=actual[j]) { std::cerr<<"byte mismatch\n"; return 1; }
  if((stderr_channel?proc.stdout_bytes():proc.stderr_bytes()).size()!=0) return 1;
 }
 ::close(pipefd[0]); ::close(pipefd[1]);
 std::cout<<iterations<<" private-pipe native write comparisons passed\n";
}
