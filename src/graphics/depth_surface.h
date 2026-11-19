#pragma once
#include "graphics/surface.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace nightfall::graphics {
struct DepthVertex { Point position; std::uint16_t depth; };
struct DepthClear { Pixel color; std::uint16_t depth; };
struct DepthTriangle { DepthVertex a,b,c; Pixel color; };
using DepthCommand=std::variant<DepthClear,DepthTriangle>;
// Experimental CPU rasterizer with affine 16-bit depth and strict LESS test.
// This is not a PS5 GPU, shader implementation, or perspective-correct renderer.
class DepthSurface {
public:
    DepthSurface(const DepthSurface&)=delete;
    DepthSurface& operator=(const DepthSurface&)=delete;
    DepthSurface(DepthSurface&& other) noexcept;
    DepthSurface& operator=(DepthSurface&& other) noexcept;
    static std::optional<DepthSurface> create(std::uint32_t width,std::uint32_t height,std::size_t max_pixels);
    std::uint32_t width() const noexcept { return width_; }
    std::uint32_t height() const noexcept { return height_; }
    std::span<const Pixel> pixels() const noexcept { return pixels_; }
    std::span<const std::uint16_t> depths() const noexcept { return depth_; }
    // Entire command batch is validated before rendering. Invalid means no changes.
    bool execute(std::span<const DepthCommand> commands) noexcept;
private:
    DepthSurface(std::uint32_t width,std::uint32_t height,std::vector<Pixel>&& pixels,std::vector<std::uint16_t>&& depth);
    std::uint32_t width_,height_;
    std::vector<Pixel> pixels_;
    std::vector<std::uint16_t> depth_;
};
}
