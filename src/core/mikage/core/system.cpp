// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "core.h"
#include "core_timing.h"
#include "mem_map.h"
#include "system.h"
#include "./hw/hw.h"
#include "./hle/hle.h"

#include "../video_core/video_core.h"

namespace System {

volatile State g_state;
MetaFileSystem g_ctr_file_system;

void UpdateState(State state) {
}

void Init(EmuWindow* emu_window) {
    Core::Init();
    Memory::Init();
    HW::Init();
    HLE::Init();
    CoreTiming::Init();
    VideoCore::Init(emu_window);
}

void RunLoopFor(int cycles) {
    if (cycles <= 0) {
        return;
    }
    RunLoopUntil(CoreTiming::GetTicks() + static_cast<u64>(cycles));
}

void RunLoopUntil(u64 global_cycles) {
    while (CoreTiming::GetTicks() < global_cycles) {
        Core::SingleStep();
        CoreTiming::Advance();
        if (VideoCore::g_renderer != nullptr) {
            VideoCore::g_renderer->SwapBuffers();
        }
    }
}

void Shutdown() {
    Core::Shutdown();
    Memory::Shutdown();
    HW::Shutdown();
    HLE::Shutdown();
    CoreTiming::Shutdown();
    VideoCore::Shutdown();
    g_ctr_file_system.Shutdown();
}

} // namespace
