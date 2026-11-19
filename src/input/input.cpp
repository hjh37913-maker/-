#include "input/input.h"
#include <algorithm>
namespace nightfall::input {
bool Mapper::connect(DeviceId device) { return devices_.try_emplace(device).second; }
bool Mapper::disconnect(DeviceId device) { return devices_.erase(device) != 0; }
bool Mapper::set_button(DeviceId device, ControlId control, bool pressed) {
    auto it = devices_.find(device);
    if (it == devices_.end()) return false;
    if (pressed) it->second.insert(control);
    else it->second.erase(control);
    return true;
}
bool Mapper::bind(std::string_view action, ControlId control) {
    if (action.empty()) return false;
    return bindings_[std::string(action)].insert(control).second;
}
bool Mapper::unbind(std::string_view action, ControlId control) {
    auto it = bindings_.find(action);
    if (it == bindings_.end() || !it->second.erase(control)) return false;
    if (it->second.empty()) bindings_.erase(it);
    return true;
}
bool Mapper::active(std::string_view action) const {
    const auto it = bindings_.find(action);
    if (it == bindings_.end()) return false;
    for (const auto& [device, pressed] : devices_) {
        (void)device;
        for (auto button : pressed) if (it->second.contains(button)) return true;
    }
    return false;
}
InputStats Mapper::stats() const noexcept {
    InputStats result{devices_.size(), 0, bindings_.size(), 0};
    for (const auto& entry : devices_) result.pressed_buttons += entry.second.size();
    for (const auto& entry : bindings_) result.bindings += entry.second.size();
    return result;
}
}
