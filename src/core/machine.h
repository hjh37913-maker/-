#pragma once
#include "cpu/cpu.h"
#include <cstddef>
namespace nightfall {
enum class RunResult { halted, invalid_instruction, memory_fault, budget_exhausted };
class Machine {
public:
    explicit Machine(std::size_t memory_size) : memory(memory_size) {}
    RunResult run(std::size_t instruction_budget);
    Memory memory;
    Cpu cpu;
};
}
