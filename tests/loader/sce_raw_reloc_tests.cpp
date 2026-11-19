// Synthetic ELF assembled entirely in this test; no proprietary data.
#include "loader/sce_raw_reloc.h"
#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall;using namespace nightfall::loader;using namespace nightfall::runtime;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(0)
void put(std::vector<std::uint8_t>&b,std::size_t p,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b[p+i]=v>>(8*i);}
std::vector<std::uint8_t> fixture(){std::vector<std::uint8_t>b(0xc00);b[0]=0x7f;b[1]='E';b[2]='L';b[3]='F';b[4]=2;b[5]=1;b[6]=1;b[7]=9;
 put(b,16,0xfe10,2);put(b,18,62,2);put(b,20,1,4);put(b,24,0x1000,8);put(b,32,64,8);put(b,52,64,2);put(b,54,56,2);put(b,56,4,2);
 auto ph=[&](int i,std::uint32_t t,std::uint32_t flags,std::uint64_t off,std::uint64_t addr,std::uint64_t len){auto p=64+i*56;put(b,p,t,4);put(b,p+4,flags,4);put(b,p+8,off,8);put(b,p+16,addr,8);put(b,p+32,len,8);put(b,p+40,len,8);put(b,p+48,1,8);};
 ph(0,1,5,0x400,0x1000,0x40);ph(1,1,6,0x500,0x2000,0x40);ph(2,2,4,0x600,0,192);ph(3,0x61000000,4,0x800,0,0x400);
 const std::array<std::uint8_t,18> code{0x48,0x8b,0x05,0xf9,0x0f,0,0,0xff,0xd0,0x89,0xc7,0xb8,0x3c,0,0,0,0x0f,0x05};for(std::size_t i=0;i<code.size();++i)b[0x400+i]=code[i];
 const std::array<std::uint8_t,6> stub{0xb8,42,0,0,0,0xc3};for(std::size_t i=0;i<stub.size();++i)b[0x420+i]=stub[i];
 const std::array<std::pair<std::uint64_t,std::uint64_t>,12> tags{{{0x61000035,0},{0x61000037,32},{0x61000039,0x100},{0x6100003f,48},{0x6100003b,24},{0x6100002f,0x200},{0x61000031,48},{0x61000033,24},{0x61000029,0x230},{0x6100002d,24},{0x6100002b,7},{0,0}}};
 for(std::size_t i=0;i<tags.size();++i){put(b,0x600+16*i,tags[i].first,8);put(b,0x608+16*i,tags[i].second,8);}
 const std::string name="synthetic#A#B";for(std::size_t i=0;i<name.size();++i)b[0x801+i]=name[i];put(b,0x918,1,4);b[0x91c]=0x12; // undefined global func
 put(b,0xa00,0x2000,8);put(b,0xa08,(1ULL<<32)|6,8);
 put(b,0xa18,0x2008,8);put(b,0xa20,8,8);put(b,0xa28,0x1020,8);
 put(b,0xa30,0x2010,8);put(b,0xa38,(1ULL<<32)|7,8);return b;
}
int main(){auto b=fixture();
 for(std::uint64_t bias:{0x10000ULL,0x30000ULL,0x50000ULL})for(bool sparse:{false,true}){Machine m=sparse?Machine(0x100000,SparsePages{256}):Machine(0x100000);std::array<ImportBinding,1> bindings{{{"synthetic#A#B",bias+0x1020}}};std::string missing="stale";
  CHECK(load_sce_raw_relocatable(b,m,bias,0xf0000,0x10000,bindings,&missing)==ElfStatus::ok&&missing.empty());
  std::array<std::uint8_t,24> values{};CHECK(m.memory.read(bias+0x2000,values));for(unsigned j=0;j<3;++j){std::uint64_t v=0;for(unsigned k=0;k<8;++k)v|=std::uint64_t(values[j*8+k])<<(k*8);CHECK(v==bias+0x1020);}
  CHECK(!m.memory.write(bias+0x1000,std::span(values).first(1))&&!m.memory.fetch(bias+0x2000,std::span(values).first(1)));
  CHECK(prepare_linux_stack(m,0xf0000,0x10000,std::array<std::string,1>{"test-only"}));LinuxProcess p(m);auto r=p.run(100);CHECK(r.result==ProcessResult::exited&&r.exit_code==42);
 }
 auto rejected=[&](const std::vector<std::uint8_t>& data,std::span<const ImportBinding> bindings,ElfStatus expected){Machine m(0x100000);std::array<std::uint8_t,1>marker{0x5a},out{};m.memory.write(0,marker);m.cpu.state.rip=0x55;m.cpu.state.rax=0x66;std::string missing;
  auto result=load_sce_raw_relocatable(data,m,0x10000,0xf0000,0x10000,bindings,&missing);if(result!=expected){std::cerr<<elf_status_name(result)<<" vs "<<elf_status_name(expected)<<'\n';return false;}
  return m.memory.read(0,out)&&out==marker&&m.cpu.state.rip==0x55&&m.cpu.state.rax==0x66&&(result!=ElfStatus::unresolved_symbol||missing=="synthetic#A#B");};
 std::array<ImportBinding,1> bind{{{"synthetic#A#B",0x11020}}};CHECK(rejected(b,{},ElfStatus::unresolved_symbol));
 {auto x=b;x[0]='S';CHECK(rejected(x,bind,ElfStatus::malformed));} // protected/non-ELF rejected
 {auto x=b;put(x,16,3,2);CHECK(rejected(x,bind,ElfStatus::unsupported));}
 {auto x=b;put(x,18,183,2);CHECK(rejected(x,bind,ElfStatus::unsupported));}
 {auto x=b;put(x,64,0x61000001,4);CHECK(rejected(x,bind,ElfStatus::unsupported));}
 {auto x=b;put(x,0x600,0x6100000f,8);CHECK(rejected(x,bind,ElfStatus::unsupported));} // dependencies not supported
 {auto x=b;put(x,0x610,0x61000035,8);CHECK(rejected(x,bind,ElfStatus::malformed));}
 {auto x=b;put(x,0x608,UINT64_MAX,8);CHECK(rejected(x,bind,ElfStatus::malformed));}
 {auto x=b;put(x,0x618,0x100000,8);CHECK(rejected(x,bind,ElfStatus::resource_limit));}
 {auto x=b;put(x,0xa08,(3ULL<<32)|6,8);CHECK(rejected(x,bind,ElfStatus::unsupported));}
 {auto x=b;put(x,0xa08,(1ULL<<32)|37,8);CHECK(rejected(x,bind,ElfStatus::unsupported));}
 {auto x=b;put(x,0xa00,0x1000,8);CHECK(rejected(x,bind,ElfStatus::malformed));}
 {auto x=b;put(x,0x918,32,4);CHECK(rejected(x,bind,ElfStatus::malformed));}
 {auto x=b;put(x,0x6a8,1,8);CHECK(rejected(x,bind,ElfStatus::malformed));}
 {auto x=b;put(x,0xa18,0x1000,8);CHECK(rejected(x,bind,ElfStatus::malformed));} // failure after staged relocation
 {std::array<ImportBinding,2> duplicate{{bind[0],bind[0]}};CHECK(rejected(b,duplicate,ElfStatus::malformed));}
 std::cout<<"synthetic raw SCE-style RELA/JUMP_SLOT, explicit bindings and rollback passed\n";
}
