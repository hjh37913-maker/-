// Synthetic ELF assembled entirely in this test; no proprietary data.
#include "loader/sce_raw_reloc.h"
#include "runtime/linux_process.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall;using namespace nightfall::loader;using namespace nightfall::runtime;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(0)
#include "sce_test_fixture.h"
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
 {auto x=b;put(x,0x600,0x61000011,8);CHECK(rejected(x,bind,ElfStatus::unsupported));} // unknown attributes not supported
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
 // Explicit module/library identity contracts: no host library is loaded.
 {auto x=b;put(x,64+2*56+32,224,8);put(x,64+2*56+40,224,8);
  put(x,0x6b0,0x6100000f,8);put(x,0x6b8,(1ULL<<48)|(2ULL<<40)|(3ULL<<32)|16,8);
  put(x,0x6c0,0x61000015,8);put(x,0x6c8,(4ULL<<32)|24,8);put(x,0x6d0,0,8);
  const std::string module="module",library="library";for(std::size_t j=0;j<module.size();++j)x[0x810+j]=module[j];for(std::size_t j=0;j<library.size();++j)x[0x818+j]=library[j];
  std::array<SceDependencyBinding,2> deps{{{SceDependencyKind::module,1,2,3,"module"},{SceDependencyKind::library,0,4,0,"library"}}};
  Machine m(0x100000);std::string missing;
  CHECK(load_sce_raw_relocatable(x,m,0x10000,0xf0000,0x10000,bind,&missing)==ElfStatus::unresolved_dependency&&missing=="module");
  CHECK(load_sce_raw_relocatable(x,m,0x10000,0xf0000,0x10000,bind,&missing,deps)==ElfStatus::ok&&missing.empty());
  CHECK(prepare_linux_stack(m,0xf0000,0x10000,std::array<std::string,1>{"synthetic"}));LinuxProcess p(m);CHECK(p.run(100).exit_code==42);
  auto wrong=deps;wrong[0].version_minor=4;Machine n(0x100000);n.cpu.state.rip=99;
  CHECK(load_sce_raw_relocatable(x,n,0x10000,0xf0000,0x10000,bind,&missing,wrong)==ElfStatus::unresolved_dependency&&missing=="module"&&n.cpu.state.rip==99);
  auto bad=x;bad[0x80c]='C';CHECK(load_sce_raw_relocatable(bad,n,0x10000,0xf0000,0x10000,bind,&missing,deps)==ElfStatus::malformed);
  auto dup=x;put(dup,64+2*56+32,240,8);put(dup,64+2*56+40,240,8);put(dup,0x6d0,0x6100000f,8);put(dup,0x6d8,(1ULL<<48)|16,8);put(dup,0x6e0,0,8);
  CHECK(load_sce_raw_relocatable(dup,n,0x10000,0xf0000,0x10000,bind,&missing,deps)==ElfStatus::malformed);
  auto repeated=deps;repeated[1]=deps[0];CHECK(load_sce_raw_relocatable(x,n,0x10000,0xf0000,0x10000,bind,&missing,repeated)==ElfStatus::malformed);
 }
 std::cout<<"synthetic raw SCE-style RELA/JUMP_SLOT, explicit bindings and rollback passed\n";
}
