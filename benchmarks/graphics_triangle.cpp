#include "graphics/surface.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
using namespace nightfall::graphics;
int main() {
    auto surface=Surface::create(320,180,320*180); if(!surface) return 1;
    const Pixel red{220,40,60,255};
    const std::array<Command,2> batch{Clear{Pixel{0,0,0,255}},Triangle{{20,10},{300,20},{100,170},red}};
    constexpr std::size_t iterations=1000;
    const auto start=std::chrono::steady_clock::now();
    for(std::size_t i=0;i<iterations;++i) if(!surface->execute(batch)) return 1;
    const auto nanos=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    if(!(surface->pixels()[50*320+100]==red) || surface->pixels()[0]==red) return 1;
    std::cout<<"CPU triangle plus clear, 320x180 RGBA8, "<<iterations<<" batches\n"
             <<"Elapsed ns: "<<nanos<<"\nNanoseconds per batch: "<<double(nanos)/iterations<<'\n';
}
