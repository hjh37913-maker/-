#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace nightfall {
// Experimental byte-granular mappings; not a PS5 memory map or sandbox.
enum class Permission : std::uint8_t { none = 0, read = 1, write = 2, execute = 4 };
constexpr Permission operator|(Permission a, Permission b) noexcept {
    return static_cast<Permission>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
struct MemoryStats { std::size_t capacity=0, mapped=0, readable=0, writable=0, executable=0; };
// A bounded number of 4096-byte guest pages may be allocated, independent of virtual capacity.
struct SparsePages { std::size_t max_pages; };
enum class MapStatus { ok, invalid, overlap, resource_limit };
class Memory {
public:
    // The default remains one RWX flat mapping for backwards compatibility.
    explicit Memory(std::size_t size, bool map_default = true);
    // Sparse memory starts entirely unmapped. It does not enforce canonical x86 addresses.
    explicit Memory(std::size_t size, SparsePages sparse);
    Memory(const Memory&)=default;
    Memory& operator=(const Memory&)=default;
    Memory(Memory&& other) noexcept;
    Memory& operator=(Memory&& other) noexcept;
    MapStatus map_detailed(std::uint64_t address,std::size_t count,Permission permissions);
    bool map(std::uint64_t address, std::size_t count, Permission permissions) { return map_detailed(address,count,permissions)==MapStatus::ok; }
    bool unmap(std::uint64_t address, std::size_t count);
    bool protect(std::uint64_t address, std::size_t count, Permission permissions);
    bool read(std::uint64_t address, std::span<std::uint8_t> out) const;
    bool write(std::uint64_t address, std::span<const std::uint8_t> data);
    bool fetch(std::uint64_t address, std::span<std::uint8_t> out) const;
    std::size_t size() const noexcept { return capacity_; }
    bool is_sparse() const noexcept { return sparse_; }
    std::size_t max_pages() const noexcept { return page_limit_; }
    std::size_t allocated_pages() const noexcept { return pages_.size(); }
    MemoryStats stats() const noexcept;
private:
    static constexpr std::size_t page_size=4096;
    struct Page { std::array<std::uint8_t,page_size> bytes{}, attributes{}; };
    bool contains(std::uint64_t address, std::size_t count) const noexcept;
    bool accessible(std::uint64_t address, std::size_t count, Permission permission) const noexcept;
    bool copy_out(std::uint64_t address,std::span<std::uint8_t> out,Permission permission) const;
    std::size_t capacity_;
    bool sparse_=false;
    std::size_t page_limit_=0;
    std::vector<std::uint8_t> bytes_, attributes_;
    std::map<std::uint64_t,Page> pages_;
};
}
