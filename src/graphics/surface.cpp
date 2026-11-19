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
        } else if (const auto* t=std::get_if<Triangle>(&command)) {
            const auto valid=[](Point p) { return p.x>=-32768 && p.x<=32767 && p.y>=-32768 && p.y<=32767; };
            if (!valid(t->a) || !valid(t->b) || !valid(t->c)) return false;
        } else if (!std::holds_alternative<Clear>(command)) return false;
    }
    for (const auto& command : commands) {
        if (const auto* c=std::get_if<Clear>(&command)) {
            std::fill(pixels_.begin(),pixels_.end(),c->color);
        } else if (const auto* f=std::get_if<Fill>(&command)) {
            if (!f->width || !f->height) continue;
            for (std::uint32_t y=0; y<f->height; ++y)
                std::fill_n(pixels_.begin()+static_cast<std::size_t>(f->y+y)*width_+f->x,f->width,f->color);
        } else if (const auto* t=std::get_if<Triangle>(&command)) {
            auto a=t->a, b=t->b, c=t->c;
            // Twice-coordinate sample space places pixel centers at odd integers.
            const auto edge=[](Point p, Point q, std::int64_t x, std::int64_t y) {
                return (std::int64_t(q.x)-p.x)*(y-2*std::int64_t(p.y)) -
                       (std::int64_t(q.y)-p.y)*(x-2*std::int64_t(p.x));
            };
            auto area=edge(a,b,2*std::int64_t(c.x),2*std::int64_t(c.y));
            if (!area) continue;
            if (area<0) std::swap(b,c);
            const auto inclusive=[](Point p, Point q) { return q.y<p.y || (q.y==p.y && q.x>p.x); };
            const auto x0=std::max<std::int64_t>(0,std::min({a.x,b.x,c.x}));
            const auto y0=std::max<std::int64_t>(0,std::min({a.y,b.y,c.y}));
            const auto x1=std::min<std::int64_t>(width_,std::max({a.x,b.x,c.x}));
            const auto y1=std::min<std::int64_t>(height_,std::max({a.y,b.y,c.y}));
            for (auto y=y0;y<y1;++y) for (auto x=x0;x<x1;++x) {
                const auto e0=edge(a,b,2*x+1,2*y+1), e1=edge(b,c,2*x+1,2*y+1), e2=edge(c,a,2*x+1,2*y+1);
                if ((e0>0 || (e0==0 && inclusive(a,b))) && (e1>0 || (e1==0 && inclusive(b,c))) &&
                    (e2>0 || (e2==0 && inclusive(c,a))))
                    pixels_[static_cast<std::size_t>(y)*width_+static_cast<std::size_t>(x)]=t->color;
            }
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
