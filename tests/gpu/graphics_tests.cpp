#include "graphics/surface.h"
#include <algorithm>
#include <array>
#include <utility>
#include <cstdint>
#include <iostream>
#include <variant>
#include <vector>
using namespace nightfall::graphics;
namespace {
constexpr std::uint32_t w=8,h=7;
std::uint64_t next(std::uint64_t& x) { x ^= x<<13; x ^= x>>7; x ^= x<<17; return x; }
bool valid_rect(std::uint32_t x,std::uint32_t y,std::uint32_t rw,std::uint32_t rh) {
    return x<=w && y<=h && rw<=w-x && rh<=h-y;
}
bool reference(std::vector<Pixel>& pixels,const std::vector<Command>& batch) {
    for (const auto& cmd:batch) {
        if (const auto* f=std::get_if<Fill>(&cmd)) {
            if (!valid_rect(f->x,f->y,f->width,f->height)) return false;
        } else if (const auto* c=std::get_if<Copy>(&cmd)) {
            if (!valid_rect(c->sx,c->sy,c->width,c->height) || !valid_rect(c->dx,c->dy,c->width,c->height)) return false;
        }
    }
    for (const auto& cmd:batch) {
        if (auto c=std::get_if<Clear>(&cmd)) { for (auto& p:pixels) p=c->color; }
        else if (auto f=std::get_if<Fill>(&cmd)) {
            for (std::uint32_t y=0;y<f->height;++y) for (std::uint32_t x=0;x<f->width;++x)
                pixels[(f->y+y)*w+f->x+x]=f->color;
        } else if (auto c=std::get_if<Copy>(&cmd)) {
            auto before=pixels; // Independent snapshot oracle for overlapping regions.
            for (std::uint32_t y=0;y<c->height;++y) for (std::uint32_t x=0;x<c->width;++x)
                pixels[(c->dy+y)*w+c->dx+x]=before[(c->sy+y)*w+c->sx+x];
        }
    }
    return true;
}
}
int main() {
    if (Surface::create(0,1,1) || Surface::create(1,0,1) || Surface::create(9,7,62) ||
        Surface::create(UINT32_MAX,UINT32_MAX,1000) || !Surface::create(9,7,63)) return 1;
    // Moved-from surfaces remain empty and safely reject nonempty operations.
    { auto original=Surface::create(2,2,4); if (!original) return 1;
      Surface moved=std::move(*original);
      const std::array<Command,1> cmd{Fill{0,0,1,1,Pixel{1,2,3,4}}};
      if (original->width()!=0 || original->height()!=0 || !original->pixels().empty() || original->execute(cmd) || !moved.execute(cmd)) return 1;
      *original=std::move(moved);
      if (moved.width()!=0 || moved.height()!=0 || moved.execute(cmd) || !original->execute(cmd)) return 1;
    }
    std::size_t accepted=0,rejected=0,clears=0,fills=0,copies=0;
    for (std::uint64_t seed=1;seed<=100;++seed) {
        std::uint64_t rng=0x4752415048494353ULL ^ (seed*0x9e3779b97f4a7c15ULL);
        auto surface=Surface::create(w,h,w*h);
        if (!surface) return 1;
        std::vector<Pixel> model(w*h);
        for (std::size_t step=0;step<200;++step) {
            std::vector<Command> batch;
            const unsigned commands=1+next(rng)%3;
            for (unsigned i=0;i<commands;++i) {
                const auto op=next(rng)%3;
                const Pixel color{static_cast<std::uint8_t>(next(rng)),static_cast<std::uint8_t>(next(rng)),
                    static_cast<std::uint8_t>(next(rng)),static_cast<std::uint8_t>(next(rng))};
                if (op==0) { batch.emplace_back(Clear{color}); ++clears; }
                else if (op==1) {
                    auto x=static_cast<std::uint32_t>(next(rng)%10), y=static_cast<std::uint32_t>(next(rng)%9);
                    auto rw=static_cast<std::uint32_t>(next(rng)%5), rh=static_cast<std::uint32_t>(next(rng)%5);
                    batch.emplace_back(Fill{x,y,rw,rh,color}); ++fills;
                } else {
                    auto sx=static_cast<std::uint32_t>(next(rng)%10), sy=static_cast<std::uint32_t>(next(rng)%9);
                    auto dx=static_cast<std::uint32_t>(next(rng)%10), dy=static_cast<std::uint32_t>(next(rng)%9);
                    auto rw=static_cast<std::uint32_t>(next(rng)%5), rh=static_cast<std::uint32_t>(next(rng)%5);
                    batch.emplace_back(Copy{sx,sy,dx,dy,rw,rh}); ++copies;
                }
            }
            const bool expected=reference(model,batch), actual=surface->execute(batch);
            if (actual) ++accepted; else ++rejected;
            if (expected!=actual || !std::equal(model.begin(),model.end(),surface->pixels().begin(),surface->pixels().end())) {
                std::cerr << "Graphics mismatch seed=" << seed << " batch=" << step << '\n'; return 1;
            }
        }
    }
    // Explicitly cover every direction of overlapping copies and transactional rejection.
    auto surface=Surface::create(w,h,w*h); std::vector<Pixel> model(w*h);
    if (!surface) return 1;
    std::vector<Command> pattern;
    for (unsigned y=0;y<4;++y) for (unsigned x=0;x<4;++x) pattern.emplace_back(Fill{x,y,1,1,Pixel{static_cast<std::uint8_t>(y*4+x),0,0,255}});
    if (!reference(model,pattern) || !surface->execute(pattern)) return 1;
    for (Copy copy : {Copy{0,0,1,0,3,4},Copy{1,0,0,0,3,4},Copy{0,0,0,1,4,3},Copy{0,1,0,0,4,3},Copy{0,0,1,1,3,3}}) {
        std::vector<Command> batch{copy};
        if (!reference(model,batch) || !surface->execute(batch) || !std::equal(model.begin(),model.end(),surface->pixels().begin())) return 1;
    }
    const auto old=model;
    std::vector<Command> invalid{Clear{Pixel{1,2,3,4}},Fill{UINT32_MAX,0,1,1,Pixel{}}};
    if (surface->execute(invalid) || !std::equal(old.begin(),old.end(),surface->pixels().begin())) return 1;
    std::vector<Command> empty{Fill{w,h,0,0,Pixel{}},Copy{w,h,w,h,0,0}};
    if (!surface->execute(empty) || !surface->execute({})) return 1;
    if (!accepted || !rejected || !clears || !fills || !copies) return 1;
    std::cout << "100 deterministic scenarios, 200 batches each: " << accepted << " accepted, " << rejected
              << " rejected; " << clears << " clears, " << fills << " fills, " << copies << " copies generated\n";
}
