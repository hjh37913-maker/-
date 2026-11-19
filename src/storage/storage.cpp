#include "storage/storage.h"
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>
#include <system_error>
#include <utility>
namespace nightfall::storage {
ReadResult read_range(const std::filesystem::path& path, std::uint64_t offset,
                      std::size_t length, std::size_t max_bytes) {
    if (length > max_bytes) return {ReadStatus::limit_exceeded, {}};
    // This preflight prevents ordinary directory/device reads, not symlink races.
    std::error_code ec;
    const auto info = std::filesystem::status(path, ec);
    if (ec || !std::filesystem::exists(info)) return {ReadStatus::open_failed, {}};
    if (!std::filesystem::is_regular_file(info)) return {ReadStatus::not_regular_file, {}};
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {ReadStatus::open_failed, {}};
    const auto end = in.tellg();
    if (end < 0) return {ReadStatus::io_error, {}};
    const auto size = static_cast<std::uint64_t>(end);
    if (offset > size || length > size - offset ||
        offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()) ||
        length > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))
        return {ReadStatus::range_error, {}};
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) return {ReadStatus::io_error, {}};
    std::vector<std::uint8_t> bytes;
    try { bytes.resize(length); }
    catch (const std::bad_alloc&) { return {ReadStatus::allocation_failed, {}}; }
    catch (const std::length_error&) { return {ReadStatus::allocation_failed, {}}; }
    if (length && !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length)))
        return {ReadStatus::io_error, {}};
    return {ReadStatus::ok, std::move(bytes)};
}
void Crc32::update(std::span<const std::uint8_t> bytes) noexcept {
    for (auto byte : bytes) {
        state_ ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit)
            state_ = (state_ >> 1) ^ (0xedb88320u & (0u - (state_ & 1u)));
    }
}
}
