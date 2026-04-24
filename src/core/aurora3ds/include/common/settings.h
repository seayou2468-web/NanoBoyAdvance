#pragma once

#include <array>
#include <string>
#include <unordered_map>

struct EmulatorConfig;

namespace Settings {

template <typename T>
class Setting {
public:
    constexpr Setting() = default;
    constexpr Setting(const T& value_) : value(value_) {}

    constexpr const T& GetValue() const {
        return value;
    }

    constexpr void SetValue(const T& new_value) {
        value = new_value;
    }

    constexpr operator const T&() const {
        return value;
    }

    constexpr Setting& operator=(const T& new_value) {
        value = new_value;
        return *this;
    }

private:
    T value{};
};

enum NativeButton {
    A = 0,
    B,
    Select,
    Start,
    Right,
    Left,
    Up,
    Down,
    R,
    L,
    X,
    Y,
    Debug,
    Gpio14,
    ZL,
    ZR,
    Home,
    Power,
    NumButtons,
    BUTTON_HID_BEGIN = A,
    BUTTON_HID_END = Gpio14 + 1,
    NUM_BUTTONS_HID = BUTTON_HID_END,
};

enum NativeAnalog {
    CirclePad = 0,
    CStick,
    NumAnalogs,
};

enum class InitClock {
    SystemTime = 0,
    FixedTime = 1,
};

constexpr int REGION_VALUE_AUTO_SELECT = -1;

struct InputProfile {
    std::array<std::string, NativeButton::NumButtons> buttons{};
    std::array<std::string, NativeAnalog::NumAnalogs> analogs{};
    std::string motion_device{};
    std::string touch_device{};
    std::string controller_touch_device{};
    bool use_touchpad = false;
    bool use_touch_from_button = false;
};

struct Values {
    Setting<bool> is_new_3ds{true};
    Setting<bool> use_virtual_sd{true};
    Setting<float> volume{1.0f};
    Setting<int> factor_3d{0};
    Setting<double> frame_limit{100.0};
    Setting<bool> deterministic_async_operations{false};
    Setting<bool> compress_cia_installs{false};
    Setting<bool> toggle_unique_data_console_type{false};
    Setting<bool> enable_required_online_lle_modules{false};
    std::unordered_map<std::string, bool> lle_modules{};
    Setting<bool> lle_applets{false};
    Setting<int> region_value{REGION_VALUE_AUTO_SELECT};
    Setting<unsigned short> steps_per_hour{0};
    Setting<unsigned int> delay_game_render_thread_us{0};
    Setting<bool> delay_start_for_lle_modules{false};
    Setting<InitClock> init_clock{InitClock::SystemTime};
    Setting<long long> init_time{0};
    Setting<long long> init_time_offset{0};
    Setting<std::string> input_type{"auto"};
    Setting<std::string> input_device{""};
    Setting<bool> plugin_loader_enabled{false};

    InputProfile current_input_profile{};
    std::array<std::string, 3> camera_name{};
    std::array<std::string, 3> camera_config{};
    std::array<int, 3> camera_flip{};
};

inline Values values{};
inline bool is_temporary_frame_limit = false;
inline double temporary_frame_limit = 0.0;

void ApplyFromConfig(const EmulatorConfig& config);
void ApplyToConfig(EmulatorConfig& config);

} // namespace Settings
