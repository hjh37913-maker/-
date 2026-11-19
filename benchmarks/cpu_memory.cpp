#include "cpu/cpu.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
int main() {
    constexpr std::size_t iterations=1000000;
    // mov [rbx], rax; mov rax, [rbx]
    constexpr std::array<std::uint8_t,6> code{0x48,0x89,0x03,0x48,0x8b,0x03};
    nightfall::Memory memory(256); if (!memory.write(0,code)) return 1;
    nightfall::Cpu cpu; cpu.state.rax=0x123456789abcdef0ULL; *cpu.state.reg(3)=128;
    const auto start=std::chrono::steady_clock::now();
    for (std::size_t i=0;i<iterations;++i) {
        cpu.state.rip=0;
        if (cpu.step(memory)!=nightfall::StepResult::running || cpu.step(memory)!=nightfall::StepResult::running) return 1;
    }
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    if (cpu.state.rax!=0x123456789abcdef0ULL) return 1;
    std::cout << "Synthetic MOV store+load: " << iterations << " pairs, elapsed ns=" << ns
              << ", ns/pair=" << static_cast<double>(ns)/iterations << '\n';
}
