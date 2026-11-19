#pragma once
#include "cpu/cpu.h"
#include <array>
#include <cstddef>
#include <cstdint>
namespace nightfall {
enum class RunResult { halted, syscall_trap, invalid_instruction, memory_fault, budget_exhausted, divide_error };
// Diagnostic bytes are execute-accessible bytes at the failing RIP, not a decoded
// instruction or a guarantee that a memory fault occurred during instruction fetch.
struct RunReport {
    RunResult result=RunResult::budget_exhausted;
    std::size_t attempted=0;
    std::uint64_t stop_rip=0;
    std::array<std::uint8_t,15> fault_bytes{};
    std::size_t fault_byte_count=0;
};
class Machine {
public:
    explicit Machine(std::size_t memory_size) : memory(memory_size) {}
    Machine(std::size_t memory_size, SparsePages sparse) : memory(memory_size,sparse) {}
    // Budget counts CPU steps. A repeated string instruction uses one step
    // per element (or one step for a zero-count instruction).
    RunResult run(std::size_t instruction_budget);
    RunReport run_detailed(std::size_t instruction_budget);
    Memory memory;
    Cpu cpu;
};
}
