#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
using namespace nightfall;
using namespace nightfall::runtime;
#define CHECK(x) do {if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(0)
std::uint64_t call(LinuxProcess& p,Machine& m,std::uint64_t n,std::uint64_t a,std::uint64_t b=0,std::uint64_t c=0,std::uint64_t d=0) {
 m.cpu.state.rip=0;m.cpu.state.rax=n;*m.cpu.state.reg(7)=a;*m.cpu.state.reg(6)=b;*m.cpu.state.reg(2)=c;*m.cpu.state.reg(10)=d;
 return p.run(1).result==ProcessResult::budget_exhausted?m.cpu.state.rax:UINT64_MAX;
}
void put64(std::array<std::uint8_t,32>& a,unsigned pos,std::uint64_t v){for(unsigned i=0;i<8;++i)a[pos+i]=v>>(8*i);}
int main() {
 constexpr auto ebad=std::uint64_t{0}-9,efault=std::uint64_t{0}-14,einval=std::uint64_t{0}-22,enoent=std::uint64_t{0}-2,emfile=std::uint64_t{0}-24,espipe=std::uint64_t{0}-29,erofs=std::uint64_t{0}-30,enametoolong=std::uint64_t{0}-36;
 const std::array<std::uint8_t,2> code{0x0f,0x05};const std::array<std::uint8_t,6> content{'a','b','c','d','e','f'};
 const std::string path="/test/message"; std::vector<std::uint8_t> pathbytes(path.begin(),path.end());pathbytes.push_back(0);
 { Machine m(2048);CHECK(m.memory.write(0,code)&&m.memory.write(64,pathbytes));LinuxProcess p(m);
   CHECK(!p.add_test_file("",content)&&!p.add_test_file("relative",content)&&!p.add_test_file(std::string(256,'a'),content)&&!p.add_test_file(std::string("/x\0y",4),content));
   CHECK(p.add_test_file(path,content)&&!p.add_test_file(path,content));
   CHECK(call(p,m,2,64,1)==erofs && call(p,m,2,64,0x40)==einval && call(p,m,257,0,64,0)==ebad);
   CHECK(call(p,m,2,64,0x80000)==3 && call(p,m,257,std::uint64_t{0}-100,64,0)==4);
   CHECK(!p.add_test_file("/later",content));
   CHECK(call(p,m,0,3,512,2)==2 && call(p,m,0,4,520,1)==1);
   std::array<std::uint8_t,3> got{};CHECK(m.memory.read(512,got)&&got[0]=='a'&&got[1]=='b');
   CHECK(call(p,m,17,3,514,2,4)==2 && call(p,m,8,3,0,1)==2);
   CHECK(m.memory.read(514,got)&&got[0]=='e'&&got[1]=='f');
   CHECK(call(p,m,8,3,std::uint64_t{0}-1,2)==5 && call(p,m,0,3,516,4)==1);
   CHECK(call(p,m,8,3,100,0)==100 && call(p,m,0,3,UINT64_MAX,99)==0);
   CHECK(call(p,m,8,3,std::uint64_t{0}-101,1)==einval && call(p,m,8,3,0,1)==100);
   CHECK(call(p,m,8,3,0x7fffffffffffffffULL,0)==0x7fffffffffffffffULL && call(p,m,8,3,1,1)==einval);
   CHECK(call(p,m,17,3,UINT64_MAX,0,UINT64_MAX)==einval && call(p,m,17,0,512,1,0)==espipe);
   CHECK(call(p,m,8,0,0,0)==espipe && call(p,m,8,77,0,0)==ebad);
   CHECK(call(p,m,8,3,0,0)==0 && m.memory.protect(513,1,Permission::read));
   CHECK(call(p,m,0,3,512,3)==efault && call(p,m,8,3,0,1)==0);
   CHECK(m.memory.protect(513,1,Permission::read|Permission::write));
   std::array<std::uint8_t,32> iov{};put64(iov,0,600);put64(iov,8,2);put64(iov,16,602);put64(iov,24,2);CHECK(m.memory.write(128,iov));
   CHECK(call(p,m,19,3,128,2)==4 && call(p,m,8,3,0,1)==4);std::array<std::uint8_t,4> out{};CHECK(m.memory.read(600,out)&&out[0]=='a'&&out[3]=='d');
   CHECK(call(p,m,3,3)==0 && call(p,m,3,3)==ebad && call(p,m,0,3,512,1)==ebad && call(p,m,2,64,0)==3);
   CHECK(call(p,m,3,1)==0 && call(p,m,1,1,512,1)==ebad && call(p,m,2,64,0)==1 && call(p,m,0,1,512,1)==1);
 }
 // Invalid paths are bounded and never forwarded to the host.
 { Machine m(2048);CHECK(m.memory.write(0,code));LinuxProcess p(m);CHECK(p.add_test_file(path,content));
   const std::array<std::uint8_t,8> missing{'/','m','i','s','s','i','n','\0'};CHECK(m.memory.write(64,missing));CHECK(call(p,m,2,64)==enoent && call(p,m,2,UINT64_MAX)==efault);
   std::array<std::uint8_t,256> longpath{};longpath.fill('x');CHECK(m.memory.write(128,longpath));CHECK(call(p,m,2,128)==enametoolong);
   CHECK(m.memory.protect(67,1,Permission::write) && call(p,m,2,64)==efault);
 }
 // All 19 model descriptors are allocated, then reused from the lowest slot.
 { Machine m(2048);CHECK(m.memory.write(0,code)&&m.memory.write(64,pathbytes));LinuxProcess p(m);CHECK(p.add_test_file(path,content));
   for(unsigned fd=3;fd<19;++fd)CHECK(call(p,m,2,64)==fd);
   CHECK(call(p,m,2,64)==emfile && call(p,m,3,7)==0 && call(p,m,2,64)==7);
 }
 // Resource limits apply before guest execution.
 { Machine m(16);LinuxProcess p(m);std::vector<std::uint8_t> big(65537);CHECK(!p.add_test_file("/big",big));
   std::vector<std::uint8_t> full(65536);for(int i=0;i<4;++i)CHECK(p.add_test_file("/"+std::to_string(i),full));CHECK(!p.add_test_file("/over",std::span(full).first(1)));
 }
 std::cout<<"synthetic regular file, descriptor, seek, pread and fault checks passed\n";
}
