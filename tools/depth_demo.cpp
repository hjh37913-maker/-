#include "graphics/depth_surface.h"
#include <array>
#include <fstream>
#include <iostream>
using namespace nightfall::graphics;
int main(int argc,char** argv) {
 if(argc!=2) { std::cerr<<"Usage: nightfall_depth_demo output.ppm\n"; return 2; }
 auto surface=DepthSurface::create(320,180,320*180); if(!surface) return 1;
 const Pixel background{12,18,32,255},far{92,216,206,255},near{239,189,94,255};
 // The nearer yellow triangle is drawn first; the far cyan triangle cannot cover it.
 const std::array<DepthCommand,3> commands{
  DepthClear{background,65535},
  DepthTriangle{{{130,35},8000},{{275,110},8000},{{110,155},8000},near},
  DepthTriangle{{{30,140},40000},{{165,20},40000},{{295,140},40000},far}
 };
 if(!surface->execute(commands) || surface->pixels()[90*320+150]!=near || surface->pixels()[0]!=background) return 1;
 std::ofstream out(argv[1],std::ios::binary); if(!out) return 1;
 out<<"P6\n"<<surface->width()<<' '<<surface->height()<<"\n255\n";
 for(const auto& p:surface->pixels()) { const std::array<char,3> rgb{char(p.r),char(p.g),char(p.b)}; out.write(rgb.data(),rgb.size()); }
 out.close(); if(!out) return 1;
 std::cout<<"Wrote original CPU-rendered depth test image; not a PS5 GPU\n";
}
