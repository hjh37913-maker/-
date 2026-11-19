// Deterministic malformed-input mutation stress. Differential parity between
// memory backends is not an independent correctness oracle.
#include "loader/static_pie.h"
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace nightfall;using namespace nightfall::loader;
int main(int argc,char** argv){
 std::uint64_t seed=argc>1?std::strtoull(argv[1],nullptr,0):0x8acf207b521ad319ULL;if(!seed)seed=1;
 auto rnd=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 std::ifstream in(NIGHTFALL_PIE_FIXTURE,std::ios::binary);if(!in)return 1;std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(in)),{});if(image.empty())return 1;
 constexpr unsigned iterations=5000;
 for(unsigned n=0;n<iterations;++n){auto b=image;unsigned mutations=1+rnd()%8;
  for(unsigned j=0;j<mutations;++j){auto index=rnd()%b.size();b[index]^=static_cast<std::uint8_t>(1+(rnd()%255));}
  if((rnd()&7)==0)b.resize(rnd()%(b.size()+1));
  auto bias=(rnd()%16)*4096; constexpr std::uint64_t stack=0xf0000;Machine m(0x100000,SparsePages{64});
  if(!m.memory.map(0,4096,Permission::read|Permission::write))return 1;std::array<std::uint8_t,4> marker{17,23,48,91},after{};if(!m.memory.write(0,marker))return 1;
  m.cpu.state.rip=0x44332211;m.cpu.state.rax=0x77665544;*m.cpu.state.reg(4)=0x88888888;
  auto result=load_static_pie(b,m,bias,stack,0x10000);
  if(result!=ElfStatus::ok){if(!m.memory.read(0,after)||after!=marker||m.cpu.state.rip!=0x44332211||m.cpu.state.rax!=0x77665544||*m.cpu.state.reg(4)!=0x88888888){std::cerr<<"rollback failed case "<<n<<'\n';return 1;}}
  else {std::array<std::uint8_t,1> entry{};if(!m.memory.fetch(m.cpu.state.rip,entry)||*m.cpu.state.reg(4)!=0x100000){std::cerr<<"success invariant failed "<<n<<'\n';return 1;}}
 }
 std::cout<<iterations<<" deterministic mutated ELF loader rollback/invariant cases passed\n";
}
