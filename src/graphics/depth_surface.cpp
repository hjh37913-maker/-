#include "graphics/depth_surface.h"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
namespace nightfall::graphics {
namespace {
bool valid(Point p) { return p.x>=-32768 && p.x<=32767 && p.y>=-32768 && p.y<=32767; }
std::int64_t edge(Point p,Point q,std::int64_t x,std::int64_t y) {
    return (std::int64_t(q.x)-p.x)*(y-2*std::int64_t(p.y))-(std::int64_t(q.y)-p.y)*(x-2*std::int64_t(p.x));
}
bool inclusive(Point p,Point q) { return q.y<p.y || (q.y==p.y && q.x>p.x); }
bool inside(std::int64_t e,Point p,Point q) { return e>0 || (e==0 && inclusive(p,q)); }
}
DepthSurface::DepthSurface(std::uint32_t w,std::uint32_t h,std::vector<Pixel>&& p,std::vector<std::uint16_t>&& d)
 : width_(w),height_(h),pixels_(std::move(p)),depth_(std::move(d)) {}
DepthSurface::DepthSurface(DepthSurface&& other) noexcept
 : width_(other.width_),height_(other.height_),pixels_(std::move(other.pixels_)),depth_(std::move(other.depth_)) {
    other.width_=other.height_=0; other.pixels_.clear(); other.depth_.clear();
}
DepthSurface& DepthSurface::operator=(DepthSurface&& other) noexcept {
    if(this!=&other) { width_=other.width_; height_=other.height_; pixels_=std::move(other.pixels_); depth_=std::move(other.depth_);
        other.width_=other.height_=0; other.pixels_.clear(); other.depth_.clear(); }
    return *this;
}
std::optional<DepthSurface> DepthSurface::create(std::uint32_t w,std::uint32_t h,std::size_t max_pixels) {
    auto count=std::uint64_t(w)*h;
    if(!w || !h || count>max_pixels || count>std::numeric_limits<std::size_t>::max()) return std::nullopt;
    try { return DepthSurface(w,h,std::vector<Pixel>(static_cast<std::size_t>(count)),std::vector<std::uint16_t>(static_cast<std::size_t>(count),65535)); }
    catch(const std::bad_alloc&) { return std::nullopt; } catch(const std::length_error&) { return std::nullopt; }
}
bool DepthSurface::execute(std::span<const DepthCommand> commands) noexcept {
    for(const auto& cmd:commands) {
        if(const auto* t=std::get_if<DepthTriangle>(&cmd)) {
            if(!valid(t->a.position)||!valid(t->b.position)||!valid(t->c.position)) return false;
        } else if(!std::holds_alternative<DepthClear>(cmd)) return false;
    }
    for(const auto& cmd:commands) {
        if(const auto* c=std::get_if<DepthClear>(&cmd)) {
            std::fill(pixels_.begin(),pixels_.end(),c->color); std::fill(depth_.begin(),depth_.end(),c->depth);
        } else if(const auto* t=std::get_if<DepthTriangle>(&cmd)) {
            auto a=t->a,b=t->b,c=t->c;
            auto area=edge(a.position,b.position,2*std::int64_t(c.position.x),2*std::int64_t(c.position.y));
            if(!area) continue;
            if(area<0) { std::swap(b,c); area=-area; }
            const auto x0=std::max<std::int64_t>(0,std::min({a.position.x,b.position.x,c.position.x}));
            const auto y0=std::max<std::int64_t>(0,std::min({a.position.y,b.position.y,c.position.y}));
            const auto x1=std::min<std::int64_t>(width_,std::max({a.position.x,b.position.x,c.position.x}));
            const auto y1=std::min<std::int64_t>(height_,std::max({a.position.y,b.position.y,c.position.y}));
            for(auto y=y0;y<y1;++y) for(auto x=x0;x<x1;++x) {
                auto wa=edge(b.position,c.position,2*x+1,2*y+1), wb=edge(c.position,a.position,2*x+1,2*y+1), wc=edge(a.position,b.position,2*x+1,2*y+1);
                if(!inside(wa,b.position,c.position)||!inside(wb,c.position,a.position)||!inside(wc,a.position,b.position)) continue;
                // Nonnegative weights sum to area; the bounded coordinates ensure no int64 overflow.
                auto z=static_cast<std::uint16_t>((wa*a.depth+wb*b.depth+wc*c.depth+area/2)/area);
                auto i=static_cast<std::size_t>(y)*width_+static_cast<std::size_t>(x);
                if(z<depth_[i]) { depth_[i]=z; pixels_[i]=t->color; }
            }
        }
    }
    return true;
}
}
