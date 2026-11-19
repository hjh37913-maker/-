#include "cpu/cpu.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
int main() {
    constexpr std::size_t iterations=1000000;
    constexpr std::array<std::uint8_t,3> program{0x48,0x01,0xd8}; // add rax,rbx
    nightfall::Memory memory(program.size());
    if (!memory.write(0,program)) return 1;
    nightfall::Cpu cpu; *cpu.state.reg(3)=1;
    const auto start=std::chrono::steady_clock::now();
    for (std::size_t i=0;i<iterations;++i) {
        cpu.state.rip=0;
        if (cpu.step(memory)!=nightfall::StepResult::running) return 1;
    }
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    if (cpu.state.rax!=iterations) return 1;
    std::cout << "Synthetic ADD RAX,RBX: " << iterations << " iterations, elapsed ns: " << ns
              << ", ns/iteration: " << static_cast<double>(ns)/iterations << '\n';
}
