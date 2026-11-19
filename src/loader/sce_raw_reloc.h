#pragma once
#include "loader/static_pie.h"
namespace nightfall::loader {
// Experimental raw SCE-style ELF relocation for independently authored test
// inputs only. Does not recognize SELF, decrypt, implement the PS5 ABI, or
// run games. Strict subset; unknown headers/tags are rejected. Bindings are
// explicit guest addresses, never host symbols. Failure preserves machine.
ElfStatus load_sce_raw_relocatable(std::span<const std::uint8_t> file,Machine& machine,
                                  std::uint64_t bias,std::uint64_t stack_base,std::size_t stack_size,
                                  std::span<const ImportBinding> bindings={},std::string* missing_symbol=nullptr);
}
