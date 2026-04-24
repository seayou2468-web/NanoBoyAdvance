#pragma once

#include <memory>
#include <tuple>
#include <utility>

#include "common/vector_math.h"

namespace Input {

class ButtonDevice {
public:
    bool GetStatus() const { return false; }
};

class AnalogDevice {
public:
    std::pair<float, float> GetStatus() const { return {0.0f, 0.0f}; }
};

class MotionDevice {
public:
    std::pair<Common::Vec3<float>, Common::Vec3<float>> GetStatus() const { return {}; }
};

class TouchDevice {
public:
    std::tuple<unsigned, unsigned, bool> GetStatus() const { return {0, 0, false}; }
};

template <typename Device, typename Param>
std::unique_ptr<Device> CreateDevice(const Param&) {
    return std::make_unique<Device>();
}

template <typename Device>
std::unique_ptr<Device> CreateDevice() {
    return std::make_unique<Device>();
}

} // namespace Input
