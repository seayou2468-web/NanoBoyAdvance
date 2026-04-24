// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/sys/service/am/am.h"

namespace Service::AM {

class AM_U final : public Module::Interface {
public:
    explicit AM_U(std::shared_ptr<Module> am);

private:
    SERVICE_SERIALIZATION(AM_U, am, Module)
};

} // namespace Service::AM

SERIALIZATION_CLASS_EXPORT_KEY(Service::AM::AM_U)
SERIALIZATION_CONSTRUCT(Service::AM::AM_U)
