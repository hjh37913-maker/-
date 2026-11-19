#include "input/input.h"
#include <chrono>
#include <cstddef>
#include <iostream>
int main() {
    nightfall::input::Mapper m;
    if (!m.connect(1) || !m.connect(2) || !m.bind("jump",7) || !m.bind("jump",9)) return 1;
    constexpr std::size_t iterations=1000000;
    std::size_t active=0;
    const auto start=std::chrono::steady_clock::now();
    for (std::size_t i=0;i<iterations;++i) {
        if (!m.set_button(1,7,(i & 1)==0)) return 1;
        active += m.active("jump");
    }
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    if (active!=iterations/2) return 1;
    std::cout << "Synthetic input: " << iterations << " button updates + action queries\nElapsed ns: " << ns
              << "\nActive observations: " << active << '\n';
}
