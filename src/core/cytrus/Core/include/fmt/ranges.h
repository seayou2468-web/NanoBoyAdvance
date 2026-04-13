#pragma once

#include <sstream>
#include <string>
#include "fmt/core.h"

namespace fmt {

template <typename Range>
[[nodiscard]] inline std::string join(const Range& range, std::string_view separator) {
    std::ostringstream oss;
    bool first = true;
    for (const auto& value : range) {
        if (!first) {
            oss << separator;
        }
        first = false;
        oss << value;
    }
    return oss.str();
}

} // namespace fmt
