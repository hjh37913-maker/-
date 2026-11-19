#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>
using namespace nightfall;using namespace nightfall::runtime;
std::uint64_t call(LinuxProcess& p,Machine& m,std::uint64_t n,std::uint64_t a,std::uint64_t b=0,std::uint64_t c=0){m.cpu.state.rip=0;m.cpu.state.rax=n;*m.cpu.state.reg(7)=a;*m.cpu.state.reg(6)=b;*m.cpu.state.reg(2)=c;return p.run(1).result==ProcessResult::budget_exhausted?m.cpu.state.rax:UINT64_MAX;}
int main(){std::uint64_t seed=0x7396ad73dd98de31ULL;auto rnd=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};unsigned cases=0;
 for(unsigned scenario=0;scenario<256;++scenario){Machine m(4096);std::array<std::uint8_t,2> code{15,5};std::array<std::uint8_t,3> path{'/','f',0};std::array<std::uint8_t,5> data{1,2,3,4,5};if(!m.memory.write(0,code)||!m.memory.write(64,path))return 1;LinuxProcess p(m);if(!p.add_test_file("/f",data)||call(p,m,2,64,0x80000)!=3)return 1;
  int tmp=memfd_create("nightfall-fcntl",MFD_CLOEXEC);if(tmp<0||dup3(tmp,103,O_CLOEXEC)!=103)return 1;close(tmp);std::array<bool,19> used{};used[3]=true;
  for(unsigned op=0;op<128;++op){unsigned fd=3+rnd()%16,action=rnd()%5;std::uint64_t guest=0;int native=0;
   if(action==0){guest=call(p,m,72,fd,1);if(used[fd]){native=fcntl(100+fd,F_GETFD);if(native<0||guest!=static_cast<unsigned>(native))return 1;}else if(guest!=std::uint64_t{0}-9)return 1;}
   else if(action==1){unsigned flags=rnd()%4;guest=call(p,m,72,fd,2,flags);if(used[fd]){native=fcntl(100+fd,F_SETFD,flags);if(native||guest!=0)return 1;}else if(guest!=std::uint64_t{0}-9)return 1;}
   else if(action==2){unsigned min=3+rnd()%16;bool clo=rnd()&1;guest=call(p,m,72,fd,clo?1030:0,min);if(used[fd]){native=fcntl(100+fd,clo?F_DUPFD_CLOEXEC:F_DUPFD,100+min);if(native<0)return 1;if(native>=119){close(native);if(guest!=std::uint64_t{0}-24)return 1;}else{if(guest!=static_cast<unsigned>(native-100))return 1;used[native-100]=true;}}else if(guest!=std::uint64_t{0}-9)return 1;}
   else if(action==3){unsigned target=3+rnd()%16;bool clo=rnd()&1;guest=call(p,m,292,fd,target,clo?0x80000:0);if(!used[fd]){if(guest!=std::uint64_t{0}-9)return 1;}else if(target==fd){if(guest!=std::uint64_t{0}-22)return 1;}else{if(dup3(100+fd,100+target,clo?O_CLOEXEC:0)!=100+int(target)||guest!=target)return 1;used[target]=true;}}
   else{guest=call(p,m,3,fd);if(used[fd]){if(close(100+fd)||guest)return 1;used[fd]=false;}else if(guest!=std::uint64_t{0}-9)return 1;}
   ++cases;
  }for(unsigned fd=3;fd<19;++fd)if(used[fd])close(100+fd);
 }std::cout<<cases<<" stateful fcntl/dup3/close native private-memfd comparisons passed\n";
}
