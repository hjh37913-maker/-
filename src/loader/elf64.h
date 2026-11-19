#pragma once
#include "core/machine.h"
#include <cstddef>
#include <cstdint>
#include <span>
namespace nightfall::loader {
// Restricted, static, unprotected ELF64 x86-64 research programs only.
enum class ElfStatus { ok, malformed, unsupported, range_error, overlap, resource_limit, no_executable_entry, unresolved_symbol };
// Replaces machine state only on success. Stack region must not overlap a load segment.
// No dynamic linker, relocations, syscall ABI, ELF security or PS5 support.
ElfStatus load_elf64(std::span<const std::uint8_t> file, Machine& machine,
                     std::uint64_t stack_base, std::size_t stack_size);
const char* elf_status_name(ElfStatus status) noexcept;
}
