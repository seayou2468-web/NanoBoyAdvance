// iOS Compatibility Layer for Settings
// Emulator configuration cstubs
#pragma once

#include <cstdint>
#include <array>
#include <string>

namespace Settings {

// Basic emulator settings
struct EmuSettings {
    bool use_big_endian = false;
    bool use_extended_logging = false;
    int cpu_core_count = 2;
};

struct ValuesSettings {
    struct ScalarSetting {
        int value = 100;
        int GetValue() const {
            return value;
        }
        operator bool() const {
            return value != 0;
        }
    };

    bool deterministic_async_operations = false;
    struct InputProfile {
        std::array<std::string, 32> buttons{};
        std::array<std::string, 8> analogs{};
        std::string motion_device{};
        std::string touch_device{};
        bool use_touchpad = false;
        std::string controller_touch_device{};
        bool use_touch_from_button = false;
    } current_input_profile{};
    ScalarSetting factor_3d{};
    ScalarSetting volume{};
    ScalarSetting is_new_3ds{1};
    bool lle_applets = false;
};

namespace NativeButton {
enum : std::size_t {
    BUTTON_HID_BEGIN = 0,
    BUTTON_HID_END = 16,
    NUM_BUTTONS_HID = 16,
    A = 0,
    B = 1,
    Select = 2,
    Start = 3,
    Right = 4,
    Left = 5,
    Up = 6,
    Down = 7,
    R = 8,
    L = 9,
    X = 10,
    Y = 11,
    Debug = 12,
    Gpio14 = 13,
    Home = 16,
    Power = 17,
    ZL = 18,
    ZR = 19,
};
}

namespace NativeAnalog {
enum : std::size_t {
    CirclePad = 0,
    CStick = 1,
};
}

// Global settings instance
static EmuSettings g_settings;
static ValuesSettings values;

// Helper functions
inline bool IsBigEndian() {
    return g_settings.use_big_endian;
}

inline bool IsExtendedLoggingEnabled() {
    return g_settings.use_extended_logging;
}

} // namespace Settings
