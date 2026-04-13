// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/arm/exclusive_monitor.h"
#include "core/memory.h"

namespace Core {

ExclusiveMonitor::~ExclusiveMonitor() = default;

std::unique_ptr<Core::ExclusiveMonitor> MakeExclusiveMonitor(Memory::MemorySystem& /*memory*/,
                                                             std::size_t /*num_cores*/) {
    // JIT was removed; keep interpreter-only path.
    return nullptr;
}

} // namespace Core
