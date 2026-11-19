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
void put(std::vector<std::uint8_t>& b,std::size_t p,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b[p+i]=v>>(i*8);}
int main(int argc,char** argv){CHECK(argc==2);std::ifstream in(argv[1],std::ios::binary);CHECK(in);std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(in)),{});
 const auto phoff=get(b,32,8),count=get(b,56,2);std::uint64_t dyn=0,sz=0;
 for(unsigned i=0;i<count;++i){auto p=phoff+i*56;if(get(b,p,4)==2){dyn=get(b,p+8,8);sz=get(b,p+32,8);}}
 std::uint64_t symtab=0,strtab=0,strsz=0,rela=0;for(unsigned i=0;i<sz;i+=16){const auto p=dyn+i;switch(get(b,p,8)){case 0:i=sz;break;case 5:strtab=get(b,p+8,8);break;case 6:symtab=get(b,p+8,8);break;case 7:rela=get(b,p+8,8);break;case 10:strsz=get(b,p+8,8);break;}}
 CHECK(symtab&&strtab&&strsz&&rela);const auto index=get(b,rela+8,8)>>32;CHECK(index);const auto import=symtab+index*24;std::uint64_t stub=0;
 for(unsigned i=1;i<16;++i){const auto p=symtab+i*24;if(p+24>b.size())break;const auto name=get(b,p,4);if(name>=strsz)continue;std::string_view s(reinterpret_cast<const char*>(b.data()+strtab+name),strsz-name);auto z=s.find('\0');if(z!=s.npos&&s.substr(0,z)=="guest_stub"){stub=get(b,p+8,8);break;}}
 CHECK(stub);
 for(std::uint64_t bias:{0x10000ULL,0x30000ULL,0x50000ULL})for(bool sparse:{false,true}){
  Machine m=sparse?Machine(0x100000,SparsePages{256}):Machine(0x100000);std::array<ImportBinding,1> bind{{{"host_export",bias+stub}}};
  CHECK(load_static_pie(b,m,bias,0xf0000,0x10000,bind)==ElfStatus::ok);
  CHECK(prepare_linux_stack(m,0xf0000,0x10000,std::array<std::string,1>{"fixture"}));LinuxProcess p(m);const auto r=p.run(100);CHECK(r.result==ProcessResult::exited&&r.exit_code==42);
 }
 auto rejected=[&](const std::vector<std::uint8_t>& file,std::span<const ImportBinding> bind,ElfStatus e){Machine m(0x100000);std::array<std::uint8_t,1>a{7},z{};m.memory.write(0,a);m.cpu.state.rip=99;auto r=load_static_pie(file,m,0x10000,0xf0000,0x10000,bind);if(r!=e){std::cerr<<elf_status_name(r)<<" vs "<<elf_status_name(e)<<'\n';return false;}return m.cpu.state.rip==99&&m.memory.read(0,z)&&z==a;};
 std::array<ImportBinding,1> valid{{{"host_export",0x10000+stub}}};std::array<ImportBinding,1> wrong{{{"other",0x10000+stub}}};
 CHECK(rejected(b,{},ElfStatus::unresolved_symbol)&&rejected(b,wrong,ElfStatus::unresolved_symbol));
 {Machine m(0x100000);std::string missing="stale";CHECK(load_static_pie(b,m,0x10000,0xf0000,0x10000,{},&missing)==ElfStatus::unresolved_symbol&&missing=="host_export");CHECK(load_static_pie(b,m,0x10000,0xf0000,0x10000,valid,&missing)==ElfStatus::ok&&missing.empty());}
 {std::array<ImportBinding,2>x{{valid[0],valid[0]}};CHECK(rejected(b,x,ElfStatus::malformed));}
 {std::array<ImportBinding,1>x{{{"host_export",0x100000}}};CHECK(rejected(b,x,ElfStatus::malformed));}
 {std::array<ImportBinding,1>x{{{"",0x10000}}};CHECK(rejected(b,x,ElfStatus::malformed));}
 {auto x=b;put(x,import,strsz,4);CHECK(rejected(x,valid,ElfStatus::malformed));}
 {auto x=b;put(x,import+4,0x06,1);CHECK(rejected(x,valid,ElfStatus::unsupported));}
 {auto x=b;put(x,import+5,2,1);CHECK(rejected(x,valid,ElfStatus::unsupported));}
 {auto x=b;auto n=get(x,import,4);for(std::size_t j=strtab+n;j<strtab+strsz;++j)x[j]='x';CHECK(rejected(x,valid,ElfStatus::malformed));}
 std::cout<<"explicit guest-address import binding, failure and rollback passed\n";
}
