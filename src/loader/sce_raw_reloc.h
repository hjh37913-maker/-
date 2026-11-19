#pragma once
#include "loader/static_pie.h"
#include <vector>
namespace nightfall::loader {
enum class SceDependencyKind { module, library };
struct SceExport { std::string name; std::uint64_t guest_address=0; bool weak=false; };
struct SceDependencyBinding {
 SceDependencyKind kind{};std::uint16_t id=0,version_major=0,version_minor=0;std::string_view name;
};
// Experimental raw SCE-style ELF relocation for independently authored test
// inputs only. Does not recognize SELF, decrypt, implement the PS5 ABI, or
// run games. Strict subset; unknown headers/tags are rejected. Bindings are
// explicit guest addresses, never host symbols. Generic x86-64 NONE, PC32,
// PLT32, 32 and 32S relocations are newly implemented but not yet tested;
// their meaning on actual PS5 files is not verified. Optional dependency identities
// are caller assertions, not loaded modules. Failure preserves machine.
ElfStatus load_sce_raw_relocatable(std::span<const std::uint8_t> file,Machine& machine,
                                  std::uint64_t bias,std::uint64_t stack_base,std::size_t stack_size,
                                  std::span<const ImportBinding> bindings={},std::string* missing_name=nullptr,
                                  std::span<const SceDependencyBinding> dependencies={});
// Unverified multi-image research APIs. Append a provider to existing guest
// memory without changing CPU state, then append and start a consumer. Callers
// must initialize an empty unmapped Machine and explicitly resolve all guest
// symbols and dependency identities; no automatic linking, interposition, TLS,
// initializers, unloading, PS5 module semantics or system libraries exist.
// Optional provider exports are owned copies of exact names and guest addresses,
// not verified PS5 exports. They are replaced only on a successful append.
// Each call stages the whole memory and commits only on success. A failed
// later call does not undo earlier successful calls. No concurrent access.
// Allocation exceptions may propagate. Neither API is a security boundary.
ElfStatus append_sce_raw_module(std::span<const std::uint8_t> file,Machine& machine,
                                std::uint64_t bias,std::span<const ImportBinding> bindings={},
                                std::string* missing_name=nullptr,
                                std::span<const SceDependencyBinding> dependencies={},
                                std::vector<SceExport>* exports=nullptr);
ElfStatus append_and_start_sce_raw_module(std::span<const std::uint8_t> file,Machine& machine,
                                          std::uint64_t bias,std::uint64_t stack_base,std::size_t stack_size,
                                          std::span<const ImportBinding> bindings={},std::string* missing_name=nullptr,
                                          std::span<const SceDependencyBinding> dependencies={});
// All spans and names must remain alive for the call. Providers precede the
// executable; the final image starts on the supplied stack. Names and guest
// addresses are caller-provided. No provider export matching is automatic.
struct SceBundleImage {
 std::span<const std::uint8_t> file;
 std::uint64_t bias=0;
 std::span<const ImportBinding> bindings;
 std::span<const SceDependencyBinding> dependencies;
};
// At most 32 images. Stages the entire bundle, preserving original machine
// state on error; failed_index is SIZE_MAX when failure is not image-specific.
// Allocation exceptions may propagate. This API has not been tested.
ElfStatus load_sce_raw_bundle(std::span<const SceBundleImage> images,Machine& machine,
                              std::uint64_t stack_base,std::size_t stack_size,
                              std::string* missing_name=nullptr,std::size_t* failed_index=nullptr);
// Experimental alternative: providers must precede their consumers. Ordinary
// visible defined exports of earlier images become exact-name guest bindings
// for later images. Duplicate names are rejected, including weak/strong ties;
// no versioning, symbol interposition, forward references, circular linking,
// host symbols or authentic PS5 NID semantics. Caller bindings are per-image;
// conflicts with discovered exports are rejected. Dependency identities remain
// explicit assertions. Up to 1024 discovered exports and bindings per image.
// Whole bundle rollback; not yet tested or safe for untrusted inputs.
ElfStatus load_sce_raw_bundle_autobind(std::span<const SceBundleImage> images,Machine& machine,
                                       std::uint64_t stack_base,std::size_t stack_size,
                                       std::string* problem_name=nullptr,std::size_t* failed_index=nullptr);
}
