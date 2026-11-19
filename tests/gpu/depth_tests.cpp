#include "graphics/depth_surface.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>
using namespace nightfall::graphics;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
 CHECK(!DepthSurface::create(0,1,4) && !DepthSurface::create(4,4,15) && !DepthSurface::create(UINT32_MAX,UINT32_MAX,1));
 const Pixel clear{0,0,0,255}, red{255,0,0,255},blue{0,0,255,255}, green{0,255,0,255};
 auto s=DepthSurface::create(4,4,16); CHECK(s && s->depths().size()==16);
 // Two geometrically identical triangles: closer wins regardless of submission order.
 const DepthTriangle far{{{0,0},500},{{4,0},500},{{0,4},500},red};
 const DepthTriangle near{{{0,0},100},{{4,0},100},{{0,4},100},blue};
 const std::array<DepthCommand,3> commands{DepthClear{clear,65535},far,near}; CHECK(s->execute(commands));
 CHECK(s->pixels()[0]==blue && s->depths()[0]==100 && s->pixels()[15]==clear && s->depths()[15]==65535);
 const auto before=std::vector<Pixel>(s->pixels().begin(),s->pixels().end()); const auto depth_before=std::vector<std::uint16_t>(s->depths().begin(),s->depths().end());
 const std::array<DepthCommand,3> reverse{DepthClear{clear,65535},near,far}; CHECK(s->execute(reverse)); CHECK(std::equal(before.begin(),before.end(),s->pixels().begin()) && std::equal(depth_before.begin(),depth_before.end(),s->depths().begin()));
 // Equal depths fail LESS. Winding reversal preserves interpolation.
 const DepthTriangle tied{{{0,0},100},{{0,4},100},{{4,0},100},green}; const std::array<DepthCommand,1> tie{tied}; CHECK(s->execute(tie)); CHECK(s->pixels()[0]==blue);
 // Barycentric interpolation at center (0.5,0.5): 75% of 0 + 12.5% of 400 + 12.5% of 800 = 150.
 const DepthTriangle gradient{{{0,0},0},{{4,0},400},{{0,4},800},green};
 const std::array<DepthCommand,2> grad{DepthClear{clear,65535},gradient}; CHECK(s->execute(grad)); CHECK(s->depths()[0]==150 && s->pixels()[0]==green);
 // A whole invalid batch leaves both buffers intact.
 const auto saved=std::vector<Pixel>(s->pixels().begin(),s->pixels().end()); const auto saved_depth=std::vector<std::uint16_t>(s->depths().begin(),s->depths().end());
 const std::array<DepthCommand,2> invalid{DepthClear{blue,0},DepthTriangle{{{0,0},1},{{32768,0},2},{{0,3},3},red}};
 CHECK(!s->execute(invalid) && std::equal(saved.begin(),saved.end(),s->pixels().begin()) && std::equal(saved_depth.begin(),saved_depth.end(),s->depths().begin()));
 const std::array<DepthCommand,1> degenerate{DepthTriangle{{{1,1},1},{{2,2},2},{{3,3},3},red}};
 CHECK(s->execute(degenerate) && std::equal(saved.begin(),saved.end(),s->pixels().begin()) && std::equal(saved_depth.begin(),saved_depth.end(),s->depths().begin()));
 DepthSurface moved=std::move(*s); CHECK(s->width()==0 && s->height()==0 && s->pixels().empty() && s->depths().empty());
 CHECK(moved.width()==4 && moved.pixels()[0]==green); *s=std::move(moved); CHECK(moved.width()==0 && moved.depths().empty() && s->width()==4);
 std::cout<<"depth raster order, interpolation, faults, move and clear tests passed\n";
}
