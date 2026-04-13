// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "../../../../include/core/core.h"
#include "../../../../include/core/hle/service/mcu/mcu.h"
#include "../../../../include/core/hle/service/mcu/mcu_hwc.h"
namespace Service::MCU {

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<HWC>()->InstallAsService(service_manager);
}

} // namespace Service::MCU
