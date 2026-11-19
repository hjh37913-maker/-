#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace nightfall::loader {
// Passive raw-ELF metadata inspection only. Does not parse containers, decrypt,
// load, resolve or execute code. A header match is NOT platform identification.
enum class SceInspectStatus { ok, read_error, not_elf, unsupported, malformed, resource_limit };
struct SceImport { std::string encoded_name; std::uint32_t symbol_index=0; std::uint8_t binding=0,type=0; };
struct SceRelocation { std::uint64_t target=0,addend=0; std::uint32_t symbol_index=0,type=0; bool plt=false; };
struct SceImportReport {
 SceInspectStatus status=SceInspectStatus::read_error;
 std::uint16_t object_type=0,machine=0;
 std::uint8_t osabi=0;
 std::uint64_t file_size=0;
 std::vector<SceImport> imports;
 std::vector<SceRelocation> relocations;
};
SceImportReport inspect_sce_imports(const std::filesystem::path& path);
const char* sce_inspect_status_name(SceInspectStatus status) noexcept;
}
