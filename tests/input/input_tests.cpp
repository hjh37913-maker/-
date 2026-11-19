#include "input/input.h"
#include <cstdint>
#include <iostream>
using namespace nightfall::input;
static int failures=0;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x " failed\n"; ++failures; } } while (0)
int main() {
    Mapper m; auto s=m.stats(); CHECK(!s.devices && !s.pressed_buttons && !s.actions && !s.bindings);
    CHECK(!m.active("jump")); CHECK(m.connect(0)); CHECK(!m.connect(0));
    CHECK(m.connect(UINT64_MAX)); CHECK(!m.set_button(5,10,true)); CHECK(!m.disconnect(5));
    CHECK(!m.bind("",10)); CHECK(m.bind("jump",10)); CHECK(!m.bind("jump",10));
    CHECK(m.bind("jump",11)); CHECK(m.bind("sprint",10)); CHECK(!m.active("jump"));
    CHECK(m.set_button(0,10,true)); CHECK(m.set_button(0,10,true));
    CHECK(m.active("jump") && m.active("sprint"));
    s=m.stats(); CHECK(s.devices==2 && s.pressed_buttons==1 && s.actions==2 && s.bindings==3);
    CHECK(m.set_button(UINT64_MAX,11,true)); CHECK(m.set_button(0,10,false));
    CHECK(m.active("jump") && !m.active("sprint"));
    CHECK(m.set_button(0,10,true)); CHECK(m.disconnect(UINT64_MAX)); CHECK(m.active("jump"));
    CHECK(m.disconnect(0)); CHECK(!m.active("jump") && !m.active("sprint"));
    CHECK(m.connect(0)); CHECK(!m.active("jump")); // reconnect starts neutral
    CHECK(m.set_button(0,11,true)); CHECK(m.active("jump"));
    CHECK(m.unbind("jump",11)); CHECK(!m.active("jump")); CHECK(!m.unbind("jump",11));
    CHECK(m.bind("jump",11)); CHECK(m.active("jump")); // binding changes apply immediately
    CHECK(m.unbind("jump",10)); CHECK(m.unbind("jump",11)); CHECK(!m.active("jump"));
    s=m.stats(); CHECK(s.actions==1 && s.bindings==1 && s.devices==1 && s.pressed_buttons==1);
    CHECK(m.set_button(0,11,false)); CHECK(m.set_button(0,11,false));
    CHECK(m.unbind("sprint",10)); s=m.stats(); CHECK(!s.pressed_buttons && !s.actions && !s.bindings);
    CHECK(m.bind("case",UINT32_MAX)); CHECK(m.set_button(0,UINT32_MAX,true));
    CHECK(m.active("case") && !m.active("Case")); CHECK(m.disconnect(0));
    CHECK(m.stats().pressed_buttons==0 && !m.active("case"));
    return failures ? 1 : 0;
}
