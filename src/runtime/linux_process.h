#pragma once
#include "core/machine.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace nightfall::runtime {
// Tiny Linux x86-64 test ABI, not a Linux/PS5 compatibility layer or sandbox.
// No guest syscall is forwarded to the host. Only bounded synthetic I/O,
// read-only test files, a test heap/anonymous arena, and process exit are
// modeled; unknown calls return -ENOSYS.
enum class ProcessResult { exited, halted, invalid_instruction, memory_fault, budget_exhausted, already_exited, divide_error, alignment_fault };
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
    // Before execution: register immutable in-memory synthetic bytes at an
    // absolute guest path. No path is opened or resolved by the host OS.
    bool add_test_file(std::string path,std::span<const std::uint8_t> bytes);
    // Configures an initially unmapped, at-most-1 MiB heap for the modeled brk.
    // Bounds must be 4096-byte aligned and fit the guest virtual capacity.
    bool configure_brk(std::uint64_t base,std::uint64_t limit);
    // Before execution: enable a page-aligned synthetic anonymous mmap arena,
    // at most 4 MiB. This is not host mapping or an isolation boundary.
    bool configure_mmap(std::uint64_t base,std::uint64_t limit);
    // Budget counts CPU steps, including each element of a REP instruction.
    ProcessReport run(std::size_t instruction_budget);
    std::span<const std::uint8_t> stdout_bytes() const noexcept { return stdout_; }
    std::span<const std::uint8_t> stderr_bytes() const noexcept { return stderr_; }
    bool exited() const noexcept { return exited_; }
private:
    enum class FdKind { unused, input, output, error, file };
    struct TestFile { std::string path; std::vector<std::uint8_t> bytes; };
    struct Descriptor { FdKind kind=FdKind::unused; std::size_t file_index=0; std::uint64_t cursor=0; };
    struct ReadSource { const std::vector<std::uint8_t>* bytes=nullptr; std::uint64_t* cursor=nullptr; };
    ReadSource read_source(std::uint32_t fd) noexcept;
    unsigned lookup_test_file(std::uint64_t guest_path,std::size_t& index);
    bool write_test_stat(std::size_t index,std::uint64_t guest_address);
    void dispatch();
    Machine& machine_;
    std::size_t max_output_;
    std::vector<std::uint8_t> stdout_,stderr_,stdin_;
    std::uint64_t stdin_cursor_=0;
    std::vector<TestFile> files_;
    std::size_t file_bytes_=0;
    std::array<Descriptor,19> descriptors_{};
    std::uint64_t brk_base_=0, brk_limit_=0, brk_current_=0, brk_mapped_end_=0;
    std::uint64_t mmap_base_=0,mmap_limit_=0;
    std::array<bool,1024> mmap_pages_{};
    bool mmap_configured_=false;
    bool brk_configured_=false, started_=false,exited_=false;
    std::uint8_t exit_code_=0;
};
}
