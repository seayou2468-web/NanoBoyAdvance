// iOS Compatibility Layer for Settings
// Emulator configuration cstubs
#pragma once

#include <cstdint>
#include <string>

namespace Settings {

// Basic emulator settings
struct EmuSettings {
    bool use_big_endian = false;
    bool use_extended_logging = false;
    int cpu_core_count = 2;
};

// Global settings instance
static EmuSettings g_settings;

// Helper functions
inline bool IsBigEndian() {
    return g_settings.use_big_endian;
}

inline bool IsExtendedLoggingEnabled() {
    return g_settings.use_extended_logging;
}

} // namespace Settings
