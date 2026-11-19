#include "core/machine.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace nightfall;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; ++failures; } } while (0)
int main() {
    constexpr auto rw = Permission::read | Permission::write;
    constexpr auto rx = Permission::read | Permission::execute;
    Memory m(16, false);
    std::array<std::uint8_t, 4> data{1,2,3,4}, out{9,9,9,9};
    CHECK(m.stats().mapped == 0); CHECK(!m.read(0, out)); CHECK(out[0] == 9);
    CHECK(!m.map(0, 0, rw)); CHECK(!m.map(UINT64_MAX, 2, rw));
    CHECK(!m.map(0, 1, static_cast<Permission>(8)));
    CHECK(m.map(2, 4, rw)); CHECK(!m.map(4, 4, rw)); CHECK(m.stats().mapped == 4);
    CHECK(m.write(2, data)); CHECK(m.read(2, out)); CHECK(out == data);
    CHECK(!m.write(4, data)); CHECK(m.read(2, out)); CHECK(out == data); // no partial write
    CHECK(m.protect(4, 2, Permission::read)); CHECK(!m.write(2, data));
    CHECK(m.read(2, out)); CHECK(out == data); // mixed permission failure is atomic
    CHECK(!m.protect(5, 2, rw)); CHECK(!m.unmap(5, 2)); CHECK(m.stats().mapped == 4);
    auto stats = m.stats(); CHECK(stats.capacity == 16 && stats.readable == 4 && stats.writable == 2 && stats.executable == 0);
    CHECK(m.protect(2, 4, Permission::none)); CHECK(!m.read(2, out)); CHECK(m.stats().mapped == 4);
    CHECK(m.unmap(2, 4)); CHECK(m.map(2, 4, rw)); CHECK(m.read(2, out));
    CHECK((out == std::array<std::uint8_t,4>{0,0,0,0})); // remapping never exposes old data
    CHECK(m.read(16, std::span(out).first(0))); CHECK(!m.read(17, std::span(out).first(0)));
    { Machine machine(6); constexpr std::array<std::uint8_t, 6> ins{0x48,0x05,1,0,0,0};
      CHECK(machine.memory.write(0, ins)); CHECK(machine.memory.protect(0, 6, rx));
      CHECK(machine.memory.protect(5, 1, Permission::read));
      machine.cpu.state.rax = 7; machine.cpu.state.rflags = 0x402;
      CHECK(machine.run(1) == RunResult::memory_fault); CHECK(machine.cpu.state.rip == 0);
      CHECK(machine.cpu.state.rax == 7 && machine.cpu.state.rflags == 0x402);
      CHECK(machine.memory.protect(5, 1, Permission::execute));
      CHECK(machine.run(1) == RunResult::budget_exhausted); CHECK(machine.cpu.state.rax == 8);
      CHECK(!machine.memory.read(5, std::span(out).first(1))); // execute does not imply read
    }
    { Machine machine(1); constexpr std::array<std::uint8_t,1> nop{0x90};
      CHECK(machine.memory.write(0, nop)); CHECK(machine.memory.protect(0, 1, Permission::read));
      CHECK(machine.run(1) == RunResult::memory_fault); CHECK(machine.cpu.state.rip == 0); }
    return failures ? 1 : 0;
}
