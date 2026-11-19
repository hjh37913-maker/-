#include "loader/static_pie.h"
#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace nightfall;using namespace nightfall::loader;using namespace nightfall::runtime;
#define CHECK(x) do { if(!(x)){std::cerr<<__LINE__<<": " #x " failed\n";return 1;} }while(0)
std::uint64_t get(const std::vector<std::uint8_t>& b,std::size_t p,unsigned n){std::uint64_t x=0;for(unsigned i=0;i<n;++i)x|=std::uint64_t(b[p+i])<<(8*i);return x;}
void put(std::vector<std::uint8_t>& b,std::size_t p,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b[p+i]=v>>(8*i);}
int main(int argc,char** argv){CHECK(argc==2);std::ifstream in(argv[1],std::ios::binary);CHECK(in);std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),{});
 const auto phoff=get(image,32,8),count=get(image,56,2);std::size_t dynph=0,loadph=0,stackph=0;
 for(unsigned i=0;i<count;++i){const auto p=phoff+i*56;switch(get(image,p,4)){case 1:if(!loadph)loadph=p;break;case 2:dynph=p;break;case 0x6474e551:stackph=p;break;}}
 CHECK(dynph&&loadph&&stackph);const auto dynoff=get(image,dynph+8,8),dynsize=get(image,dynph+32,8);std::size_t rela_tag=0,size_tag=0,ent_tag=0,count_tag=0,null_tag=0;
 for(unsigned i=0;i<dynsize;i+=16){auto p=dynoff+i;auto tag=get(image,p,8);if(tag==7)rela_tag=p;else if(tag==8)size_tag=p;else if(tag==9)ent_tag=p;else if(tag==0x6ffffff9)count_tag=p;else if(!tag){null_tag=p;break;}}
 CHECK(rela_tag&&size_tag&&ent_tag&&count_tag&&null_tag);const auto rela=get(image,rela_tag+8,8); // fixture first LOAD is file-offset == virtual-address
 const auto target=get(image,rela,8),addend=get(image,rela+16,8);CHECK(get(image,rela+8,8)==8);
 for(std::uint64_t bias:{0x10000ULL,0x20000ULL,0x30000ULL})for(bool sparse:{false,true}){
  Machine m=sparse?Machine(0x100000,SparsePages{256}):Machine(0x100000);
  CHECK(load_static_pie(image,m,bias,0xf0000,0x10000)==ElfStatus::ok);
  std::array<std::uint8_t,8> data{};CHECK(m.memory.read(bias+target,data));std::uint64_t value=0;for(unsigned j=0;j<8;++j)value|=std::uint64_t(data[j])<<(8*j);CHECK(value==bias+addend);
  CHECK(!m.memory.write(bias+0x1000,std::span(data).first(1)) && !m.memory.fetch(bias+target,std::span(data).first(1)));
  CHECK(prepare_linux_stack(m,0xf0000,0x10000,std::array<std::string,1>{"fixture"}));LinuxProcess process(m);auto result=process.run(100);
  CHECK(result.result==ProcessResult::exited && result.exit_code==42);
 }
 auto rejected=[&](const std::vector<std::uint8_t>& b,ElfStatus expected,std::uint64_t bias=0x10000,std::uint64_t stack=0xf0000){
  Machine m(0x100000);std::array<std::uint8_t,2> marker{19,37},out{};if(!m.memory.write(0,marker))return false;m.cpu.state.rip=777;m.cpu.state.rax=999;
  auto result=load_static_pie(b,m,bias,stack,0x10000);if(result!=expected){std::cerr<<"status "<<elf_status_name(result)<<" expected "<<elf_status_name(expected)<<'\n';return false;}
  return m.memory.read(0,out)&&out==marker&&m.cpu.state.rip==777&&m.cpu.state.rax==999;
 };
 {auto b=image;put(b,16,2,2);CHECK(rejected(b,ElfStatus::unsupported));}
 {auto b=image;put(b,stackph,3,4);CHECK(rejected(b,ElfStatus::unsupported));}
 {auto b=image;put(b,stackph,7,4);CHECK(rejected(b,ElfStatus::unsupported));}
 {auto b=image;put(b,stackph,0x6474e552,4);CHECK(rejected(b,ElfStatus::unsupported));}
 {auto b=image;put(b,stackph+4,7,4);CHECK(rejected(b,ElfStatus::unsupported));}
 {auto b=image;put(b,dynph,0,4);CHECK(rejected(b,ElfStatus::malformed));}
 {auto b=image;put(b,dynph+8,UINT64_MAX,8);CHECK(rejected(b,ElfStatus::malformed));}
 {auto b=image;put(b,dynph+16,UINT64_MAX,8);CHECK(rejected(b,ElfStatus::malformed));}
 {auto b=image;put(b,rela_tag,1,8);CHECK(rejected(b,ElfStatus::unsupported));} // DT_NEEDED
 {auto b=image;put(b,rela_tag+8,UINT64_MAX,8);CHECK(rejected(b,ElfStatus::malformed));}
 {auto b=image;put(b,size_tag+8,25,8);CHECK(rejected(b,ElfStatus::malformed));}
 {auto b=image;put(b,ent_tag+8,16,8);CHECK(rejected(b,ElfStatus::malformed));}
 {auto b=image;put(b,count_tag+8,2,8);CHECK(rejected(b,ElfStatus::malformed));}
 {auto b=image;put(b,rela+8,6,8);CHECK(rejected(b,ElfStatus::unsupported));} // GLOB_DAT
 {auto b=image;put(b,rela+8,0x100000008ULL,8);CHECK(rejected(b,ElfStatus::unsupported));}
 {auto b=image;put(b,rela,0x1000,8);CHECK(rejected(b,ElfStatus::malformed));} // text relocation
 {auto b=image;put(b,rela,UINT64_MAX,8);CHECK(rejected(b,ElfStatus::malformed));}
 {auto b=image;put(b,24,UINT64_MAX,8);CHECK(rejected(b,ElfStatus::range_error));}
 {auto b=image;put(b,24,0,8);CHECK(rejected(b,ElfStatus::no_executable_entry));}
 CHECK(rejected(image,ElfStatus::malformed,0x10001));
 CHECK(rejected(image,ElfStatus::overlap,0x10000,0x10000));
 {auto b=image;put(b,null_tag,1,8);CHECK(rejected(b,ElfStatus::unsupported));}
 {auto b=image;put(b,loadph+32,UINT64_MAX,8);CHECK(rejected(b,ElfStatus::malformed));}
 {auto b=image;put(b,32,UINT64_MAX,8);CHECK(rejected(b,ElfStatus::malformed));}
 std::cout<<"static PIE relative relocation, biased execution, strict rejection and rollback passed\n";
}
