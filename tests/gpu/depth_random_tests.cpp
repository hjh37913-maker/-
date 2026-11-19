#include "graphics/depth_surface.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall::graphics;
std::uint64_t next(std::uint64_t& x) { x^=x<<13; x^=x>>7; x^=x<<17; return x; }
int main() {
 constexpr int W=8,H=7; std::uint64_t rng=0x938aa11223dedf58ULL; std::size_t cases=0;
 for(unsigned scene=0;scene<2000;++scene) {
  auto s=DepthSurface::create(W,H,W*H); if(!s) return 1;
  std::vector<Pixel> colors(W*H,Pixel{1,2,3,255}); std::vector<std::uint16_t> depths(W*H,65535);
  std::vector<DepthCommand> commands{DepthClear{Pixel{1,2,3,255},65535}};
  for(unsigned i=0;i<8;++i) {
   auto vertex=[&]() { return DepthVertex{Point{std::int32_t(next(rng)%23)-7,std::int32_t(next(rng)%21)-7},std::uint16_t(next(rng))}; };
   DepthTriangle t{vertex(),vertex(),vertex(),Pixel{std::uint8_t(next(rng)),std::uint8_t(next(rng)),std::uint8_t(next(rng)),255}}; commands.push_back(t);
   auto cross=[](Point a,Point b,std::int64_t x,std::int64_t y) { return (std::int64_t(b.x)-a.x)*(y-2*std::int64_t(a.y))-(std::int64_t(b.y)-a.y)*(x-2*std::int64_t(a.x)); };
   auto a=t.a,b=t.b,c=t.c; auto area=cross(a.position,b.position,2*c.position.x,2*c.position.y); if(!area) continue;
   if(area<0) { std::swap(b,c); area=-area; }
   auto accept=[](std::int64_t v,Point p,Point q) { return v>0 || (v==0 && (q.y<p.y || (q.y==p.y && q.x>p.x))); };
   // Traverse all surface pixels rather than the renderer's clipped bounding box.
   for(int y=0;y<H;++y) for(int x=0;x<W;++x) {
    const auto wa=cross(b.position,c.position,2*x+1,2*y+1), wb=cross(c.position,a.position,2*x+1,2*y+1), wc=cross(a.position,b.position,2*x+1,2*y+1);
    if(!accept(wa,b.position,c.position)||!accept(wb,c.position,a.position)||!accept(wc,a.position,b.position)) continue;
    const auto z=std::uint16_t((wa*a.depth+wb*b.depth+wc*c.depth+area/2)/area); const auto pos=y*W+x;
    if(z<depths[pos]) { depths[pos]=z; colors[pos]=t.color; }
   }
   ++cases;
  }
  if(!s->execute(commands)||!std::equal(colors.begin(),colors.end(),s->pixels().begin())||!std::equal(depths.begin(),depths.end(),s->depths().begin())) { std::cerr<<"depth scene mismatch "<<scene<<'\n'; return 1; }
 }
 std::cout<<cases<<" nondegenerate triangles in 2000 depth scenes compared with full-screen reference\n";
}
