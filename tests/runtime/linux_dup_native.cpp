#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>
using namespace nightfall;using namespace nightfall::runtime;
std::uint64_t call(LinuxProcess& p,Machine& m,std::uint64_t n,std::uint64_t a,std::uint64_t b=0,std::uint64_t c=0){m.cpu.state.rip=0;m.cpu.state.rax=n;*m.cpu.state.reg(7)=a;*m.cpu.state.reg(6)=b;*m.cpu.state.reg(2)=c;if(p.run(1).result!=ProcessResult::budget_exhausted)return UINT64_MAX;return m.cpu.state.rax;}
int main(int argc,char** argv){std::uint64_t seed=argc>1?std::strtoull(argv[1],nullptr,0):0x5c741f106495aa83ULL;if(!seed)seed=1;unsigned comparisons=0;auto rnd=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 const std::array<std::uint8_t,26> data{'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
 for(unsigned scenario=0;scenario<256;++scenario){
  Machine m(4096);const std::array<std::uint8_t,2> code{15,5};const std::array<std::uint8_t,3> path{'/','f',0};if(!m.memory.write(0,code)||!m.memory.write(64,path))return 1;LinuxProcess p(m);if(!p.add_test_file("/f",data))return 1;
  std::array<bool,19> used{};
  for(unsigned op=0;op<128;++op){
   const auto action=rnd()%5; const unsigned fd=3+rnd()%16;
   if(action==0){
    unsigned expected=3;while(expected<19&&used[expected])++expected;
    auto guest=call(p,m,2,64);
    if(expected==19){if(guest!=std::uint64_t{0}-24)return 1;}
    else {int h=memfd_create("nightfall-dup-oracle",MFD_CLOEXEC);if(h<0||write(h,data.data(),data.size())!=static_cast<ssize_t>(data.size())||lseek(h,0,SEEK_SET)!=0||dup2(h,100+expected)!=100+int(expected))return 1;close(h);used[expected]=true;if(guest!=expected)return 1;}
   }else if(action==1){
    auto guest=call(p,m,3,fd);if(used[fd]){if(close(100+fd)||guest!=0)return 1;used[fd]=false;}else if(guest!=std::uint64_t{0}-9)return 1;
   }else if(action==2){
    unsigned expected=3;while(expected<19&&used[expected])++expected;auto guest=call(p,m,32,fd);
    if(!used[fd]){if(guest!=std::uint64_t{0}-9)return 1;}else if(expected==19){if(guest!=std::uint64_t{0}-24)return 1;}else {if(dup2(100+fd,100+expected)!=100+int(expected)||guest!=expected)return 1;used[expected]=true;}
   }else if(action==3){
    const unsigned target=3+rnd()%16;auto guest=call(p,m,33,fd,target);
    if(!used[fd]){if(guest!=std::uint64_t{0}-9)return 1;}else{if(dup2(100+fd,100+target)!=100+int(target)||guest!=target)return 1;used[target]=true;}
   }else {
    const auto amount=rnd()%12;const bool seek=rnd()&1;
    if(seek){const auto offset=rnd()%40;auto guest=call(p,m,8,fd,offset,0);if(!used[fd]){if(guest!=std::uint64_t{0}-9)return 1;}else if(lseek(100+fd,offset,SEEK_SET)!=static_cast<off_t>(guest))return 1;}
    else {auto guest=call(p,m,0,fd,512,amount);if(!used[fd]){if(guest!=std::uint64_t{0}-9)return 1;}else{std::array<std::uint8_t,12> host{},actual{};auto n=read(100+fd,host.data(),amount);if(n<0||guest!=static_cast<std::uint64_t>(n)||!m.memory.read(512,std::span(actual).first(n))||!std::equal(host.begin(),host.begin()+n,actual.begin()))return 1;}}
   }++comparisons;
  }
  for(unsigned fd=3;fd<19;++fd)if(used[fd])close(100+fd);
 }
 std::cout<<comparisons<<" stateful open/dup/dup2/close/seek/read comparisons with private Linux memfds passed\n";
}
