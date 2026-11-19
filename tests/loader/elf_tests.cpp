#include "loader/elf64.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall;
using namespace nightfall::loader;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
namespace {
void put(std::vector<std::uint8_t>& b, std::size_t pos, std::uint64_t value, unsigned n) {
    for (unsigned i=0;i<n;++i) b[pos+i]=static_cast<std::uint8_t>(value>>(8*i));
}
std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> b(0x117,0);
    b[0]=0x7f; b[1]='E'; b[2]='L'; b[3]='F'; b[4]=2; b[5]=1; b[6]=1;
    put(b,16,2,2); put(b,18,62,2); put(b,20,1,4); put(b,24,0x1000,8); put(b,32,64,8);
    put(b,52,64,2); put(b,54,56,2); put(b,56,1,2);
    put(b,64,1,4); put(b,68,5,4); put(b,72,0x100,8); put(b,80,0x1000,8);
    put(b,96,23,8); put(b,104,32,8); put(b,112,1,8);
    constexpr std::array<std::uint8_t,23> code{0x48,0xb8,40,0,0,0,0,0,0,0,
        0xe8,1,0,0,0,0xf4,0x48,0x05,2,0,0,0,0xc3};
    for (std::size_t i=0;i<code.size();++i) b[0x100+i]=code[i];
    return b;
}
}
int main() {
    constexpr std::uint64_t stack=0xf000; constexpr std::size_t stack_size=0x1000;
    { auto b=fixture(); Machine m(0x10000); m.cpu.state.rax=999;
      CHECK(load_elf64(b,m,stack,stack_size)==ElfStatus::ok);
      CHECK(m.cpu.state.rip==0x1000 && m.cpu.state.rax==0 && *m.cpu.state.reg(4)==0x10000);
      std::array<std::uint8_t,9> zero{}; CHECK((m.memory.read(0x1017,zero) && zero==std::array<std::uint8_t,9>{}));
      CHECK(!m.memory.write(0x1000,std::span<const std::uint8_t>(zero).first(1)));
      CHECK(!m.memory.fetch(stack,std::span(zero).first(1)));
      CHECK(m.run(5)==RunResult::halted && m.cpu.state.rax==42 && *m.cpu.state.reg(4)==0x10000); }
    // Every failure must leave the original memory and CPU state unchanged.
    auto rejected=[&](std::vector<std::uint8_t> b,ElfStatus expected,std::uint64_t sb=0xf000,std::size_t ss=0x1000) {
        Machine m(0x10000); const std::array<std::uint8_t,2> marker{0x7b,0x2d};
        if (!m.memory.write(0,marker)) return false;
        m.cpu.state.rax=0xface; m.cpu.state.rip=123; *m.cpu.state.reg(4)=456;
        auto status=load_elf64(b,m,sb,ss); std::array<std::uint8_t,2> out{};
        return status==expected && m.memory.read(0,out) && out==marker && m.cpu.state.rax==0xface &&
               m.cpu.state.rip==123 && *m.cpu.state.reg(4)==456;
    };
    { auto b=fixture(); b.resize(63); CHECK(rejected(b,ElfStatus::malformed)); }
    { auto b=fixture(); b[0]=0; CHECK(rejected(b,ElfStatus::malformed)); }
    { auto b=fixture(); b[4]=1; CHECK(rejected(b,ElfStatus::unsupported)); }
    { auto b=fixture(); b[5]=2; CHECK(rejected(b,ElfStatus::unsupported)); }
    { auto b=fixture(); put(b,16,3,2); CHECK(rejected(b,ElfStatus::unsupported)); }
    { auto b=fixture(); put(b,18,183,2); CHECK(rejected(b,ElfStatus::unsupported)); }
    { auto b=fixture(); put(b,32,UINT64_MAX,8); CHECK(rejected(b,ElfStatus::malformed)); }
    { auto b=fixture(); put(b,56,129,2); CHECK(rejected(b,ElfStatus::malformed)); }
    { auto b=fixture(); put(b,54,0,2); CHECK(rejected(b,ElfStatus::malformed)); }
    { auto b=fixture(); put(b,64,3,4); CHECK(rejected(b,ElfStatus::unsupported)); }
    { auto b=fixture(); put(b,64,2,4); CHECK(rejected(b,ElfStatus::unsupported)); }
    { auto b=fixture(); put(b,64,7,4); CHECK(rejected(b,ElfStatus::unsupported)); }
    { auto b=fixture(); put(b,68,8,4); CHECK(rejected(b,ElfStatus::unsupported)); }
    { auto b=fixture(); put(b,96,33,8); CHECK(rejected(b,ElfStatus::malformed)); }
    { auto b=fixture(); put(b,72,UINT64_MAX,8); CHECK(rejected(b,ElfStatus::malformed)); }
    { auto b=fixture(); put(b,112,3,8); CHECK(rejected(b,ElfStatus::malformed)); }
    { auto b=fixture(); put(b,112,0x1000,8); CHECK(rejected(b,ElfStatus::malformed)); }
    { auto b=fixture(); put(b,80,0xfffffffffffffff0ULL,8); CHECK(rejected(b,ElfStatus::range_error)); }
    { auto b=fixture(); CHECK(rejected(b,ElfStatus::range_error,stack,0)); }
    { auto b=fixture(); CHECK(rejected(b,ElfStatus::range_error,UINT64_MAX,1)); }
    { auto b=fixture(); CHECK(rejected(b,ElfStatus::overlap,0x1000,0x1000)); }
    { auto b=fixture(); put(b,24,0x2000,8); CHECK(rejected(b,ElfStatus::no_executable_entry)); }
    { auto b=fixture(); put(b,68,4,4); CHECK(rejected(b,ElfStatus::no_executable_entry)); }
    { auto b=fixture(); put(b,104,0,8); put(b,96,0,8); CHECK(rejected(b,ElfStatus::malformed)); }
    // Duplicate PT_LOAD is explicitly rejected rather than silently merged.
    { auto b=fixture(); put(b,56,2,2); for (unsigned i=0;i<56;++i) b[120+i]=b[64+i];
      CHECK(rejected(b,ElfStatus::overlap)); }
}
