#include "core/machine.h"
#include <limits>
namespace nightfall {
RunReport Machine::run_detailed(std::size_t instruction_budget) {
    RunReport report;
    for (std::size_t i=0;i<instruction_budget;++i) {
        const auto before=cpu.state.rip;
        const auto step=cpu.step(memory);
        ++report.attempted;
        if (step==StepResult::running) continue;
        report.stop_rip=before;
        if (step==StepResult::halted) { report.result=RunResult::halted; return report; }
        report.result=step==StepResult::invalid_instruction?RunResult::invalid_instruction:RunResult::memory_fault;
        for (std::size_t n=0;n<report.fault_bytes.size();++n) {
            if (n>std::numeric_limits<std::uint64_t>::max()-before ||
                !memory.fetch(before+n,std::span(report.fault_bytes).subspan(n,1))) break;
            ++report.fault_byte_count;
        }
        return report;
    }
    report.stop_rip=cpu.state.rip;
    return report;
}
RunResult Machine::run(std::size_t instruction_budget) { return run_detailed(instruction_budget).result; }
}
