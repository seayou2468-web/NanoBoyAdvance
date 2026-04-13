// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace NetSettings {

struct Values {
    // WebService
    bool enable_telemetry;
    std::string web_api_url;
    std::string manicemu_username;//Manic修改
    std::string manicemu_token;//Manic修改
} extern values;

} // namespace NetSettings
