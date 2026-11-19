// Self-contained original test data. No guest file or host symbol is loaded.
#include "loader/sce_raw_reloc.h"
#include "runtime/guest_services.h"
#include "../examples/synthetic/sce_test_fixture.h"
#include <array>
#include <iostream>
using namespace nightfall;
using namespace nightfall::loader;
using namespace nightfall::runtime;
int main() {
    std::cout << "NIGHTFALL synthetic loader + guest service diagnostic\n"
                 "Not a PS5 emulator or GTA VI launcher; no game files are used.\n";
    auto file=fixture();
    // lea test string, rdi; mov relocated import pointer, rax; call *rax; hlt
    constexpr std::array<std::uint8_t,17> code{0x48,0x8d,0x3d,0x19,0x10,0,0,0x48,0x8b,0x05,0xf2,0x0f,0,0,0xff,0xd0,0xf4};
    constexpr std::array<std::uint8_t,6> text{'h','e','l','l','o',0};
    for(std::size_t i=0;i<code.size();++i)file[0x400+i]=code[i];
    for(std::size_t i=0;i<text.size();++i)file[0x520+i]=text[i];
    for(std::uint64_t bias : {0x10000ULL,0x30000ULL,0x50000ULL}) {
        Machine machine(0x100000,SparsePages{64});
        GuestServices services;
        if(!services.add("synthetic#A#B",0x80000,ServiceKind::string_length)) {
            std::cerr << "Service registration failed.\n"; return 1;
        }
        const auto bindings=services.bindings();
        const auto status=load_sce_raw_relocatable(file,machine,bias,0xf0000,0x10000,bindings);
        if(status!=ElfStatus::ok) {
            std::cerr << "Synthetic load failed: " << elf_status_name(status) << '\n'; return 1;
        }
        const auto result=run_with_services(machine,services,100);
        if(result.result!=GuestRunResult::halted || result.attempted!=5 || machine.cpu.state.rax!=5 || *machine.cpu.state.reg(4)!=0x100000) {
            std::cerr << "Synthetic execution failed at bias 0x" << std::hex << bias << std::dec << "\n"; return 1;
        }
        std::cout << "PASS bias=0x" << std::hex << bias << std::dec
                  << " steps=" << result.attempted << " guest strlen=5\n";
    }
    std::cout << "Synthetic diagnostic passed.\n";
}
