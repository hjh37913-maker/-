// Independent arithmetic model for synthetic raw SCE-style relocation inputs.
// This does not execute or validate a real PS5 ELF or establish its ABI.
#include "loader/sce_raw_reloc.h"
#include <array>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall;using namespace nightfall::loader;
void put(std::vector<std::uint8_t>&v,std::size_t p,std::uint64_t x,unsigned n){for(unsigned i=0;i<n;++i)v[p+i]=x>>(i*8);}
int main(int argc,char**argv){std::uint64_t seed=argc>1?std::strtoull(argv[1],nullptr,0):0x436345bccdf2821ULL;if(!seed)seed=1;
 auto rnd=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};constexpr unsigned cases=5000;
 for(unsigned n=0;n<cases;++n){std::vector<std::uint8_t> f(0xb00);f[0]=0x7f;f[1]='E';f[2]='L';f[3]='F';f[4]=2;f[5]=1;f[6]=1;f[7]=9;put(f,16,0xfe10,2);put(f,18,62,2);put(f,20,1,4);put(f,24,0x1000,8);put(f,32,64,8);put(f,52,64,2);put(f,54,56,2);put(f,56,4,2);
  auto ph=[&](int i,unsigned type,unsigned flags,unsigned off,unsigned addr,unsigned size){auto p=64+i*56;put(f,p,type,4);put(f,p+4,flags,4);put(f,p+8,off,8);put(f,p+16,addr,8);put(f,p+32,size,8);put(f,p+40,size,8);put(f,p+48,1,8);};
  ph(0,1,5,0x400,0x1000,64);ph(1,1,6,0x500,0x2000,64);ph(2,2,4,0x600,0,144);ph(3,0x61000000,4,0x800,0,0x300);f[0x400]=0xf4;
  std::array<std::pair<std::uint64_t,std::uint64_t>,9> tags{{{0x61000035,0},{0x61000037,32},{0x61000039,0x100},{0x6100003f,48},{0x6100003b,24},{0x6100002f,0x200},{0x61000031,24},{0x61000033,24},{0,0}}};for(unsigned i=0;i<tags.size();++i){put(f,0x600+i*16,tags[i].first,8);put(f,0x608+i*16,tags[i].second,8);}
  f[0x801]='i';f[0x802]='m';f[0x803]='p';put(f,0x918,1,4);f[0x91c]=0x12;
  const bool local=rnd()&1;const unsigned kinds[]{1,6,7,8};const auto type=kinds[rnd()%4];const std::uint64_t bias=(1+rnd()%80)*4096,offset=rnd()%49,addend=rnd(),symbol=0x1000+(rnd()%64),guest=bias+0x1000+(rnd()%64);
  if(local){put(f,0x91e,1,2);put(f,0x920,symbol,8);}
  put(f,0xa00,0x2000+offset,8);put(f,0xa08,(type==8?0:1ULL<<32)|type,8);put(f,0xa10,addend,8);
  const std::array<ImportBinding,1> bindings{{{"imp",guest}}};Machine m(0x100000,SparsePages{64});
  auto status=load_sce_raw_relocatable(f,m,bias,0xf0000,0x10000,bindings);if(status!=ElfStatus::ok){std::cerr<<"case "<<n<<" status "<<elf_status_name(status)<<'\n';return 1;}
  std::array<std::uint8_t,8> bytes{};if(!m.memory.read(bias+0x2000+offset,bytes))return 1;std::uint64_t actual=0;for(unsigned i=0;i<8;++i)actual|=std::uint64_t(bytes[i])<<(i*8);
  const auto reference=type==8?bias+addend:(local?bias+symbol:guest)+(type==1?addend:0);
  if(actual!=reference){std::cerr<<"value mismatch case="<<n<<" type="<<type<<'\n';return 1;}
 }
 std::cout<<cases<<" synthetic SCE-style relocation comparisons against arithmetic model passed\n";
}
