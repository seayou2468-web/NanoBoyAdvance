// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "../../../core.h"
#include "gsp.h"
#include "gsp_gpu.h"
#include "gsp_lcd.h"

namespace Service::GSP {

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<GSP_GPU>(system)->InstallAsService(service_manager);
    std::make_shared<GSP_LCD>()->InstallAsService(service_manager);
}

} // namespace Service::GSP
