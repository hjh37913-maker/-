#include "cpu/cpu.h"
#include <array>
#include <chrono>
#include <iostream>
int main() {
    constexpr std::size_t iterations=1000000;
    constexpr std::array<std::uint8_t,2> code{0x50,0x59}; // push rax; pop rcx
    nightfall::Memory memory(256); if (!memory.write(0,code)) return 1;
    nightfall::Cpu cpu; cpu.state.rax=42; *cpu.state.reg(4)=256;
    const auto start=std::chrono::steady_clock::now();
    for (std::size_t i=0;i<iterations;++i) {
        cpu.state.rip=0;
        if (cpu.step(memory)!=nightfall::StepResult::running || cpu.step(memory)!=nightfall::StepResult::running) return 1;
    }
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    if (*cpu.state.reg(4)!=256 || *cpu.state.reg(1)!=42) return 1;
    std::cout << "Synthetic PUSH/POP: " << iterations << " pairs, elapsed ns=" << ns
              << ", ns/pair=" << static_cast<double>(ns)/iterations << '\n';
}
