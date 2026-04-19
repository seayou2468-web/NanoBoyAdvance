// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "exclusive_monitor.h"

namespace Core {

ExclusiveMonitor::~ExclusiveMonitor() = default;

namespace {

class NoJitExclusiveMonitor final : public ExclusiveMonitor {
public:
    u8 ExclusiveRead8(std::size_t, VAddr) override {
        return 0;
    }
    u16 ExclusiveRead16(std::size_t, VAddr) override {
        return 0;
    }
    u32 ExclusiveRead32(std::size_t, VAddr) override {
        return 0;
    }
    u64 ExclusiveRead64(std::size_t, VAddr) override {
        return 0;
    }
    void ClearExclusive(std::size_t) override {}

    bool ExclusiveWrite8(std::size_t, VAddr, u8) override {
        return true;
    }
    bool ExclusiveWrite16(std::size_t, VAddr, u16) override {
        return true;
    }
    bool ExclusiveWrite32(std::size_t, VAddr, u32) override {
        return true;
    }
    bool ExclusiveWrite64(std::size_t, VAddr, u64) override {
        return true;
    }
};

} // namespace

std::unique_ptr<Core::ExclusiveMonitor> MakeExclusiveMonitor(Memory::MemorySystem& memory,
                                                             std::size_t num_cores) {
    (void)memory;
    (void)num_cores;
    return std::make_unique<NoJitExclusiveMonitor>();
}

} // namespace Core
