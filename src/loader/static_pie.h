#pragma once
#include "loader/elf64.h"
#include <string_view>
#include <string>
namespace nightfall::loader {
struct ImportBinding { std::string_view name; std::uint64_t guest_address=0; };
// Strict opt-in research loader for unprotected, independently authored ELF64
// ET_DYN fixtures using selected RELA relocations and explicitly supplied
// guest-address-only bindings. No host symbols or paths are resolved. This is NOT a
// dynamic linker, PS5 executable loader, DRM handler, or security boundary.
// bias must meet each PT_LOAD alignment. Failures leave machine unchanged.
// missing_symbol, when supplied, is cleared on entry and reports only the
// first unresolved symbol; it never looks up or discloses host addresses.
ElfStatus load_static_pie(std::span<const std::uint8_t> file, Machine& machine,
                          std::uint64_t bias,std::uint64_t stack_base,std::size_t stack_size,
                          std::span<const ImportBinding> bindings={},
                          std::string* missing_symbol=nullptr);
}
