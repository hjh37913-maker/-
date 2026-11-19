#include "graphics/surface.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
using namespace nightfall::graphics;
int main() {
    auto surface=Surface::create(256,256,256*256);
    if (!surface) return 1;
    const std::array<Command,3> batch{Clear{Pixel{1,2,3,255}},
        Fill{32,32,128,128,Pixel{4,5,6,255}},Copy{32,32,96,96,128,128}};
    constexpr std::size_t repetitions=10000;
    const auto start=std::chrono::steady_clock::now();
    for (std::size_t i=0;i<repetitions;++i) if (!surface->execute(batch)) return 1;
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    if (!(surface->pixels()[0]==Pixel{1,2,3,255}) || !(surface->pixels()[96*256+96]==Pixel{4,5,6,255})) return 1;
    std::cout << "CPU software graphics: " << repetitions << " batches (clear, fill, copy), 256x256 RGBA8\n"
              << "Elapsed ns: " << ns << "\nNanoseconds per batch: " << static_cast<double>(ns)/repetitions << '\n';
}
