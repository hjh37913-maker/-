#include "graphics/surface.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
namespace nightfall::graphics {
Surface::Surface(std::uint32_t width, std::uint32_t height, std::vector<Pixel>&& pixels)
    : width_(width), height_(height), pixels_(std::move(pixels)) {}
Surface::Surface(Surface&& other) noexcept
    : width_(other.width_), height_(other.height_), pixels_(std::move(other.pixels_)) {
    other.width_ = other.height_ = 0;
    other.pixels_.clear();
}
Surface& Surface::operator=(Surface&& other) noexcept {
    if (this != &other) {
        width_ = other.width_; height_ = other.height_;
        pixels_ = std::move(other.pixels_);
        other.width_ = other.height_ = 0;
        other.pixels_.clear();
    }
    return *this;
}
std::optional<Surface> Surface::create(std::uint32_t width, std::uint32_t height, std::size_t max_pixels) {
    if (!width || !height || static_cast<std::uint64_t>(width)*height > max_pixels ||
        static_cast<std::uint64_t>(width)*height > std::numeric_limits<std::size_t>::max()) return std::nullopt;
    try {
        return Surface(width, height, std::vector<Pixel>(static_cast<std::size_t>(width)*height));
    } catch (const std::bad_alloc&) { return std::nullopt; }
      catch (const std::length_error&) { return std::nullopt; }
}
bool Surface::within(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) const noexcept {
    return x<=width_ && y<=height_ && w<=width_-x && h<=height_-y;
}
bool Surface::execute(std::span<const Command> commands) noexcept {
    for (const auto& command : commands) {
        if (const auto* f=std::get_if<Fill>(&command)) {
            if (!within(f->x,f->y,f->width,f->height)) return false;
        } else if (const auto* c=std::get_if<Copy>(&command)) {
            if (!within(c->sx,c->sy,c->width,c->height) || !within(c->dx,c->dy,c->width,c->height)) return false;
        } else if (!std::holds_alternative<Clear>(command)) return false;
    }
    for (const auto& command : commands) {
        if (const auto* c=std::get_if<Clear>(&command)) {
            std::fill(pixels_.begin(),pixels_.end(),c->color);
        } else if (const auto* f=std::get_if<Fill>(&command)) {
            if (!f->width || !f->height) continue;
            for (std::uint32_t y=0; y<f->height; ++y)
                std::fill_n(pixels_.begin()+static_cast<std::size_t>(f->y+y)*width_+f->x,f->width,f->color);
        } else if (const auto* c=std::get_if<Copy>(&command)) {
            if (!c->width || !c->height) continue;
            static_assert(std::is_trivially_copyable_v<Pixel>);
            // Copy bottom-up for downward overlap, otherwise top-down. memmove
            // handles horizontal overlap within each row without a temporary buffer.
            for (std::uint32_t n=0; n<c->height; ++n) {
                const auto row = c->dy>c->sy ? c->height-1-n : n;
                auto* dst=pixels_.data()+static_cast<std::size_t>(c->dy+row)*width_+c->dx;
                const auto* src=pixels_.data()+static_cast<std::size_t>(c->sy+row)*width_+c->sx;
                std::memmove(dst,src,static_cast<std::size_t>(c->width)*sizeof(Pixel));
            }
        }
    }
    return true;
}
}
