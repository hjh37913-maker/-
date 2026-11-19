#include "memory/memory.h"
#include <algorithm>
namespace nightfall {
namespace {
constexpr std::uint8_t mapped = 8;
constexpr std::uint8_t flags(Permission p) { return static_cast<std::uint8_t>(p); }
constexpr bool valid(Permission p) { return (flags(p) & ~7u) == 0; }
}
Memory::Memory(std::size_t size, bool map_default)
    : bytes_(size, 0), attributes_(size, map_default ? mapped | 7 : 0) {}
bool Memory::contains(std::uint64_t address, std::size_t count) const noexcept {
    return address <= bytes_.size() && count <= bytes_.size() - static_cast<std::size_t>(address);
}
bool Memory::accessible(std::uint64_t address, std::size_t count, Permission permission) const noexcept {
    if (!contains(address, count)) return false;
    auto begin = attributes_.begin() + static_cast<std::size_t>(address);
    const auto required = mapped | flags(permission);
    return std::all_of(begin, begin + count, [required](auto attr) { return (attr & required) == required; });
}
bool Memory::map(std::uint64_t address, std::size_t count, Permission permissions) {
    if (!count || !contains(address, count) || !valid(permissions)) return false;
    auto offset = static_cast<std::size_t>(address);
    auto begin = attributes_.begin() + offset;
    if (!std::all_of(begin, begin + count, [](auto attr) { return !(attr & mapped); })) return false;
    std::fill_n(bytes_.begin() + offset, count, 0);
    std::fill_n(begin, count, mapped | flags(permissions));
    return true;
}
bool Memory::unmap(std::uint64_t address, std::size_t count) {
    if (!count || !accessible(address, count, Permission::none)) return false;
    auto offset = static_cast<std::size_t>(address);
    std::fill_n(attributes_.begin() + offset, count, 0);
    std::fill_n(bytes_.begin() + offset, count, 0);
    return true;
}
bool Memory::protect(std::uint64_t address, std::size_t count, Permission permissions) {
    if (!count || !valid(permissions) || !accessible(address, count, Permission::none)) return false;
    std::fill_n(attributes_.begin() + static_cast<std::size_t>(address), count, mapped | flags(permissions));
    return true;
}
bool Memory::read(std::uint64_t address, std::span<std::uint8_t> out) const {
    if (!accessible(address, out.size(), Permission::read)) return false;
    std::copy_n(bytes_.begin() + static_cast<std::size_t>(address), out.size(), out.begin());
    return true;
}
bool Memory::write(std::uint64_t address, std::span<const std::uint8_t> data) {
    if (!accessible(address, data.size(), Permission::write)) return false;
    std::copy(data.begin(), data.end(), bytes_.begin() + static_cast<std::size_t>(address));
    return true;
}
bool Memory::fetch(std::uint64_t address, std::span<std::uint8_t> out) const {
    if (!accessible(address, out.size(), Permission::execute)) return false;
    std::copy_n(bytes_.begin() + static_cast<std::size_t>(address), out.size(), out.begin());
    return true;
}
MemoryStats Memory::stats() const noexcept {
    MemoryStats result; result.capacity = size();
    for (auto attr : attributes_) if (attr & mapped) {
        ++result.mapped;
        if (attr & flags(Permission::read)) ++result.readable;
        if (attr & flags(Permission::write)) ++result.writable;
        if (attr & flags(Permission::execute)) ++result.executable;
    }
    return result;
}
}
