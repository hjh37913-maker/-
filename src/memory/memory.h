#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace nightfall {
// Experimental byte-granular mappings; not a PS5 memory map or sandbox.
enum class Permission : std::uint8_t { none = 0, read = 1, write = 2, execute = 4 };
constexpr Permission operator|(Permission a, Permission b) noexcept {
    return static_cast<Permission>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
struct MemoryStats {
    std::size_t capacity = 0, mapped = 0, readable = 0, writable = 0, executable = 0;
};
class Memory {
public:
    // The default is a single RWX mapping to retain the original experimental API.
    // Pass false to start with no mappings.
    explicit Memory(std::size_t size, bool map_default = true);
    bool map(std::uint64_t address, std::size_t count, Permission permissions);
    bool unmap(std::uint64_t address, std::size_t count);
    bool protect(std::uint64_t address, std::size_t count, Permission permissions);
    bool read(std::uint64_t address, std::span<std::uint8_t> out) const;
    bool write(std::uint64_t address, std::span<const std::uint8_t> data);
    bool fetch(std::uint64_t address, std::span<std::uint8_t> out) const;
    std::size_t size() const noexcept { return bytes_.size(); }
    MemoryStats stats() const noexcept;
private:
    bool contains(std::uint64_t address, std::size_t count) const noexcept;
    bool accessible(std::uint64_t address, std::size_t count, Permission permission) const noexcept;
    std::vector<std::uint8_t> bytes_;
    // Bit 3 means mapped, low three bits are permissions.
    std::vector<std::uint8_t> attributes_;
};
}
