#include "cpu/cpu.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
int main() {
    constexpr std::size_t iterations = 1000000;
    constexpr std::array<std::uint8_t, 6> program{0x48, 0x05, 1, 0, 0, 0};
    nightfall::Memory memory(program.size());
    if (!memory.write(0, program)) return 1;
    nightfall::Cpu cpu;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        cpu.state.rip = 0;
        if (cpu.step(memory) != nightfall::StepResult::running) return 1;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();
    if (cpu.state.rax != iterations) return 1;
    std::cout << "Workload: ADD RAX, 1; host resets RIP each iteration\n"
              << "Iterations: " << iterations << "\nElapsed nanoseconds: " << elapsed
              << "\nNanoseconds per iteration: " << static_cast<double>(elapsed) / iterations
              << "\nFinal RAX: " << cpu.state.rax << '\n';
}
