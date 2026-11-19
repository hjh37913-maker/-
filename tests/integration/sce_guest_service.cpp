// End-to-end original synthetic SCE-style file + explicit guest service.
// Linux/SysV test ABI only: not a PS5 system library or a game.
#include "loader/sce_raw_reloc.h"
#include "runtime/guest_services.h"
#include "../loader/sce_test_fixture.h"
#include <array>
#include <iostream>
using namespace nightfall;using namespace nightfall::loader;using namespace nightfall::runtime;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": " #x "\n";return 1;}}while(0)
int main(){auto file=fixture();
 // lea data+32(%rip),rdi; mov pointer(%rip),rax; call *rax; hlt
 const std::array<std::uint8_t,17> code{0x48,0x8d,0x3d,0x19,0x10,0,0,0x48,0x8b,0x05,0xf2,0x0f,0,0,0xff,0xd0,0xf4};
 for(std::size_t i=0;i<code.size();++i)file[0x400+i]=code[i];const std::array<std::uint8_t,6> text{'h','e','l','l','o',0};for(std::size_t i=0;i<text.size();++i)file[0x520+i]=text[i];
 for(std::uint64_t bias:{0x10000ULL,0x30000ULL,0x50000ULL}){
  Machine m(0x100000,SparsePages{64});GuestServices services;CHECK(services.add("synthetic#A#B",0x80000,ServiceKind::string_length));const auto bindings=services.bindings();
  CHECK(load_sce_raw_relocatable(file,m,bias,0xf0000,0x10000,bindings)==ElfStatus::ok);
  const auto result=run_with_services(m,services,100);CHECK(result.result==GuestRunResult::halted&&result.attempted==5&&m.cpu.state.rax==5&&*m.cpu.state.reg(4)==0x100000);
 }
 std::cout<<"synthetic candidate ELF import bound to guest strlen service across three biases\n";
}
