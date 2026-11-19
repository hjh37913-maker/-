#include "loader/elf64.h"
#include "storage/storage.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <system_error>
int main(int argc,char** argv) {
    if (argc!=2) { std::cerr << "Usage: elf_loader_benchmark original-elf-file\n"; return 2; }
    std::error_code ec; const auto size=std::filesystem::file_size(argv[1],ec);
    if (ec || !size || size>65536) return 1;
    auto file=nightfall::storage::read_range(argv[1],0,static_cast<std::size_t>(size),65536);
    if (file.status!=nightfall::storage::ReadStatus::ok) return 1;
    constexpr int iterations=5000;
    const auto start=std::chrono::steady_clock::now();
    for (int i=0;i<iterations;++i) {
        nightfall::Machine machine(65536);
        if (nightfall::loader::load_elf64(file.bytes,machine,0xf000,0x1000)!=nightfall::loader::ElfStatus::ok ||
            machine.run(5)!=nightfall::RunResult::halted || machine.cpu.state.rax!=42) return 1;
    }
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout << "Synthetic ELF map+execute: " << iterations << " iterations; elapsed ns=" << ns
              << "; ns/iteration=" << static_cast<double>(ns)/iterations << '\n';
}
