#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace nightfall::storage {
enum class ReadStatus { ok, limit_exceeded, open_failed, not_regular_file, range_error, io_error, allocation_failed };
struct ReadResult {
    ReadStatus status;
    std::vector<std::uint8_t> bytes;
};
// Synchronous bounded read of an exact byte range of a regular host file.
// On error the result contains no data. Not a path sandbox or snapshot.
ReadResult read_range(const std::filesystem::path& path, std::uint64_t offset,
                      std::size_t length, std::size_t max_bytes);

// CRC-32/ISO-HDLC; accidental corruption detection, NOT authentication.
class Crc32 {
public:
    void update(std::span<const std::uint8_t> bytes) noexcept;
    void reset() noexcept { state_ = 0xffffffffu; }
    std::uint32_t value() const noexcept { return state_ ^ 0xffffffffu; }
private:
    std::uint32_t state_ = 0xffffffffu;
};
}
