// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/sys/service/cfg/cfg.h"

namespace Service::CFG {

class CFG_I final : public Module::Interface {
public:
    explicit CFG_I(std::shared_ptr<Module> cfg);

private:
    SERVICE_SERIALIZATION(CFG_I, cfg, Module)
};

} // namespace Service::CFG

SERIALIZATION_CLASS_EXPORT_KEY(Service::CFG::CFG_I)
SERIALIZATION_CONSTRUCT(Service::CFG::CFG_I)
