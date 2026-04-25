// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

class Memory;

namespace Applets {

// Transitional compatibility shim.
// Legacy include paths still expect Applets::AppletManager at this location.
// The real manager lives under core/hle/service/apt.
class AppletManager {
public:
    explicit AppletManager(Memory&) {}
};

}  // namespace Applets
