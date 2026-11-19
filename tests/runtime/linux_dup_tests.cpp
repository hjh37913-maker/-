#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;using namespace nightfall::runtime;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(0)
std::uint64_t call(LinuxProcess& p,Machine& m,std::uint64_t n,std::uint64_t a,std::uint64_t b=0,std::uint64_t c=0){m.cpu.state.rip=0;m.cpu.state.rax=n;*m.cpu.state.reg(7)=a;*m.cpu.state.reg(6)=b;*m.cpu.state.reg(2)=c;return p.run(1).result==ProcessResult::budget_exhausted?m.cpu.state.rax:UINT64_MAX;}
int main(){
 constexpr auto ebad=std::uint64_t{0}-9,einval=std::uint64_t{0}-22,emfile=std::uint64_t{0}-24;
 Machine m(4096);const std::array<std::uint8_t,2> code{15,5};const std::array<std::uint8_t,3> path{'/','f',0};const std::array<std::uint8_t,6> file{'a','b','c','d','e','f'};
 CHECK(m.memory.write(0,code)&&m.memory.write(64,path));LinuxProcess p(m);CHECK(p.add_test_file("/f",file));
 CHECK(call(p,m,2,64)==3 && call(p,m,32,3)==4 && call(p,m,2,64)==5);
 CHECK(call(p,m,0,3,128,2)==2 && call(p,m,0,4,130,2)==2 && call(p,m,0,5,132,2)==2);
 std::array<std::uint8_t,6> out{};CHECK(m.memory.read(128,out) && (out==std::array<std::uint8_t,6>{'a','b','c','d','a','b'}));
 CHECK(call(p,m,33,3,5)==5 && call(p,m,8,5,0,1)==4 && call(p,m,3,3)==0 && call(p,m,0,5,128,2)==2);
 CHECK(call(p,m,33,4,4)==4 && call(p,m,292,4,4,0)==einval && call(p,m,292,4,6,0x80000)==6);
 CHECK(call(p,m,292,4,7,1)==einval && call(p,m,292,4,7,0x100000000ULL)==7 && call(p,m,3,7)==0 && call(p,m,32,3)==ebad && call(p,m,33,4,19)==ebad);
 CHECK(call(p,m,33,4,1)==1 && call(p,m,0,1,128,1)==0); // replacing stdout with file
 CHECK(call(p,m,33,2,1)==1 && call(p,m,1,1,64,2)==2 && p.stderr_bytes().size()==2);
 for(unsigned fd:{3u,7u,8u,9u,10u,11u,12u,13u,14u,15u,16u,17u,18u})CHECK(call(p,m,32,4)==fd);
 CHECK(call(p,m,32,4)==emfile && call(p,m,3,6)==0 && call(p,m,32,4)==6);
 for(unsigned fd=0;fd<19;++fd)CHECK(call(p,m,3,fd)==0);
 CHECK(call(p,m,2,64)==0 && call(p,m,8,0,0,1)==0); // description reclaimed and reset
 std::cout<<"dup shared offsets, independent opens, replacement, limits, streams and reuse passed\n";
}
