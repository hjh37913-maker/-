#include "storage/storage.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>
int main() {
    constexpr std::size_t bytes = 1024 * 1024, repetitions = 32;
    std::vector<std::uint8_t> data(bytes);
    for (std::size_t i = 0; i < bytes; ++i) data[i] = static_cast<std::uint8_t>(i);
    std::uint32_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < repetitions; ++i) {
        nightfall::storage::Crc32 crc;
        crc.update(data);
        if (i && checksum != crc.value()) return 1;
        checksum = crc.value();
    }
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout << "Synthetic CRC-32: " << repetitions << " x " << bytes << " bytes\n"
              << "Elapsed ns: " << ns << "\nChecksum: " << std::hex << checksum << std::dec << '\n';
}
