#include "core/machine.h"
#include "loader/elf64.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
using namespace nightfall;
int main(int argc, char** argv) {
 if(argc!=2) return 1;
 std::ifstream f(argv[1],std::ios::binary); if(!f) return 1;
 std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
 Machine m(0x100000);
 if(loader::load_elf64(bytes,m,0xf0000,0x10000)!=loader::ElfStatus::ok) return 1;
 auto report=m.run_detailed(200); std::array<std::uint8_t,3> input{},output{};
 if(report.result!=RunResult::halted || m.cpu.state.rax!=42 || !m.memory.read(0x2000,input) || !m.memory.read(0x2003,output) || input!=std::array<std::uint8_t,3>{0xf6,20,32} || output!=input) {
  std::cerr<<"byte fixture failed: result or copied output mismatch\n"; return 1;
 }
}
