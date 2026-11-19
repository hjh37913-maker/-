#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace nightfall::graphics {
struct Pixel {
    std::uint8_t r=0, g=0, b=0, a=0;
    bool operator==(const Pixel&) const = default;
};
struct Clear { Pixel color; };
struct Fill { std::uint32_t x, y, width, height; Pixel color; };
struct Copy { std::uint32_t sx, sy, dx, dy, width, height; };
// Integer pixel-space vertices. Pixel centers and a top-left fill rule are used.
// Coordinates outside [-32768,32767] are rejected to bound edge arithmetic.
struct Point { std::int32_t x, y; };
struct Triangle { Point a, b, c; Pixel color; };
using Command = std::variant<Clear, Fill, Copy, Triangle>;
// CPU-only RGBA8 software reference. Not a console GPU or a sandbox.
class Surface {
public:
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&& other) noexcept;
    Surface& operator=(Surface&& other) noexcept;
    static std::optional<Surface> create(std::uint32_t width, std::uint32_t height, std::size_t max_pixels);
    std::uint32_t width() const noexcept { return width_; }
    std::uint32_t height() const noexcept { return height_; }
    std::span<const Pixel> pixels() const noexcept { return pixels_; }
    // Validates the complete batch first. An invalid command leaves pixels unchanged.
    bool execute(std::span<const Command> commands) noexcept;
private:
    Surface(std::uint32_t width, std::uint32_t height, std::vector<Pixel>&& pixels);
    bool within(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) const noexcept;
    std::uint32_t width_, height_;
    std::vector<Pixel> pixels_;
};
}
