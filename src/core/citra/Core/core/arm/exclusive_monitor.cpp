// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../../include/core/arm/exclusive_monitor.h"
#include "../../include/core/memory.h"
namespace Core {

ExclusiveMonitor::~ExclusiveMonitor() = default;

std::unique_ptr<Core::ExclusiveMonitor> MakeExclusiveMonitor(Memory::MemorySystem& memory,
                                                             std::size_t num_cores) {
    (void)memory;
    (void)num_cores;
    // TODO(merry): Passthrough exclusive monitor
    return nullptr;
}

} // namespace Core
