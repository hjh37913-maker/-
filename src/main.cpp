#include "core/machine.h"
#include "storage/storage.h"
#include "loader/elf64.h"
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <system_error>
namespace {
constexpr std::size_t capacity=65536, default_budget=10000, max_budget=1000000;
const char* name(nightfall::RunResult result) {
    switch (result) {
    case nightfall::RunResult::halted: return "halted";
    case nightfall::RunResult::invalid_instruction: return "invalid_instruction";
    case nightfall::RunResult::memory_fault: return "memory_fault";
    case nightfall::RunResult::budget_exhausted: return "budget_exhausted";
    }
    return "unknown";
}
int raw(const char* path,std::size_t budget) {
    std::error_code ec;
    const auto size=std::filesystem::file_size(path,ec);
    if (ec || size==0 || size>capacity) {
        std::cerr << "Cannot use file: must be a nonempty regular test file of at most " << capacity << " bytes.\n";
        return 1;
    }
    auto data=nightfall::storage::read_range(path,0,static_cast<std::size_t>(size),capacity);
    if (data.status!=nightfall::storage::ReadStatus::ok) { std::cerr << "Test file read failed.\n"; return 1; }
    nightfall::Machine machine(capacity);
    if (!machine.memory.write(0,data.bytes) || !machine.memory.protect(0,capacity,nightfall::Permission::read | nightfall::Permission::write) ||
        !machine.memory.protect(0,data.bytes.size(),nightfall::Permission::read | nightfall::Permission::execute)) {
        std::cerr << "Failed to initialize synthetic guest memory.\n"; return 1;
    }
    // Synthetic raw programs start with an empty stack at the end of memory.
    *machine.cpu.state.reg(4)=capacity;
    const auto result=machine.run(budget);
    std::cout << "Experimental raw test program: " << name(result) << " RAX=" << machine.cpu.state.rax
              << " RIP=" << machine.cpu.state.rip << "\nPS5 software and GTA VI are not supported.\n";
    return result==nightfall::RunResult::halted?0:1;
}
int elf(const char* path,std::size_t budget) {
    constexpr std::size_t image_capacity=1024*1024, stack_size=65536;
    std::error_code ec; const auto size=std::filesystem::file_size(path,ec);
    if (ec || size==0 || size>image_capacity) { std::cerr << "Invalid test ELF file size.\n"; return 1; }
    auto data=nightfall::storage::read_range(path,0,static_cast<std::size_t>(size),image_capacity);
    if (data.status!=nightfall::storage::ReadStatus::ok) { std::cerr << "Test ELF read failed.\n"; return 1; }
    nightfall::Machine machine(image_capacity);
    auto status=nightfall::loader::load_elf64(data.bytes,machine,image_capacity-stack_size,stack_size);
    if (status!=nightfall::loader::ElfStatus::ok) {
        std::cerr << "Test ELF rejected: " << nightfall::loader::elf_status_name(status) << '\n'; return 1;
    }
    auto result=machine.run(budget);
    std::cout << "Experimental ELF64 test program: " << name(result) << " RAX=" << machine.cpu.state.rax
              << " RIP=" << machine.cpu.state.rip << "\nPS5 software and GTA VI are not supported.\n";
    return result==nightfall::RunResult::halted?0:1;
}

}
int main(int argc,char** argv) {
    if (argc>1) {
        if ((std::string_view(argv[1])!="--run-raw" && std::string_view(argv[1])!="--run-elf") || argc<3 || argc>4) {
            std::cerr << "Usage: nightfall_cli [--run-raw|--run-elf original-test-file [instruction-budget]]\n"; return 2;
        }
        std::size_t budget=default_budget;
        if (argc==4) {
            std::string_view value(argv[3]);
            auto result=std::from_chars(value.data(),value.data()+value.size(),budget);
            if (value.empty() || result.ec!=std::errc{} || result.ptr!=value.data()+value.size() || !budget || budget>max_budget) {
                std::cerr << "Budget must be an integer from 1 to " << max_budget << ".\n"; return 2;
            }
        }
        return std::string_view(argv[1])=="--run-elf" ? elf(argv[2],budget) : raw(argv[2],budget);
    }
    // Original synthetic x86-64 bytes: mov rax, 40; add rax, 2; hlt.
    constexpr std::array<std::uint8_t, 17> program = {0x48,0xb8,40,0,0,0,0,0,0,0,0x48,0x05,2,0,0,0,0xf4};
    nightfall::Machine machine(4096);
    if (!machine.memory.write(0, program) || machine.run(3) != nightfall::RunResult::halted || machine.cpu.state.rax != 42) {
        std::cerr << "Synthetic execution failed\n"; return 1;
    }
    std::cout << "NIGHTFALL experimental interpreter: synthetic result RAX=" << machine.cpu.state.rax
              << "\nPS5 software and GTA VI are not supported.\n";
}
