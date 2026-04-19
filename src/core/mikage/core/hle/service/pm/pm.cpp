// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "../../../core.h"
#include "pm.h"
#include "pm_app.h"
#include "pm_dbg.h"

namespace Service::PM {

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<PM_APP>(system)->InstallAsService(service_manager);
    std::make_shared<PM_DBG>()->InstallAsService(service_manager);
}

} // namespace Service::PM
