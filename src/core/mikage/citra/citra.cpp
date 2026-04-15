// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "../common/common.h"
#include "../common/log_manager.h"
#include "../common/file_util.h"

#include "../core/system.h"
#include "../core/core.h"
#include "../core/loader.h"

#include "./emu_window/emu_window_ios.h"

#include "citra.h"

/// Application entry point
int __cdecl citra_main(int argc, char **argv) {
    std::string program_dir = File::GetCurrentDir();

    LogManager::Init();

    EmuWindow_IOS* emu_window = new EmuWindow_IOS;

    System::Init(emu_window);

    std::string boot_filename;

    if (argc < 2) {
        ERROR_LOG(BOOT, "Failed to load ROM: No ROM specified");
    }
    else {
        boot_filename = argv[1];
    }
    std::string error_str;

    bool res = Loader::LoadFile(boot_filename, &error_str);

    if (!res) {
        ERROR_LOG(BOOT, "Failed to load ROM: %s", error_str.c_str());
    }

    Core::RunLoop();

    delete emu_window;

    return 0;
}
