// Minimal compatibility declarations for ArticBase protocol types.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ArticBaseCommon {

enum class LogOnServerType : std::uint32_t {
    Info = 0,
    Warning = 1,
    Error = 2,
};

struct RequestPacket {
    std::uint32_t request_id{};
    std::uint32_t method_size{};
    std::uint32_t parameter_count{};
};

struct RequestParameter {
    std::uint32_t type{};
    std::uint32_t size{};
    std::uint64_t value{};
};

struct DataPacket {
    std::uint32_t request_id{};
    std::uint32_t data_size{};
};

} // namespace ArticBaseCommon
