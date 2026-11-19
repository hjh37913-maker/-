// Linux native oracle: private memfd only; never forwards guest names,
// descriptors, guest addresses, or any protected content to the host.
#include "runtime/linux_process.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>
using namespace nightfall;
using namespace nightfall::runtime;
std::uint64_t next(std::uint64_t& x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
std::uint64_t call(LinuxProcess& p,Machine& m,std::uint64_t n,std::uint64_t a,std::uint64_t b=0,std::uint64_t c=0,std::uint64_t d=0) {
 m.cpu.state.rip=0;m.cpu.state.rax=n;*m.cpu.state.reg(7)=a;*m.cpu.state.reg(6)=b;*m.cpu.state.reg(2)=c;*m.cpu.state.reg(10)=d;
 return p.run(1).result==ProcessResult::budget_exhausted?m.cpu.state.rax:UINT64_MAX;
}
void put64(std::array<std::uint8_t,32>& a,unsigned at,std::uint64_t v){for(unsigned i=0;i<8;++i)a[at+i]=v>>(8*i);}
int main() {
 std::uint64_t rng=0x13aa9032f7fe961bULL;unsigned cases=0;
 for(unsigned scene=0;scene<256;++scene) {
  const auto size=next(rng)%65;std::array<std::uint8_t,64> file{};for(auto& v:file)v=next(rng);
  int host=memfd_create("nightfall-original-test",MFD_CLOEXEC);if(host<0)return 1;
  if(::write(host,file.data(),size)!=static_cast<ssize_t>(size)||::lseek(host,0,SEEK_SET)!=0)return 1;
  Machine m(2048);const std::array<std::uint8_t,2> code{0x0f,0x05};const std::array<std::uint8_t,6> path{'/','t','e','s','t',0};
  if(!m.memory.write(0,code)||!m.memory.write(64,path))return 1;LinuxProcess p(m);
  if(!p.add_test_file("/test",std::span(file).first(size))||call(p,m,2,64)!=3)return 1;
  // Compare selected portable facts, not synthetic inode/time/mode metadata.
  struct stat host_stat{}; if(::fstat(host,&host_stat) || !S_ISREG(host_stat.st_mode))return 1;
  if(call(p,m,5,3,1024)!=0)return 1;std::array<std::uint8_t,144> guest_stat{};if(!m.memory.read(1024,guest_stat))return 1;
  std::uint64_t guest_size=0;for(unsigned j=0;j<8;++j)guest_size|=std::uint64_t(guest_stat[48+j])<<(8*j);
  const unsigned guest_mode=unsigned(guest_stat[24])|(unsigned(guest_stat[25])<<8);
  if(guest_size!=static_cast<std::uint64_t>(host_stat.st_size) || (guest_mode&0170000)!=0100000)return 1;
  for(unsigned step=0;step<100;++step) {
   const auto op=next(rng)%4; const auto count=next(rng)%17;const bool bad=next(rng)%9==0;
   std::array<std::uint8_t,32> native_data{},guest_data{};native_data.fill(0xa5);guest_data.fill(0xa5);
   if(!m.memory.write(512,guest_data))return 1;ssize_t result=0;errno=0;std::uint64_t guest=0;
   if(op==0) {result=::read(host,bad?nullptr:native_data.data(),count);const int e=errno;guest=call(p,m,0,3,bad?UINT64_MAX:512,count);if((result<0?std::uint64_t{0}-e:static_cast<std::uint64_t>(result))!=guest) {std::cerr<<"read mismatch scene="<<scene<<" step="<<step<<'\n';return 1;}}
   else if(op==1) {const auto off=next(rng)%96;result=::pread(host,bad?nullptr:native_data.data(),count,static_cast<off_t>(off));const int e=errno;guest=call(p,m,17,3,bad?UINT64_MAX:512,count,off);if((result<0?std::uint64_t{0}-e:static_cast<std::uint64_t>(result))!=guest){std::cerr<<"pread mismatch\n";return 1;}}
   else if(op==2) {const int whence=next(rng)%3;const auto off=static_cast<std::int64_t>(next(rng)%33)-16;const auto r=::lseek(host,off,whence);const int e=errno;guest=call(p,m,8,3,static_cast<std::uint64_t>(off),whence);if((r<0?std::uint64_t{0}-e:static_cast<std::uint64_t>(r))!=guest){std::cerr<<"lseek mismatch\n";return 1;}}
   else {
    const auto len1=next(rng)%9,len2=next(rng)%9;struct iovec native_iov[2]{{native_data.data(),len1},{native_data.data()+8,len2}};
    std::array<std::uint8_t,32> table{};put64(table,0,512);put64(table,8,len1);put64(table,16,520);put64(table,24,len2);if(!m.memory.write(128,table))return 1;
    result=::readv(host,native_iov,2);const int e=errno;guest=call(p,m,19,3,128,2);if((result<0?std::uint64_t{0}-e:static_cast<std::uint64_t>(result))!=guest){std::cerr<<"readv mismatch\n";return 1;}
   }
   if(!m.memory.read(512,guest_data)||guest_data!=native_data){std::cerr<<"data mismatch op="<<op<<" scene="<<scene<<" step="<<step<<'\n';return 1;}
   const auto host_cursor=::lseek(host,0,SEEK_CUR);const auto guest_cursor=call(p,m,8,3,0,1);
   if(host_cursor<0 || guest_cursor!=static_cast<std::uint64_t>(host_cursor)){std::cerr<<"cursor mismatch\n";return 1;}
   ++cases;
  }
  if(::close(host))return 1;
 }
 std::cout<<cases<<" native private-memfd read/pread/lseek/readv stateful comparisons plus 256 selected fstat metadata checks passed\n";
}
