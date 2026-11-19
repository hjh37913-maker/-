#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace nightfall::input {
using DeviceId = std::uint64_t;
using ControlId = std::uint32_t;
struct InputStats { std::size_t devices, pressed_buttons, actions, bindings; };
// Platform-neutral digital button aggregation. No host device detection is included.
// Callers serialize all access. IDs and action names are application-defined.
class Mapper {
public:
    bool connect(DeviceId device);
    bool disconnect(DeviceId device);
    bool set_button(DeviceId device, ControlId control, bool pressed);
    bool bind(std::string_view action, ControlId control);
    bool unbind(std::string_view action, ControlId control);
    bool active(std::string_view action) const;
    InputStats stats() const noexcept;
private:
    std::map<DeviceId, std::set<ControlId>> devices_;
    std::map<std::string, std::set<ControlId>, std::less<>> bindings_;
};
}
