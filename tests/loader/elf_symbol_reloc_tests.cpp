#include "loader/static_pie.h"
#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace nightfall;using namespace nightfall::loader;using namespace nightfall::runtime;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(0)
std::uint64_t get(const std::vector<std::uint8_t>& b,std::size_t p,unsigned n){std::uint64_t x=0;for(unsigned i=0;i<n;++i)x|=std::uint64_t(b[p+i])<<(i*8);return x;}
void put(std::vector<std::uint8_t>& b,std::size_t p,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b[p+i]=v>>(8*i);}
int main(int argc,char**argv){CHECK(argc==2);std::ifstream in(argv[1],std::ios::binary);CHECK(in);std::vector<std::uint8_t>b((std::istreambuf_iterator<char>(in)),{});
 auto phoff=get(b,32,8),count=get(b,56,2);std::uint64_t dynamic=0,size=0;
 for(unsigned i=0;i<count;++i){auto p=phoff+i*56;if(get(b,p,4)==2){dynamic=get(b,p+8,8);size=get(b,p+32,8);}}
 CHECK(dynamic&&size);std::uint64_t rela=0,symtab=0,relasz=0;
 for(unsigned i=0;i<size;i+=16){auto p=dynamic+i;switch(get(b,p,8)){case 0:i=size;break;case 6:symtab=get(b,p+8,8);break;case 7:rela=get(b,p+8,8);break;case 8:relasz=get(b,p+8,8);break;}}
 CHECK(rela&&symtab&&relasz==48); // first fixture segment has equal file/virtual addresses
 const auto sym=get(b,rela+8,8)>>32;CHECK(sym && (get(b,rela+8,8)&0xffffffff)==6 && (get(b,rela+32,8)&0xffffffff)==1);
 const auto symbol=symtab+sym*24,target0=get(b,rela,8),target1=get(b,rela+24,8),symval=get(b,symbol+8,8);
 for(std::uint64_t bias:{0x10000ULL,0x30000ULL,0x50000ULL})for(bool sparse:{false,true}){
  Machine m=sparse?Machine(0x100000,SparsePages{256}):Machine(0x100000);CHECK(load_static_pie(b,m,bias,0xf0000,0x10000)==ElfStatus::ok);
  std::array<std::uint8_t,8>x{},y{};CHECK(m.memory.read(bias+target0,x)&&m.memory.read(bias+target1,y));std::uint64_t a=0,c=0;for(unsigned j=0;j<8;++j){a|=std::uint64_t(x[j])<<(8*j);c|=std::uint64_t(y[j])<<(8*j);}CHECK(a==bias+symval&&c==bias+symval);
  CHECK(prepare_linux_stack(m,0xf0000,0x10000,std::array<std::string,1>{"fixture"}));LinuxProcess p(m);auto r=p.run(100);CHECK(r.result==ProcessResult::exited&&r.exit_code==42);
 }
 auto rejected=[&](const std::vector<std::uint8_t>& image,ElfStatus e){Machine m(0x100000);std::array<std::uint8_t,1>a{7},z{};m.memory.write(0,a);m.cpu.state.rip=99;auto r=load_static_pie(image,m,0x10000,0xf0000,0x10000);return r==e&&m.cpu.state.rip==99&&m.memory.read(0,z)&&z==a;};
 {auto x=b;put(x,symbol+6,0,2);CHECK(rejected(x,ElfStatus::unresolved_symbol));} // undefined import
 {auto x=b;put(x,symbol+6,0xfff1,2);CHECK(rejected(x,ElfStatus::unsupported));} // absolute symbol
 {auto x=b;put(x,symbol+4,0x16,1);CHECK(rejected(x,ElfStatus::unsupported));} // TLS symbol
 {auto x=b;put(x,symbol+8,UINT64_MAX,8);CHECK(rejected(x,ElfStatus::malformed));}
 {auto x=b;put(x,rela+8,(4096ULL<<32)|6,8);CHECK(rejected(x,ElfStatus::unsupported));}
 {auto x=b;put(x,rela+8,(2ULL<<32)|8,8);CHECK(rejected(x,ElfStatus::unsupported));}
 {auto x=b;put(x,rela+8,(sym<<32)|7,8);CHECK(rejected(x,ElfStatus::unsupported));} // PLT unsupported
 {auto x=b;put(x,rela+16,17,8);Machine m(0x100000);CHECK(load_static_pie(x,m,0x10000,0xf0000,0x10000)==ElfStatus::ok);std::array<std::uint8_t,8>d{};CHECK(m.memory.read(0x10000+target0,d));std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(d[i])<<(8*i);CHECK(v==0x10000+symval);} // GLOB_DAT ignores A
 {auto x=b;put(x,rela+40,7,8);Machine m(0x100000);CHECK(load_static_pie(x,m,0x10000,0xf0000,0x10000)==ElfStatus::ok);std::array<std::uint8_t,8>d{};CHECK(m.memory.read(0x10000+target1,d));std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(d[i])<<(8*i);CHECK(v==0x10000+symval+7);}
 std::cout<<"defined-symbol GLOB_DAT/R_X86_64_64, rejection and rollback passed\n";
}
