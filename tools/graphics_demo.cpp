#include "graphics/surface.h"
#include <array>
#include <fstream>
#include <iostream>
using namespace nightfall::graphics;
int main(int argc,char** argv) {
    if (argc!=2) { std::cerr << "Usage: nightfall_graphics_demo output.ppm\n"; return 2; }
    auto surface=Surface::create(320,180,320*180);
    if (!surface) return 1;
    const std::array<Command,9> commands{Clear{Pixel{10,16,32,255}},
        Fill{16,16,288,148,Pixel{23,34,55,255}}, Fill{30,30,260,12,Pixel{92,216,206,255}},
        Fill{30,55,115,86,Pixel{49,75,111,255}}, Fill{155,55,135,36,Pixel{43,59,85,255}},
        Fill{155,101,135,40,Pixel{36,48,69,255}},
        Triangle{{43,131},{87,65},{132,131},Pixel{92,216,206,255}},
        Triangle{{87,65},{132,131},{112,131},Pixel{159,235,224,255}},
        Triangle{{167,127},{194,84},{222,127},Pixel{239,189,94,255}}};
    if (!surface->execute(commands)) return 1;
    std::ofstream out(argv[1],std::ios::binary);
    if (!out) { std::cerr << "Cannot open output\n"; return 1; }
    out << "P6\n" << surface->width() << ' ' << surface->height() << "\n255\n";
    for (const auto& p:surface->pixels()) {
        const std::array<char,3> rgb{static_cast<char>(p.r),static_cast<char>(p.g),static_cast<char>(p.b)};
        out.write(rgb.data(),rgb.size());
    }
    out.close();
    if (!out) { std::cerr << "Output write failed\n"; return 1; }
    std::cout << "Wrote original synthetic software-rendered image\n";
}
