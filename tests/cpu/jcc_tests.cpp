#include "cpu/cpu.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace nightfall;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    std::size_t cases=0;
    for (unsigned combination=0;combination<32;++combination) {
        const bool cf=combination&1,pf=combination&2,zf=combination&4,sf=combination&8,of=combination&16;
        const std::uint64_t flags=2|(cf?1:0)|(pf?4:0)|(zf?0x40:0)|(sf?0x80:0)|(of?0x800:0);
        const std::array<bool,16> expected{of,!of,cf,!cf,zf,!zf,cf||zf,!cf&&!zf,sf,!sf,pf,!pf,sf!=of,sf==of,zf||(sf!=of),!zf&&(sf==of)};
        for (unsigned cc=0;cc<16;++cc) for (bool near : {false,true}) {
            // Both displacements point from the instruction end back to address 62.
            const std::array<std::uint8_t,6> short_code{static_cast<std::uint8_t>(0x70+cc),0xfc,0,0,0,0};
            const std::array<std::uint8_t,6> near_code{0x0f,static_cast<std::uint8_t>(0x80+cc),0xf8,0xff,0xff,0xff};
            Memory mem(128); const auto len=near?6:2;
            CHECK(mem.write(64,std::span(near?near_code:short_code).first(len)));
            Cpu cpu; cpu.state.rip=64; cpu.state.rflags=flags; cpu.state.rax=0x1234;
            CHECK(cpu.step(mem)==StepResult::running && cpu.state.rip==(expected[cc]?62u:64u+len));
            CHECK(cpu.state.rflags==flags && cpu.state.rax==0x1234); ++cases;
        }
    }
    for (unsigned n=1;n<6;++n) {
        const std::array<std::uint8_t,6> code{0x0f,0x84,1,0,0,0}; Memory mem(n); CHECK(mem.write(0,std::span(code).first(n)));
        Cpu cpu; cpu.state.rflags=0x402; CHECK(cpu.step(mem)==StepResult::memory_fault && cpu.state.rip==0 && cpu.state.rflags==0x402);
    }
    { Memory mem(2); const std::array<std::uint8_t,2> code{0x0f,0xff}; CHECK(mem.write(0,code)); Cpu cpu;
      CHECK(cpu.step(mem)==StepResult::invalid_instruction && cpu.state.rip==0); }
    std::cout << cases << " conditional branch/flag cases passed\n";
}
