#include "core/machine.h"
#include "graphics/surface.h"
#include "input/input.h"
#include "runtime/timer_queue.h"
#include "storage/storage.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace nightfall;
using namespace nightfall::graphics;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; return 1; } } while (0)
int main() {
    // All content and IDs are original synthetic fixtures, not game software.
    input::Mapper input;
    CHECK(input.connect(7)); CHECK(input.bind("tick",42)); CHECK(input.set_button(7,42,true));
    runtime::TimerQueue timer(3);
    auto event=timer.schedule(5,77); CHECK(event.status==runtime::ScheduleStatus::ok);
    CHECK(timer.advance_to(4,4).events.empty());
    const auto ready=timer.advance_to(5,1);
    CHECK(ready.events.size()==1 && ready.events[0].id==event.id && ready.events[0].tag==77);
    CHECK(input.active("tick"));
    // mov rax,40; add rax,2; hlt; only executable after setup.
    const std::array<std::uint8_t,17> program{0x48,0xb8,40,0,0,0,0,0,0,0,0x48,0x05,2,0,0,0,0xf4};
    Machine machine(32); CHECK(machine.memory.write(0,program));
    CHECK(machine.memory.protect(0,program.size(),Permission::read | Permission::execute));
    CHECK(!machine.memory.write(0,program)); CHECK(machine.run(3)==RunResult::halted);
    CHECK(machine.cpu.state.rax==42);
    auto surface=Surface::create(4,3,12); CHECK(surface.has_value());
    const Pixel bg{0,0,0,255}, foreground{static_cast<std::uint8_t>(machine.cpu.state.rax),3,4,255};
    const std::array<Command,2> commands{Clear{bg},Fill{1,1,2,1,foreground}};
    CHECK(surface->execute(commands));
    CHECK(surface->pixels()[0]==bg && surface->pixels()[5]==foreground && surface->pixels()[6]==foreground);
    std::vector<std::uint8_t> rgba;
    for (const auto& pixel:surface->pixels()) {
        rgba.push_back(pixel.r); rgba.push_back(pixel.g); rgba.push_back(pixel.b); rgba.push_back(pixel.a);
    }
    storage::Crc32 crc; crc.update(rgba);
    CHECK(crc.value()==0xd1a66520u);
    CHECK(input.disconnect(7) && !input.active("tick"));
    return 0;
}
