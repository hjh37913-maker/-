#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;using namespace nightfall::runtime;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(0)
std::uint64_t call(LinuxProcess& p,Machine& m,std::uint64_t n,std::uint64_t a,std::uint64_t b=0,std::uint64_t c=0){m.cpu.state.rip=0;m.cpu.state.rax=n;*m.cpu.state.reg(7)=a;*m.cpu.state.reg(6)=b;*m.cpu.state.reg(2)=c;return p.run(1).result==ProcessResult::budget_exhausted?m.cpu.state.rax:UINT64_MAX;}
int main(){constexpr auto ebad=std::uint64_t{0}-9,einval=std::uint64_t{0}-22,emfile=std::uint64_t{0}-24;
 Machine m(4096);std::array<std::uint8_t,2> code{15,5};std::array<std::uint8_t,3> path{'/','f',0};std::array<std::uint8_t,3> bytes{'a','b','c'};CHECK(m.memory.write(0,code)&&m.memory.write(64,path));LinuxProcess p(m);CHECK(p.add_test_file("/f",bytes));
 CHECK(call(p,m,72,3,1)==ebad && call(p,m,2,64,0x80000)==3 && call(p,m,72,3,1)==1);
 CHECK(call(p,m,32,3)==4 && call(p,m,72,4,1)==0 && call(p,m,33,3,3)==3 && call(p,m,72,3,1)==1);
 CHECK(call(p,m,292,3,4,0x80000)==4 && call(p,m,72,4,1)==1 && call(p,m,72,4,2,0xffff)==0 && call(p,m,72,4,1)==1);
 CHECK(call(p,m,72,4,2,2)==0 && call(p,m,72,4,1)==0 && call(p,m,72,4,0,10)==10 && call(p,m,72,4,1030,10)==11 && call(p,m,72,11,1)==1);
 CHECK(call(p,m,0,10,128,2)==2 && call(p,m,0,11,130,1)==1);
 CHECK(call(p,m,72,3,0,19)==einval && call(p,m,72,3,999,0)==einval);
 for(unsigned i=0;i<7;++i)CHECK(call(p,m,72,3,0,12)==12+i);
 CHECK(call(p,m,72,3,0,12)==emfile && call(p,m,3,14)==0 && call(p,m,72,3,1030,12)==14 && call(p,m,72,14,1)==1);
 std::cout<<"fcntl descriptor flags, duplicate minima, shared offsets and limits passed\n";
}
