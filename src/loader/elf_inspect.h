#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
namespace nightfall::loader {
// Bounded metadata inspection of an unprotected little-endian ELF64 file.
// It never loads or executes instructions and does not decrypt or interpret
// proprietary/encrypted formats. Metadata does not prove compatibility.
enum class InspectStatus { ok, read_error, not_elf, unsupported_format, malformed };
struct InspectSegment {
    std::uint32_t type=0, flags=0;
    std::uint64_t offset=0, address=0, file_size=0, memory_size=0, alignment=0;
};
struct ElfInspection {
    InspectStatus status=InspectStatus::read_error;
    std::uint64_t file_size=0, entry=0;
    std::uint16_t type=0, machine=0;
    std::uint8_t osabi=0, abi_version=0;
    std::vector<InspectSegment> segments;
};
ElfInspection inspect_elf64(const std::filesystem::path& path);
const char* inspect_status_name(InspectStatus status) noexcept;
}
