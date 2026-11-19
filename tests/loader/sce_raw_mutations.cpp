// Seeded malformed-input and transaction checks, using synthetic data only.
#include "loader/sce_raw_reloc.h"
#include "sce_test_fixture.h"
#include <array>
#include <cstdlib>
#include <iostream>
using namespace nightfall;using namespace nightfall::loader;
int main(int argc,char** argv){std::uint64_t seed=argc>1?std::strtoull(argv[1],nullptr,0):0x826d2f19280a210fULL;if(!seed)seed=1;
 auto rnd=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};const auto original=fixture();constexpr unsigned cases=5000;
 for(unsigned i=0;i<cases;++i){auto b=original;const auto count=1+rnd()%8;
  for(unsigned j=0;j<count;++j){const auto area=rnd()%4;const auto pos=area==0?rnd()%0x180:area==1?0x600+rnd()%192:area==2?0x800+rnd()%0x300:rnd()%b.size();b[pos]^=static_cast<std::uint8_t>(1+rnd()%255);}
  if((rnd()&7)==0)b.resize(rnd()%(b.size()+1));
  Machine m(0x100000,SparsePages{64});if(!m.memory.map(0,4096,Permission::read|Permission::write))return 1;std::array<std::uint8_t,3> marker{0x11,0x43,0x87},out{};if(!m.memory.write(0,marker))return 1;m.cpu.state.rip=31;m.cpu.state.rax=41;
  const std::array<ImportBinding,1> bindings{{{"synthetic#A#B",0x11020}}};
  const auto status=load_sce_raw_relocatable(b,m,0x10000,0xf0000,0x10000,bindings);
  if(status!=ElfStatus::ok){if(!m.memory.read(0,out)||marker!=out||m.cpu.state.rip!=31||m.cpu.state.rax!=41){std::cerr<<"rollback failed case="<<i<<'\n';return 1;}}
  else {std::array<std::uint8_t,1> instruction{};if(!m.memory.fetch(m.cpu.state.rip,instruction)||*m.cpu.state.reg(4)!=0x100000){std::cerr<<"success invariant failed case="<<i<<'\n';return 1;}}
 }
 std::cout<<cases<<" synthetic raw SCE-style mutation rollback/invariant cases passed\n";
}
