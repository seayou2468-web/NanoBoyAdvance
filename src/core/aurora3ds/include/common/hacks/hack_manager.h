#pragma once

namespace Common::Hacks {

enum class HackType {
    RIGHT_EYE_DISABLE,
    ALLOW_SYSTEM_APP_INSTALL,
    ALLOW_CDN_INSTALL,
    ALLOW_INSTALL_TO_NAND,
    ALLOW_TITLE_DELETE,
};

enum class HackAllowMode {
    DISALLOW,
    ALLOW,
};

class HackManager {
public:
    bool OverrideBooleanSetting([[maybe_unused]] bool current, [[maybe_unused]] const char* key,
                                [[maybe_unused]] bool* from_override = nullptr) const {
        if (from_override) {
            *from_override = false;
        }
        return current;
    }

    HackAllowMode GetHackAllowMode([[maybe_unused]] HackType type,
                                   [[maybe_unused]] HackAllowMode default_mode) const {
        return default_mode;
    }
};

inline HackManager hack_manager{};

} // namespace Common::Hacks
