#pragma once
#include "core/machine.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace nightfall::runtime {
// Tiny Linux x86-64 test ABI, not a Linux/PS5 compatibility layer or sandbox.
// No guest syscall is forwarded to the host. Only captured stdout/stderr writes
// bounded stdin, readv/writev, a configured test heap, and process exit are modeled;
// unknown calls return -ENOSYS.
enum class ProcessResult { exited, halted, invalid_instruction, memory_fault, budget_exhausted, already_exited, divide_error };
struct ProcessReport { ProcessResult result=ProcessResult::budget_exhausted; std::size_t attempted=0; std::uint64_t stop_rip=0; std::uint8_t exit_code=0; };
// Minimal startup layout (argc/argv, empty environment and AT_NULL), not a
// complete Linux auxiliary vector. All input is bounded and trusted only.
// A failed setup leaves stack bytes and CPU registers unchanged.
bool prepare_linux_stack(Machine& machine,std::uint64_t stack_base,std::size_t stack_size,std::span<const std::string> args);
class LinuxProcess {
public:
    explicit LinuxProcess(Machine& machine,std::size_t max_output=65536);
    // Before execution only: copies up to 65536 input bytes; no host descriptor is retained.
    bool set_stdin(std::span<const std::uint8_t> bytes);
    // Configures an initially unmapped, at-most-1 MiB heap for the modeled brk.
    // Bounds must be 4096-byte aligned and fit the guest virtual capacity.
    bool configure_brk(std::uint64_t base,std::uint64_t limit);
    ProcessReport run(std::size_t instruction_budget);
    std::span<const std::uint8_t> stdout_bytes() const noexcept { return stdout_; }
    std::span<const std::uint8_t> stderr_bytes() const noexcept { return stderr_; }
    bool exited() const noexcept { return exited_; }
private:
    void dispatch();
    Machine& machine_;
    std::size_t max_output_;
    std::vector<std::uint8_t> stdout_,stderr_,stdin_;
    std::size_t stdin_cursor_=0;
    std::uint64_t brk_base_=0, brk_limit_=0, brk_current_=0, brk_mapped_end_=0;
    bool brk_configured_=false, started_=false,exited_=false;
    std::uint8_t exit_code_=0;
};
}
