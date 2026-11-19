#include "core/machine.h"
#include "storage/storage.h"
#include "loader/elf64.h"
#include "loader/static_pie.h"
#include "loader/elf_inspect.h"
#include "loader/sce_import_inspect.h"
#include "loader/sce_raw_reloc.h"
#include "runtime/linux_process.h"
#include "runtime/guest_services.h"
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <string_view>
#include <system_error>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#endif
namespace {
constexpr std::size_t capacity=65536, default_budget=10000, max_budget=1000000;
// Keep untrusted metadata bytes from becoming terminal control sequences.
std::string escaped_symbol(std::string_view text) {
    constexpr char hex[]="0123456789abcdef"; std::string out;
    for(unsigned char c:text) {
        if(c>=0x20 && c<0x7f && c!='\\') out.push_back(static_cast<char>(c));
        else { out+="\\x"; out.push_back(hex[c>>4]); out.push_back(hex[c&15]); }
    }
    return out;
}
const char* name(nightfall::RunResult result) {
    switch (result) {
    case nightfall::RunResult::halted: return "halted";
    case nightfall::RunResult::syscall_trap: return "syscall_trap";
    case nightfall::RunResult::invalid_instruction: return "invalid_instruction";
    case nightfall::RunResult::memory_fault: return "memory_fault";
    case nightfall::RunResult::divide_error: return "divide_error";
    case nightfall::RunResult::alignment_fault: return "alignment_fault";
    case nightfall::RunResult::budget_exhausted: return "budget_exhausted";
    }
    return "unknown";
}
void diagnostic(const nightfall::RunReport& report) {
    std::cout << "Execution steps attempted=" << report.attempted << " stop RIP=" << report.stop_rip << '\n';
    if (report.result==nightfall::RunResult::memory_fault || report.result==nightfall::RunResult::invalid_instruction || report.result==nightfall::RunResult::divide_error || report.result==nightfall::RunResult::alignment_fault) {
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

const char* process_name(nightfall::runtime::ProcessResult result) {
    using nightfall::runtime::ProcessResult;
    switch (result) {
    case ProcessResult::exited: return "exited";
    case ProcessResult::halted: return "halted";
    case ProcessResult::invalid_instruction: return "invalid_instruction";
    case ProcessResult::memory_fault: return "memory_fault";
    case ProcessResult::budget_exhausted: return "budget_exhausted";
    case ProcessResult::already_exited: return "already_exited";
    case ProcessResult::divide_error: return "divide_error";
    case ProcessResult::alignment_fault: return "alignment_fault";
    }
    return "unknown";
}

int linux_elf(const char* path,std::size_t budget,std::span<const std::string> args,bool read_stdin,bool fixture,bool static_pie) {
    constexpr std::uint64_t virtual_size=1ULL<<44;
    constexpr std::size_t stack_size=65536, image_limit=1024*1024, max_pages=4096;
    if(sizeof(std::size_t)<8) { std::cerr << "Linux test mode requires a 64-bit host.\n"; return 1; }
    std::error_code ec; const auto size=std::filesystem::file_size(path,ec);
    if(ec || !size || size>image_limit) { std::cerr << "Invalid original test ELF size.\n"; return 1; }
    auto data=nightfall::storage::read_range(path,0,static_cast<std::size_t>(size),image_limit);
    if(data.status!=nightfall::storage::ReadStatus::ok) { std::cerr << "Test ELF read failed.\n"; return 1; }
    nightfall::Machine machine(static_cast<std::size_t>(virtual_size),nightfall::SparsePages{max_pages});
    std::string missing_symbol;
    auto status=static_pie?nightfall::loader::load_static_pie(data.bytes,machine,0x40000000ULL,virtual_size-stack_size,stack_size,{},&missing_symbol)
                          :nightfall::loader::load_elf64(data.bytes,machine,virtual_size-stack_size,stack_size);
    if(status!=nightfall::loader::ElfStatus::ok) {
        std::cerr << "Test ELF rejected: " << nightfall::loader::elf_status_name(status);
        if(!missing_symbol.empty()) std::cerr << " (unresolved guest symbol: " << escaped_symbol(missing_symbol) << ")";
        std::cerr << '\n'; return 1;
    }
    if(!nightfall::runtime::prepare_linux_stack(machine,virtual_size-stack_size,stack_size,args)) { std::cerr << "Cannot prepare bounded test process stack.\n"; return 1; }
    nightfall::runtime::LinuxProcess process(machine);
    if(fixture) {
        constexpr std::string_view content="Nightfall synthetic data.\n";
        if(!process.add_test_file("/test/input",std::span(reinterpret_cast<const std::uint8_t*>(content.data()),content.size()))) {
            std::cerr << "Cannot set up synthetic test fixture.\n"; return 1;
        }
    }
    // Fixed test-only heap; overlap with loaded mappings fails safely on brk growth.
    if(!process.configure_brk(0x80000000ULL,0x80100000ULL) || !process.configure_mmap(0x90000000ULL,0x90400000ULL)) { std::cerr << "Test heap/arena setup failed.\n"; return 1; }
#ifdef _WIN32
    // Captured guest output and explicitly supplied stdin are byte streams.
    if(_setmode(_fileno(stdout),_O_BINARY)==-1 || _setmode(_fileno(stderr),_O_BINARY)==-1 || (read_stdin && _setmode(_fileno(stdin),_O_BINARY)==-1)) {
        std::cerr << "Cannot set binary test streams.\n"; return 1;
    }
#endif
    if(read_stdin) {
        std::vector<std::uint8_t> input; std::array<char,4096> chunk{};
        while(true) {
            std::cin.read(chunk.data(),static_cast<std::streamsize>(chunk.size()));
            const auto got=static_cast<std::size_t>(std::cin.gcount());
            if(got>65536-input.size()) { std::cerr << "Test stdin exceeds 65536 bytes.\n"; return 1; }
            input.insert(input.end(),chunk.data(),chunk.data()+got);
            if(std::cin.eof()) break;
            if(!std::cin) { std::cerr << "Test stdin read failed.\n"; return 1; }
        }
        if(!process.set_stdin(input)) { std::cerr << "Test stdin setup failed.\n"; return 1; }
    }
    const auto report=process.run(budget);
    if(!process.stdout_bytes().empty()) std::cout.write(reinterpret_cast<const char*>(process.stdout_bytes().data()),static_cast<std::streamsize>(process.stdout_bytes().size()));
    if(!process.stderr_bytes().empty()) std::cerr.write(reinterpret_cast<const char*>(process.stderr_bytes().data()),static_cast<std::streamsize>(process.stderr_bytes().size()));
    std::cout.flush();
    if(!std::cout || !std::cerr) return 1;
    std::cerr << "\nExperimental Linux test ABI: steps=" << report.attempted << " stop RIP=" << report.stop_rip
              << "; PS5 software and GTA VI are not supported.\n";
    if(report.result==nightfall::runtime::ProcessResult::exited) return report.exit_code;
    std::cerr << "Test program did not exit via supported syscall; result=" << process_name(report.result) << '\n';
    return 1;
}

// Explicit opt-in for trusted, independently authored, unprotected SCE-style
// fixtures. No real libraries, syscall ABI or host symbols are supplied.
// Optional caller-asserted primitives are synthetic, not PS5 system libraries.
// It must not be used as a sandbox.
int sce_raw(const char* path, std::size_t budget, std::span<const std::string_view> service_specs={}) {
    constexpr std::size_t max_file=8*1024*1024;
    constexpr std::size_t guest_capacity=256*1024*1024, stack_size=64*1024;
    constexpr std::uint64_t bias=0x100000, stack_base=guest_capacity-stack_size;
    std::cout << "Experimental SCE-style raw ELF mode for trusted authored fixtures only.\n"
                 "No SELF/decryption, PS5 ABI, graphics or games are supported. Not a sandbox.\n";
    if(!service_specs.empty()) std::cout << "Explicit synthetic SysV services only; not PS5 exports.\n";
    std::error_code ec;
    const auto size=std::filesystem::file_size(path,ec);
    if(ec || size==0 || size>max_file) {
        std::cerr << "File must be a nonempty regular unprotected ELF no larger than " << max_file << " bytes.\n";
        return 1;
    }
    auto input=nightfall::storage::read_range(path,0,static_cast<std::size_t>(size),max_file);
    if(input.status!=nightfall::storage::ReadStatus::ok) {
        std::cerr << "Could not read the entire regular file.\n"; return 1;
    }
    nightfall::Machine machine(guest_capacity,nightfall::SparsePages{4096});
    nightfall::runtime::GuestServices services;
    std::vector<nightfall::loader::SceDependencyBinding> dependencies;
    if(service_specs.size()>32) { std::cerr << "At most 32 synthetic service bindings and dependency assertions are allowed.\n"; return 2; }
    for(std::size_t i=0;i<service_specs.size();++i) {
        const auto spec=service_specs[i];
        const auto separator=spec.find('=');
        if(separator==std::string_view::npos || separator==0 || separator+1==spec.size()) {
            std::cerr << "Each entry must be KIND=VALUE.\n"; return 2;
        }
        const auto kind=spec.substr(0,separator), symbol=spec.substr(separator+1);
        if(kind=="module" || kind=="library") {
            // Caller assertions about identity only, never library loading.
            const auto a=symbol.find(':'), b=a==symbol.npos?symbol.npos:symbol.find(':',a+1);
            const auto c=b==symbol.npos?symbol.npos:symbol.find(':',b+1);
            if(a==symbol.npos || b==symbol.npos || (kind=="module" && c==symbol.npos)) {
                std::cerr << "Dependency must be module=ID:MAJOR:MINOR:NAME or library=ID:MAJOR:NAME.\n"; return 2;
            }
            const auto parse16=[](std::string_view text,std::uint16_t& result) {
                unsigned value=0;
                const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
                if(text.empty() || parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value>65535)return false;
                result=static_cast<std::uint16_t>(value);return true;
            };
            std::uint16_t id=0,major=0,minor=0;
            const auto depname=kind=="module"?symbol.substr(c+1):symbol.substr(b+1);
            if(!parse16(symbol.substr(0,a),id) || !parse16(symbol.substr(a+1,b-a-1),major) ||
               (kind=="module" && !parse16(symbol.substr(b+1,c-b-1),minor)) ||
               depname.empty() || depname.size()>255 ||
               (kind=="module" && (major>255 || minor>255))) {
                std::cerr << "Invalid dependency identity.\n"; return 2;
            }
            const auto depkind=kind=="module"?nightfall::loader::SceDependencyKind::module:nightfall::loader::SceDependencyKind::library;
            dependencies.push_back({depkind,id,major,minor,depname});
            continue;
        }
        using nightfall::runtime::ServiceKind;
        ServiceKind selected;
        if(kind=="copy")selected=ServiceKind::copy;
        else if(kind=="move")selected=ServiceKind::move;
        else if(kind=="fill")selected=ServiceKind::fill;
        else if(kind=="compare")selected=ServiceKind::compare;
        else if(kind=="strlen")selected=ServiceKind::string_length;
        else { std::cerr << "Unknown synthetic service kind.\n"; return 2; }
        if(!services.add(std::string(symbol),0x1000+i,selected)) {
            std::cerr << "Invalid or duplicate service binding.\n"; return 2;
        }
    }
    const auto bindings=services.bindings();
    std::string missing;
    const auto status=nightfall::loader::load_sce_raw_relocatable(input.bytes,machine,bias,stack_base,stack_size,bindings,&missing,dependencies);
    if(status!=nightfall::loader::ElfStatus::ok) {
        std::cerr << "Raw ELF rejected: " << nightfall::loader::elf_status_name(status);
        if(!missing.empty()) std::cerr << " (unresolved: " << escaped_symbol(missing) << ")";
        std::cerr << "\nOnly explicitly supplied synthetic bindings and dependency assertions are available; no libraries are loaded.\n";
        return 1;
    }
    if(!service_specs.empty()) {
        // Trap addresses live below the fixed image bias, and must remain unmapped.
        for(const auto& binding:bindings) {
            if(machine.memory.can_access(binding.guest_address,1,nightfall::Permission::none)) {
                std::cerr << "Synthetic trap address overlaps guest memory.\n"; return 1;
            }
        }
        const auto report=nightfall::runtime::run_with_services(machine,services,budget);
        using nightfall::runtime::GuestRunResult;
        const char* result="unknown";
        switch(report.result) {
        case GuestRunResult::budget_exhausted: result="budget_exhausted"; break;
        case GuestRunResult::halted: result="halted"; break;
        case GuestRunResult::syscall_trap: result="syscall_trap"; break;
        case GuestRunResult::invalid_instruction: result="invalid_instruction"; break;
        case GuestRunResult::memory_fault: result="memory_fault"; break;
        case GuestRunResult::divide_error: result="divide_error"; break;
        case GuestRunResult::alignment_fault: result="alignment_fault"; break;
        case GuestRunResult::service_limit: result="service_limit"; break;
        case GuestRunResult::unsupported_service: result="unsupported_service"; break;
        }
        std::cout << "Experimental guest stopped: " << result << " RAX=" << machine.cpu.state.rax
                  << " RIP=" << machine.cpu.state.rip << "\nExecution steps attempted=" << report.attempted
                  << " stop RIP=" << report.stop_rip << '\n';
        if(report.result==GuestRunResult::syscall_trap) std::cerr << "System calls are not implemented in this mode.\n";
        return report.result==GuestRunResult::halted?0:1;
    }
    const auto report=machine.run_detailed(budget);
    std::cout << "Experimental guest stopped: " << name(report.result)
              << " RAX=" << machine.cpu.state.rax << " RIP=" << machine.cpu.state.rip << '\n';
    diagnostic(report);
    if(report.result==nightfall::RunResult::syscall_trap)
        std::cerr << "System calls are not implemented in this mode.\n";
    return report.result==nightfall::RunResult::halted?0:1;
}

// Opt-in experimental multi-image mode. No system libraries, services, host
// symbols, decryptors, constructors or startup ABI are supplied automatically.
int sce_bundle(std::span<const char* const> paths,std::size_t budget) {
    constexpr std::size_t max_file=8*1024*1024,max_total=32*1024*1024;
    constexpr std::size_t guest_capacity=256*1024*1024,stack_size=64*1024;
    constexpr std::uint64_t first_bias=0x100000,bias_step=0x400000,stack_base=guest_capacity-stack_size;
    std::cout << "Experimental exact-name SCE-style raw ELF bundle for trusted authored files only.\n"
                 "Not a PS5 emulator, game launcher, decryptor or sandbox.\n";
    std::vector<std::vector<std::uint8_t>> files;files.reserve(paths.size());
    std::size_t total=0;
    for(std::size_t i=0;i<paths.size();++i){
        std::error_code ec;const auto size=std::filesystem::file_size(paths[i],ec);
        if(ec||size==0||size>max_file||size>max_total-total){
            std::cerr << "Invalid or oversized regular file at index " << i << ".\n";return 1;
        }
        auto read=nightfall::storage::read_range(paths[i],0,static_cast<std::size_t>(size),max_file);
        if(read.status!=nightfall::storage::ReadStatus::ok){std::cerr << "Read failed at index " << i << ".\n";return 1;}
        total+=static_cast<std::size_t>(size);files.push_back(std::move(read.bytes));
    }
    std::vector<nightfall::loader::SceBundleImage> images;images.reserve(files.size());
    for(std::size_t i=0;i<files.size();++i)
        images.push_back({files[i],first_bias+i*bias_step,{}, {}});
    nightfall::Machine machine(guest_capacity,nightfall::SparsePages{4096});
    std::string problem;std::size_t failed=SIZE_MAX;
    const auto status=nightfall::loader::load_sce_raw_bundle_autobind(images,machine,stack_base,stack_size,&problem,&failed);
    if(status!=nightfall::loader::ElfStatus::ok){
        std::cerr << "Bundle rejected: " << nightfall::loader::elf_status_name(status);
        if(failed!=SIZE_MAX)std::cerr << " image=" << failed;
        if(!problem.empty())std::cerr << " symbol/dependency=" << escaped_symbol(problem);
        std::cerr << "\nNo PS5 libraries or services are supplied.\n";return 1;
    }
    const auto report=machine.run_detailed(budget);
    std::cout << "Experimental guest stopped: " << name(report.result) << " RAX=" << machine.cpu.state.rax
              << " RIP=" << machine.cpu.state.rip << '\n';
    diagnostic(report);
    if(report.result==nightfall::RunResult::syscall_trap)std::cerr << "System calls are not implemented in this mode.\n";
    return report.result==nightfall::RunResult::halted?0:1;
}

int inspect_sce(const char* path) {
    const auto report=nightfall::loader::inspect_sce_imports(path);
    if(report.status!=nightfall::loader::SceInspectStatus::ok) {
        std::cerr << "Raw candidate ELF import inspection failed: " << nightfall::loader::sce_inspect_status_name(report.status) << '\n';
        return 1;
    }
    std::cout << "Raw candidate SCE-style ELF metadata only; platform and game compatibility are NOT established.\n"
              << "No code was loaded or executed; encrypted containers are not supported.\n"
              << "Undefined global/weak symbol entries=" << report.imports.size() << '\n';
    for(const auto& item:report.imports) {
        std::cout << "symbol[" << item.symbol_index << "] binding=" << unsigned(item.binding) << " type=" << unsigned(item.type)
                  << " name=" << escaped_symbol(item.encoded_name);
        if(item.qualified) {
            std::cout << " library=" << (item.library_declaration>=0?escaped_symbol(report.declarations[item.library_declaration].name):"<unmatched>")
                      << " module=" << (item.module_declaration>=0?escaped_symbol(report.declarations[item.module_declaration].name):"<unmatched>");
        }
        std::cout << '\n';
    }
    std::cout << "OS-tagged module/library declarations=" << report.declarations.size() << '\n';
    for(const auto& item:report.declarations) {
        using nightfall::loader::SceDeclarationKind;
        const char* kind=item.kind==SceDeclarationKind::imported_module?"import-module":item.kind==SceDeclarationKind::exported_module?"export-module":item.kind==SceDeclarationKind::imported_library?"import-library":"export-library";
        std::cout << kind << " id=" << item.id << " version=" << item.version_major << "." << item.version_minor << " name=" << escaped_symbol(item.name) << '\n';
    }
    std::cout << "OS-tagged RELA entries=" << report.relocations.size() << '\n';
    for(const auto& item:report.relocations)
        std::cout << (item.plt?"plt":"rela") << " type=" << item.type << " symbol=" << item.symbol_index
                  << " target=0x" << std::hex << item.target << " addend=0x" << item.addend << std::dec << '\n';
    return 0;
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
        if (std::string_view(argv[1])=="--run-linux-elf" || std::string_view(argv[1])=="--run-linux-elf-stdin" || std::string_view(argv[1])=="--run-linux-elf-fixture" || std::string_view(argv[1])=="--run-linux-static-pie") {
            if(argc<3 || argc>131) { std::cerr << "Usage: nightfall_cli --run-linux-elf file [budget [arguments...]]\n"; return 2; }
            std::size_t budget=default_budget;
            if(argc>=4) {
                std::string_view value(argv[3]); auto result=std::from_chars(value.data(),value.data()+value.size(),budget);
                if(value.empty() || result.ec!=std::errc{} || result.ptr!=value.data()+value.size() || !budget || budget>max_budget) {
                    std::cerr << "Budget must be an integer from 1 to " << max_budget << ".\n"; return 2;
                }
            }
            std::vector<std::string> args{"nightfall-guest"};
            for(int i=4;i<argc;++i) args.emplace_back(argv[i]);
            return linux_elf(argv[2],budget,args,std::string_view(argv[1])=="--run-linux-elf-stdin",std::string_view(argv[1])=="--run-linux-elf-fixture",std::string_view(argv[1])=="--run-linux-static-pie");
        }
        if (std::string_view(argv[1])=="--run-sce-bundle") {
            if(argc<5 || argc>35) {
                std::cerr << "Usage: nightfall_cli --run-sce-bundle budget provider... executable\n"; return 2;
            }
            std::size_t budget=0;const std::string_view value(argv[2]);
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),budget);
            if(value.empty()||parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||!budget||budget>max_budget){
                std::cerr << "Budget must be an integer from 1 to " << max_budget << ".\n";return 2;
            }
            std::vector<const char*> paths;for(int i=3;i<argc;++i)paths.push_back(argv[i]);
            return sce_bundle(paths,budget);
        }
        if (std::string_view(argv[1])=="--run-sce-raw-services") {
            if(argc<5 || argc>36) {
                std::cerr << "Usage: nightfall_cli --run-sce-raw-services trusted-fixture budget KIND=VALUE [...]\n"; return 2;
            }
            std::size_t budget=0;
            const std::string_view value(argv[3]);
            const auto parsed=std::from_chars(value.data(),value.data()+value.size(),budget);
            if(value.empty() || parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || !budget || budget>max_budget) {
                std::cerr << "Budget must be an integer from 1 to " << max_budget << ".\n"; return 2;
            }
            std::vector<std::string_view> specs;
            for(int i=4;i<argc;++i)specs.emplace_back(argv[i]);
            return sce_raw(argv[2],budget,specs);
        }
        if (std::string_view(argv[1])=="--run-sce-raw") {
            if(argc<3 || argc>4) { std::cerr << "Usage: nightfall_cli --run-sce-raw trusted-original-fixture [instruction-budget]\n"; return 2; }
            std::size_t budget=default_budget;
            if(argc==4) {
                std::string_view value(argv[3]);
                auto result=std::from_chars(value.data(),value.data()+value.size(),budget);
                if(value.empty() || result.ec!=std::errc{} || result.ptr!=value.data()+value.size() || !budget || budget>max_budget) {
                    std::cerr << "Budget must be an integer from 1 to " << max_budget << ".\n"; return 2;
                }
            }
            return sce_raw(argv[2],budget);
        }
        if (std::string_view(argv[1])=="--inspect-sce-imports") {
            if(argc!=3) { std::cerr << "Usage: nightfall_cli --inspect-sce-imports unprotected-elf\n"; return 2; }
            return inspect_sce(argv[2]);
        }
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
