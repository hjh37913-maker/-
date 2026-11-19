#include "loader/elf64.h"
#include <array>
#include <utility>
namespace nightfall::loader {
namespace {
std::uint64_t le(std::span<const std::uint8_t> data, std::size_t pos, unsigned count) {
    std::uint64_t value=0;
    for (unsigned i=0;i<count;++i) value |= std::uint64_t(data[pos+i]) << (8*i);
    return value;
}
bool fits(std::uint64_t offset, std::uint64_t count, std::uint64_t size) {
    return offset<=size && count<=size-offset;
}
}
const char* elf_status_name(ElfStatus status) noexcept {
    switch (status) {
    case ElfStatus::ok: return "ok";
    case ElfStatus::malformed: return "malformed";
    case ElfStatus::unsupported: return "unsupported";
    case ElfStatus::range_error: return "range_error";
    case ElfStatus::overlap: return "overlap";
    case ElfStatus::no_executable_entry: return "no_executable_entry";
    }
    return "unknown";
}
ElfStatus load_elf64(std::span<const std::uint8_t> file, Machine& machine,
                     std::uint64_t stack_base, std::size_t stack_size) {
    if (file.size()<64 || file[0]!=0x7f || file[1]!='E' || file[2]!='L' || file[3]!='F') return ElfStatus::malformed;
    if (file[4]!=2 || file[5]!=1 || file[6]!=1 || le(file,16,2)!=2 || le(file,18,2)!=62 || le(file,20,4)!=1)
        return ElfStatus::unsupported;
    const auto entry=le(file,24,8), phoff=le(file,32,8);
    const auto ehsize=le(file,52,2), entsize=le(file,54,2), count=le(file,56,2);
    if (ehsize!=64 || entsize!=56 || !count || count>128 || !fits(phoff,count*56,file.size())) return ElfStatus::malformed;
    if (!stack_size || !fits(stack_base,stack_size,machine.memory.size())) return ElfStatus::range_error;
    Memory staged(machine.memory.size(),false);
    unsigned loads=0;
    for (std::uint64_t i=0;i<count;++i) {
        const auto at=static_cast<std::size_t>(phoff+i*56);
        const auto type=le(file,at,4), flags=le(file,at+4,4), offset=le(file,at+8,8),
                   address=le(file,at+16,8), filesz=le(file,at+32,8), memsz=le(file,at+40,8), align=le(file,at+48,8);
        if (type==2 || type==3 || type==7) return ElfStatus::unsupported; // dynamic, interpreter, TLS
        if (type!=1) continue;
        if (flags & ~7ULL) return ElfStatus::unsupported;
        if (filesz>memsz || !fits(offset,filesz,file.size())) return ElfStatus::malformed;
        if (align>1 && ((align & (align-1)) || ((address^offset)&(align-1)))) return ElfStatus::malformed;
        if (!memsz) continue;
        if (!fits(address,memsz,staged.size())) return ElfStatus::range_error;
        if (!staged.map(address,static_cast<std::size_t>(memsz),Permission::read | Permission::write)) return ElfStatus::overlap;
        if (filesz && !staged.write(address,file.subspan(static_cast<std::size_t>(offset),static_cast<std::size_t>(filesz)))) return ElfStatus::malformed;
        Permission perm=Permission::none;
        if (flags&4) perm=perm | Permission::read;
        if (flags&2) perm=perm | Permission::write;
        if (flags&1) perm=perm | Permission::execute;
        if (!staged.protect(address,static_cast<std::size_t>(memsz),perm)) return ElfStatus::malformed;
        ++loads;
    }
    if (!loads) return ElfStatus::malformed;
    if (!staged.map(stack_base,stack_size,Permission::read | Permission::write)) return ElfStatus::overlap;
    std::array<std::uint8_t,1> probe{};
    if (!staged.fetch(entry,probe)) return ElfStatus::no_executable_entry;
    machine.memory=std::move(staged);
    machine.cpu.state=CpuState{};
    machine.cpu.state.rip=entry;
    *machine.cpu.state.reg(4)=stack_base+stack_size;
    return ElfStatus::ok;
}
}
