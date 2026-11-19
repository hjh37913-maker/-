#include "core/machine.h"
#include "storage/storage.h"
#include "loader/elf64.h"
#include "loader/elf_inspect.h"
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iomanip>
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
void diagnostic(const nightfall::RunReport& report) {
    std::cout << "Instructions attempted=" << report.attempted << " stop RIP=" << report.stop_rip << '\n';
    if (report.result==nightfall::RunResult::memory_fault || report.result==nightfall::RunResult::invalid_instruction) {
        std::cout << "Bytes accessible at stop RIP (not decoded; memory fault may be data access):";
        if (!report.fault_byte_count) std::cout << " unavailable";
        for (std::size_t i=0;i<report.fault_byte_count;++i)
            std::cout << ' ' << std::hex << std::setw(2) << std::setfill('0') << unsigned(report.fault_bytes[i]);
        std::cout << std::dec << std::setfill(' ') << '\n';
    }
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
    const auto report=machine.run_detailed(budget);
    const auto result=report.result;
    std::cout << "Experimental raw test program: " << name(result) << " RAX=" << machine.cpu.state.rax
              << " RIP=" << machine.cpu.state.rip << "\nPS5 software and GTA VI are not supported.\n";
    diagnostic(report);
    return result==nightfall::RunResult::halted?0:1;
}
int elf(const char* path,std::size_t budget,bool sparse=false) {
    constexpr std::size_t image_capacity=1024*1024, stack_size=65536;
    std::error_code ec; const auto size=std::filesystem::file_size(path,ec);
    if (ec || size==0 || size>image_capacity) { std::cerr << "Invalid test ELF file size.\n"; return 1; }
    auto data=nightfall::storage::read_range(path,0,static_cast<std::size_t>(size),image_capacity);
    if (data.status!=nightfall::storage::ReadStatus::ok) { std::cerr << "Test ELF read failed.\n"; return 1; }
    constexpr std::uint64_t sparse_capacity=1ULL<<44; // 16 TiB virtual, not host allocation.
    if (sparse && sizeof(std::size_t)<8) { std::cerr << "Sparse test mode requires a 64-bit host.\n"; return 1; }
    nightfall::Machine machine=sparse ? nightfall::Machine(static_cast<std::size_t>(sparse_capacity),nightfall::SparsePages{4096}) : nightfall::Machine(image_capacity);
    const auto stack_base=sparse?sparse_capacity-stack_size:image_capacity-stack_size;
    auto status=nightfall::loader::load_elf64(data.bytes,machine,stack_base,stack_size);
    if (status!=nightfall::loader::ElfStatus::ok) {
        std::cerr << "Test ELF rejected: " << nightfall::loader::elf_status_name(status) << '\n'; return 1;
    }
    auto report=machine.run_detailed(budget);
    auto result=report.result;
    std::cout << "Experimental ELF64 test program: " << name(result) << " RAX=" << machine.cpu.state.rax
              << " RIP=" << machine.cpu.state.rip << "\nPS5 software and GTA VI are not supported.\n";
    diagnostic(report);
    return result==nightfall::RunResult::halted?0:1;
}

int inspect(const char* path) {
    const auto info=nightfall::loader::inspect_elf64(path);
    if(info.status!=nightfall::loader::InspectStatus::ok) {
        std::cerr << "ELF inspection failed: " << nightfall::loader::inspect_status_name(info.status) << '\n'; return 1;
    }
    std::cout << "Unprotected ELF64 metadata only; no execution or decryption.\n"
              << "File size=" << info.file_size << " type=" << info.type << " machine=" << info.machine
              << " OSABI=" << unsigned(info.osabi) << " ABI version=" << unsigned(info.abi_version)
              << " entry=0x" << std::hex << info.entry << std::dec << "\n";
    for(std::size_t i=0;i<info.segments.size();++i) {
        const auto& s=info.segments[i];
        std::cout << "Program header " << i << ": type=" << s.type << " flags=" << s.flags
                  << " offset=" << s.offset << " vaddr=0x" << std::hex << s.address << std::dec
                  << " filesz=" << s.file_size << " memsz=" << s.memory_size << " align=" << s.alignment << '\n';
    }
    std::cout << "Metadata is not a compatibility result; PS5 software and GTA VI are not supported.\n";
    return 0;
}

}
int main(int argc,char** argv) {
    if (argc>1) {
        if (std::string_view(argv[1])=="--inspect-elf") {
            if(argc!=3) { std::cerr << "Usage: nightfall_cli --inspect-elf file\n"; return 2; }
            return inspect(argv[2]);
        }
        if ((std::string_view(argv[1])!="--run-raw" && std::string_view(argv[1])!="--run-elf" && std::string_view(argv[1])!="--run-elf-sparse") || argc<3 || argc>4) {
            std::cerr << "Usage: nightfall_cli [--run-raw|--run-elf|--run-elf-sparse original-test-file [instruction-budget]] | --inspect-elf file\n"; return 2;
        }
        std::size_t budget=default_budget;
        if (argc==4) {
            std::string_view value(argv[3]);
            auto result=std::from_chars(value.data(),value.data()+value.size(),budget);
            if (value.empty() || result.ec!=std::errc{} || result.ptr!=value.data()+value.size() || !budget || budget>max_budget) {
                std::cerr << "Budget must be an integer from 1 to " << max_budget << ".\n"; return 2;
            }
        }
        const auto mode=std::string_view(argv[1]);
        return mode=="--run-raw" ? raw(argv[2],budget) : elf(argv[2],budget,mode=="--run-elf-sparse");
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
