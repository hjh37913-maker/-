#include "cpu/cpu.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
int main() {
    constexpr std::size_t iterations=1000000;
    constexpr std::array<std::uint8_t,3> program{0x83,0xc0,0x01}; // add eax,1
    nightfall::Memory memory(program.size()); if (!memory.write(0,program)) return 1;
    nightfall::Cpu cpu;
    const auto start=std::chrono::steady_clock::now();
    for (std::size_t i=0;i<iterations;++i) {
        cpu.state.rip=0;
        if (cpu.step(memory)!=nightfall::StepResult::running) return 1;
    }
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    if (cpu.state.rax!=iterations) return 1;
    std::cout << "Synthetic 32-bit ADD imm8: " << iterations << " iterations; elapsed ns=" << ns
              << "; ns/iteration=" << static_cast<double>(ns)/iterations << '\n';
}
