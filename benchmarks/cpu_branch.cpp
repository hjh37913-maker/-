#include "cpu/cpu.h"
#include <array>
#include <chrono>
#include <iostream>
int main() {
    constexpr std::size_t iterations=1000000;
    // TEST RAX,RAX; JNZ back to start.
    constexpr std::array<std::uint8_t,5> code{0x48,0x85,0xc0,0x75,0xfb};
    nightfall::Memory memory(code.size()); if (!memory.write(0,code)) return 1;
    nightfall::Cpu cpu; cpu.state.rax=1;
    const auto start=std::chrono::steady_clock::now();
    for (std::size_t i=0;i<iterations;++i)
        if (cpu.step(memory)!=nightfall::StepResult::running || cpu.step(memory)!=nightfall::StepResult::running) return 1;
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    if (cpu.state.rip!=0 || cpu.state.rax!=1) return 1;
    std::cout << "Synthetic TEST/JNZ loop: " << iterations << " pairs; elapsed ns=" << ns
              << "; ns/pair=" << static_cast<double>(ns)/iterations << '\n';
}
