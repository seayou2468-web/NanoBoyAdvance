// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string_view>
#include <tuple>

namespace Input {

// Lightweight compatibility subset for aurora3ds header integration.
// Keep this header dependency-free so we can include it from HLE headers
// without pulling additional external/common subsystems.
template <typename StatusType>
class InputDevice {
public:
    virtual ~InputDevice() = default;
    virtual StatusType GetStatus() const {
        return {};
    }
};

template <typename InputDeviceType>
std::unique_ptr<InputDeviceType> CreateDevice(std::string_view) {
    return std::make_unique<InputDeviceType>();
}

using ButtonDevice = InputDevice<bool>;
using AnalogDevice = InputDevice<std::tuple<float, float>>;
using TouchDevice = InputDevice<std::tuple<float, float, bool>>;

}  // namespace Input
