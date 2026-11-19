#include "core/machine.h"
namespace nightfall {
RunResult Machine::run(std::size_t instruction_budget) {
    for (std::size_t i = 0; i < instruction_budget; ++i) {
        switch (cpu.step(memory)) {
        case StepResult::running: break;
        case StepResult::halted: return RunResult::halted;
        case StepResult::invalid_instruction: return RunResult::invalid_instruction;
        case StepResult::memory_fault: return RunResult::memory_fault;
        }
    }
    return RunResult::budget_exhausted;
}
}
