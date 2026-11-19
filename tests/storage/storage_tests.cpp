#include "storage/storage.h"
#include "core/machine.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>
using namespace nightfall;
using namespace nightfall::storage;
namespace fs = std::filesystem;
namespace {
struct TempDir {
    fs::path path;
    TempDir() {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned i = 0; i < 100; ++i) {
            auto candidate = fs::temp_directory_path() / ("nightfall-test-" + std::to_string(tick) + "-" + std::to_string(i));
            std::error_code ec;
            if (fs::create_directory(candidate, ec)) { path = candidate; return; }
        }
        throw std::runtime_error("cannot create test directory");
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};
bool save(const fs::path& p, std::span<const std::uint8_t> bytes) {
    std::ofstream out(p, std::ios::binary);
    if (!out) return false;
    if (!bytes.empty()) out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close(); return bool(out);
}
std::span<const std::uint8_t> as_bytes(std::string_view s) {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}
std::uint32_t crc(std::span<const std::uint8_t> bytes) { Crc32 c; c.update(bytes); return c.value(); }
}
int main() {
    TempDir temp; const auto fixture = temp.path / "fixture.bin"; const auto empty = temp.path / "empty.bin";
    const std::array<std::uint8_t,16> source{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    const std::array<std::uint8_t,17> program{0x48,0xb8,40,0,0,0,0,0,0,0,0x48,0x05,2,0,0,0,0xf4};
    const auto program_path = temp.path / "program.bin";
    if (!save(fixture, source) || !save(empty, {}) || !save(program_path, program)) {
        std::cerr << "Cannot create fixtures\n"; return 1;
    }
    std::array<bool,40> seen{}; unsigned failures=0;
    auto check = [&](unsigned id, bool result) {
        if (id < 1 || id > seen.size() || seen[id-1]) { ++failures; std::cerr << "Invalid test id " << id << '\n'; return; }
        seen[id-1]=true;
        if (!result) { ++failures; std::cerr << "STORAGE-" << id << " failed\n"; }
    };
    auto read = [&](std::uint64_t offset, std::size_t length, std::size_t limit = 16) { return read_range(fixture, offset, length, limit); };
    auto matches = [&](std::uint64_t offset, std::size_t length) {
        auto result = read(offset, length); return result.status == ReadStatus::ok &&
          result.bytes == std::vector<std::uint8_t>(source.begin()+offset, source.begin()+offset+length);
    };
    check(1, matches(0,16));
    check(2, matches(0,1));
    check(3, matches(15,1));
    check(4, matches(4,4));
    check(5, matches(8,8));
    check(6, matches(1,14));
    check(7, matches(0,0));
    check(8, matches(16,0));
    check(9, matches(7,0));
    check(10, matches(2,1));
    check(11, matches(2,2));
    check(12, matches(2,3));
    check(13, matches(2,14));
    check(14, matches(0,15));
    check(15, matches(14,2));
    check(16, read(0,16,16).status == ReadStatus::ok);
    check(17, read(0,1,0).status == ReadStatus::limit_exceeded);
    check(18, read(0,16,15).status == ReadStatus::limit_exceeded);
    check(19, read(0,17,17).status == ReadStatus::range_error);
    check(20, read(16,1).status == ReadStatus::range_error);
    check(21, read(17,0).status == ReadStatus::range_error);
    check(22, read(UINT64_MAX,1).status == ReadStatus::range_error);
    check(23, read(UINT64_MAX,0).status == ReadStatus::range_error);
    check(24, read(15,2).status == ReadStatus::range_error);
    check(25, read_range(temp.path / "missing",0,1,1).status == ReadStatus::open_failed);
    check(26, read_range(temp.path,0,0,0).status == ReadStatus::not_regular_file);
    check(27, read_range(empty,0,0,0).status == ReadStatus::ok);
    check(28, read_range(empty,0,1,1).status == ReadStatus::range_error);
    check(29, read(15,2).bytes.empty());
    check(30, read_range(temp.path / "missing",0,1,1).bytes.empty());
    check(31, crc({}) == 0);
    check(32, crc(as_bytes("123456789")) == 0xcbf43926u);
    check(33, crc(as_bytes("abc")) == 0x352441c2u);
    check(34, crc(source) == 0xcecee288u);
    check(35, crc(std::span(source).first(1)) == 0xd202ef8du);
    const std::array<std::uint8_t,1> ff{0xff}; check(36, crc(ff) == 0xff000000u);
    check(37, crc(as_bytes("Nightfall")) == 0x430dcee8u);
    Crc32 incremental; incremental.update(as_bytes("1234")); incremental.update({}); incremental.update(as_bytes("56789"));
    check(38, incremental.value() == 0xcbf43926u);
    incremental.reset(); check(39, incremental.value() == 0);
    auto loaded = read_range(program_path,0,program.size(),program.size());
    Machine machine(64); check(40, loaded.status == ReadStatus::ok && machine.memory.write(0, loaded.bytes) &&
        machine.run(3)==RunResult::halted && machine.cpu.state.rax==42);
    for (bool entry : seen) if (!entry) { ++failures; std::cerr << "Missing test ID\n"; }
    if (!failures) std::cout << "40 storage and integration checks passed\n";
    return failures ? 1 : 0;
}
