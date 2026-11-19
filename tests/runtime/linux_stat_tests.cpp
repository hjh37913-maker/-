#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace nightfall;
using namespace nightfall::runtime;
#define CHECK(x) do { if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;} }while(0)
std::uint64_t call(LinuxProcess& p,Machine& m,std::uint64_t n,std::uint64_t a,std::uint64_t b=0,std::uint64_t c=0,std::uint64_t d=0) {
 m.cpu.state.rip=0;m.cpu.state.rax=n;*m.cpu.state.reg(7)=a;*m.cpu.state.reg(6)=b;*m.cpu.state.reg(2)=c;*m.cpu.state.reg(10)=d;
 return p.run(1).result==ProcessResult::budget_exhausted?m.cpu.state.rax:UINT64_MAX;
}
std::uint64_t get64(const std::array<std::uint8_t,144>& b,unsigned pos){std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(b[pos+i])<<(8*i);return v;}
int main() {
 constexpr auto ebad=std::uint64_t{0}-9,efault=std::uint64_t{0}-14,einval=std::uint64_t{0}-22,enoent=std::uint64_t{0}-2,nosys=std::uint64_t{0}-38;
 Machine m(4096); const std::array<std::uint8_t,2> code{0x0f,0x05}; const std::array<std::uint8_t,6> path{'/','t','e','s','t',0},missing{'/','n','o','p','e',0};
 const std::array<std::uint8_t,6> contents{1,2,3,4,5,6}; CHECK(m.memory.write(0,code)&&m.memory.write(64,path)&&m.memory.write(80,missing)); LinuxProcess p(m); CHECK(p.add_test_file("/test",contents));
 CHECK(call(p,m,4,80,512)==enoent && call(p,m,4,UINT64_MAX,512)==efault);
 CHECK(call(p,m,5,0,512)==nosys && call(p,m,5,3,512)==ebad);
 CHECK(call(p,m,2,64)==3 && call(p,m,0,3,1024,2)==2);
 CHECK(call(p,m,5,3,512)==0 && call(p,m,8,3,0,1)==2);
 std::array<std::uint8_t,144> actual{},expected{};CHECK(m.memory.read(512,actual));
 expected[8]=1;expected[16]=1;expected[24]=0x24;expected[25]=0x81;expected[48]=6;expected[57]=0x10;expected[64]=1;
 CHECK(actual==expected && get64(actual,48)==contents.size());
 CHECK(call(p,m,4,64,700)==0);std::array<std::uint8_t,144> by_path{};CHECK(m.memory.read(700,by_path)&&by_path==actual);
 CHECK(call(p,m,6,64,700)==0 && call(p,m,262,std::uint64_t{0}-100,64,700,0)==0);
 CHECK(call(p,m,262,0,64,700,0)==ebad && call(p,m,262,std::uint64_t{0}-100,64,700,1)==einval);
 // Whole-structure write failure is atomic; cursor is unchanged.
 std::array<std::uint8_t,144> sentinel{};sentinel.fill(0xa5);CHECK(m.memory.write(1200,sentinel)&&m.memory.protect(1343,1,Permission::read));
 CHECK(call(p,m,5,3,1200)==efault && call(p,m,8,3,0,1)==2);
 std::array<std::uint8_t,144> after{};CHECK(m.memory.read(1200,after)&&after==sentinel);
 CHECK(call(p,m,3,3)==0 && call(p,m,5,3,512)==ebad);
 std::cout<<"synthetic x86-64 stat layout, path lookup and atomic faults passed\n";
}
