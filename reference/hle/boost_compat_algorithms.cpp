// Copyright Aurora Project
// Licensed under GPLv2 or any later version

#include "boost_compat.h"

namespace HLE::BoostCompat {

void ReplaceAllInPlace(std::string& input, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return;
    }

    std::size_t pos = 0;
    while ((pos = input.find(from, pos)) != std::string::npos) {
        input.replace(pos, from.size(), to);
        pos += to.size();
    }
}

} // namespace HLE::BoostCompat
