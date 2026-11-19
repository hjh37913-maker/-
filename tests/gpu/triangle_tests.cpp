#include "graphics/surface.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall::graphics;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
 const Pixel red{255,0,0,255}, blue{0,0,255,255}, clear{0,0,0,0};
 auto a=Surface::create(4,4,16), b=Surface::create(4,4,16), both=Surface::create(4,4,16); CHECK(a&&b&&both);
 Triangle t0{{0,0},{4,0},{0,4},red},t1{{4,0},{4,4},{0,4},blue};
 const std::array<Command,1> c0{t0},c1{t1}; const std::array<Command,2> combined{t0,t1};
 CHECK(a->execute(c0)&&b->execute(c1)&&both->execute(combined));
 // Shared diagonal uses top-left inclusion: exactly one covers each center.
 for(unsigned i=0;i<16;++i) { CHECK((a->pixels()[i]!=clear)!=(b->pixels()[i]!=clear)); CHECK(both->pixels()[i]!=clear); }
 auto reverse=Surface::create(4,4,16); CHECK(reverse); const std::array<Command,1> winding{Triangle{t0.a,t0.c,t0.b,red}};
 CHECK(reverse->execute(winding) && std::equal(a->pixels().begin(),a->pixels().end(),reverse->pixels().begin()));
 // Clipping of triangles extending outside the image, and degenerate triangles.
 auto clip=Surface::create(4,4,16); CHECK(clip); const std::array<Command,1> clipped{Triangle{{-4,-4},{8,-4},{-4,8},red}};
 CHECK(clip->execute(clipped)); CHECK(clip->pixels()[0]==red && clip->pixels()[15]==clear);
 auto before=std::vector<Pixel>(clip->pixels().begin(),clip->pixels().end()); const std::array<Command,2> degenerate{Triangle{{0,0},{1,1},{2,2},blue},Triangle{{1,1},{1,1},{1,1},blue}};
 CHECK(clip->execute(degenerate) && std::equal(before.begin(),before.end(),clip->pixels().begin()));
 // Invalid vertex rejects entire batch without applying earlier commands.
 const std::array<Command,2> invalid{Clear{blue},Triangle{{0,0},{32768,0},{0,2},red}};
 CHECK(!clip->execute(invalid) && std::equal(before.begin(),before.end(),clip->pixels().begin()));
 const std::array<Command,1> extreme{Triangle{{-32768,-32768},{32767,-32768},{-32768,32767},red}};
 CHECK(clip->execute(extreme));
 // Deterministic cross-check against a full-surface, per-pixel barycentric oracle.
 // This intentionally does not use the production bounding-box traversal.
 std::uint64_t seed=0x1234567811abcdefULL;
 auto random=[&]() { seed^=seed<<13; seed^=seed>>7; seed^=seed<<17; return seed; };
 for(unsigned k=0;k<10000;++k) {
   auto point=[&]() { return Point{static_cast<std::int32_t>(random()%17)-6,static_cast<std::int32_t>(random()%17)-6}; };
   Triangle tri{point(),point(),point(),red}; auto surface=Surface::create(4,4,16); CHECK(surface);
   const std::array<Command,1> commands{tri}; CHECK(surface->execute(commands));
   auto det=[](Point p,Point q,Point r) { return std::int64_t(q.x-p.x)*(r.y-p.y)-std::int64_t(q.y-p.y)*(r.x-p.x); };
   auto orient=det(tri.a,tri.b,tri.c); auto p=tri.a,q=tri.b,r=tri.c;
   if(orient<0) std::swap(q,r);
   auto accepted=[](Point u,Point v,int x,int y) {
     const auto value=std::int64_t(v.x-u.x)*(2*y+1-2*u.y)-std::int64_t(v.y-u.y)*(2*x+1-2*u.x);
     return value>0 || (value==0 && (v.y<u.y || (v.y==u.y && v.x>u.x)));
   };
   for(int y=0;y<4;++y) for(int x=0;x<4;++x) {
     const bool inside=orient && accepted(p,q,x,y)&&accepted(q,r,x,y)&&accepted(r,p,x,y);
     CHECK(surface->pixels()[y*4+x]==(inside?red:clear));
   }
 }
 std::cout<<"triangle coverage, adjacency, clipping, winding, transactional and 10000 full-surface comparisons passed\n";
}
