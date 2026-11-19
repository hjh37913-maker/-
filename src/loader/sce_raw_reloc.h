#pragma once
#include "loader/static_pie.h"
namespace nightfall::loader {
enum class SceDependencyKind { module, library };
struct SceDependencyBinding {
 SceDependencyKind kind{};std::uint16_t id=0,version_major=0,version_minor=0;std::string_view name;
};
// Experimental raw SCE-style ELF relocation for independently authored test
// inputs only. Does not recognize SELF, decrypt, implement the PS5 ABI, or
// run games. Strict subset; unknown headers/tags are rejected. Bindings are
// explicit guest addresses, never host symbols. Optional dependency identities
// are caller assertions, not loaded modules. Failure preserves machine.
ElfStatus load_sce_raw_relocatable(std::span<const std::uint8_t> file,Machine& machine,
                                  std::uint64_t bias,std::uint64_t stack_base,std::size_t stack_size,
                                  std::span<const ImportBinding> bindings={},std::string* missing_name=nullptr,
                                  std::span<const SceDependencyBinding> dependencies={});
}
