//
//  Configuration.mm
//  Folium-iOS
//
//  Created by Jarrod Norwell on 25/7/2024.
//

#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <inih/INIReader.h>
#include "common/file_util.h"
#include "common/param_package.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/service.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"
#include "network/network_settings.h"

#import "Configuration.h"
#import "DefaultINI.h"
#import "InputManager.h"

Configuration::Configuration() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    core_config_loc = FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "config.ini";
    std::string ini_buffer;
    FileUtil::ReadFileToString(true, core_config_loc, ini_buffer);
    if (!ini_buffer.empty()) {
        core_config = std::make_unique<INIReader>(ini_buffer.c_str(), ini_buffer.size());
    }

    Reload();
}

Configuration::~Configuration() = default;

bool Configuration::LoadINI(const std::string& default_contents, bool retry) {
    const std::string& location = this->core_config_loc;
    if (core_config == nullptr || core_config->ParseError() < 0) {
        if (retry) {
            FileUtil::CreateFullPath(location);
            FileUtil::WriteStringToFile(true, location, default_contents);
            std::string ini_buffer;
            FileUtil::ReadFileToString(true, location, ini_buffer);
            core_config =
                std::make_unique<INIReader>(ini_buffer.c_str(), ini_buffer.size()); // Reopen file
            return LoadINI(default_contents, false);
        }
        return false;
    }
    return true;
}

static const std::array<int, Settings::NativeButton::NumButtons> default_buttons = {
    InputManager::N3DS_BUTTON_A,     InputManager::N3DS_BUTTON_B,
    InputManager::N3DS_BUTTON_X,     InputManager::N3DS_BUTTON_Y,
    InputManager::N3DS_DPAD_UP,      InputManager::N3DS_DPAD_DOWN,
    InputManager::N3DS_DPAD_LEFT,    InputManager::N3DS_DPAD_RIGHT,
    InputManager::N3DS_TRIGGER_L,    InputManager::N3DS_TRIGGER_R,
    InputManager::N3DS_BUTTON_START, InputManager::N3DS_BUTTON_SELECT,
    InputManager::N3DS_BUTTON_DEBUG, InputManager::N3DS_BUTTON_GPIO14,
    InputManager::N3DS_BUTTON_ZL,    InputManager::N3DS_BUTTON_ZR,
    InputManager::N3DS_BUTTON_HOME,
};

static const std::array<int, Settings::NativeAnalog::NumAnalogs> default_analogs{{
    InputManager::N3DS_CIRCLEPAD,
    InputManager::N3DS_STICK_C,
}};

template <>
void Configuration::ReadSetting(const std::string& group, Settings::Setting<std::string>& setting) {
    std::string setting_value = core_config->Get(group, setting.GetLabel(), setting.GetDefault());
    if (setting_value.empty()) {
        setting_value = setting.GetDefault();
    }
    setting = std::move(setting_value);
}

template <>
void Configuration::ReadSetting(const std::string& group, Settings::Setting<bool>& setting) {
    setting = core_config->GetBoolean(group, setting.GetLabel(), setting.GetDefault());
}

template <typename Type, bool ranged>
void Configuration::ReadSetting(const std::string& group, Settings::Setting<Type, ranged>& setting) {
    if constexpr (std::is_floating_point_v<Type>) {
        setting = core_config->GetReal(group, setting.GetLabel(), setting.GetDefault());
    } else {
        setting = static_cast<Type>(core_config->GetInteger(
            group, setting.GetLabel(), static_cast<long>(setting.GetDefault())));
    }
}

void Configuration::ReadValues() {
    // Controls
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        std::string default_param = InputManager::GenerateButtonParamPackage(default_buttons[i]);
        Settings::values.current_input_profile.buttons[i] =
            core_config->GetString("Controls", Settings::NativeButton::mapping[i], default_param);
        if (Settings::values.current_input_profile.buttons[i].empty())
            Settings::values.current_input_profile.buttons[i] = default_param;
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        std::string default_param = InputManager::GenerateAnalogParamPackage(default_analogs[i]);
        Settings::values.current_input_profile.analogs[i] =
            core_config->GetString("Controls", Settings::NativeAnalog::mapping[i], default_param);
        if (Settings::values.current_input_profile.analogs[i].empty())
            Settings::values.current_input_profile.analogs[i] = default_param;
    }

    Settings::values.current_input_profile.motion_device = core_config->GetString(
        "Controls", "motion_device",
        "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0");
    Settings::values.current_input_profile.touch_device =
        core_config->GetString("Controls", "touch_device", "engine:emu_window");
    Settings::values.current_input_profile.udp_input_address = core_config->GetString(
        "Controls", "udp_input_address", InputCommon::CemuhookUDP::DEFAULT_ADDR);
    Settings::values.current_input_profile.udp_input_port =
        static_cast<u16>(core_config->GetInteger("Controls", "udp_input_port",
                                                 InputCommon::CemuhookUDP::DEFAULT_PORT));

    // Core execution mode is fixed to interpreter-only.
    Settings::values.use_cpu_jit = false;
    ReadSetting("Core", Settings::values.cpu_clock_percentage);

    // Renderer
    Settings::values.use_gles = false;
    Settings::values.shaders_accurate_mul = false;
    ReadSetting("Renderer", Settings::values.graphics_api);
    ReadSetting("Renderer", Settings::values.async_presentation);
    ReadSetting("Renderer", Settings::values.disable_right_eye_render);
    ReadSetting("Renderer", Settings::values.async_shader_compilation);
    ReadSetting("Renderer", Settings::values.spirv_shader_gen);
    ReadSetting("Renderer", Settings::values.use_hw_shader);
    ReadSetting("Renderer", Settings::values.resolution_factor);
    ReadSetting("Renderer", Settings::values.use_disk_shader_cache);
    ReadSetting("Renderer", Settings::values.use_vsync_new);
    ReadSetting("Renderer", Settings::values.texture_filter);
    ReadSetting("Renderer", Settings::values.texture_sampling);
    ReadSetting("Renderer", Settings::values.mono_render_option);

    // Software-only renderer policy for iOS bridge.
    Settings::values.graphics_api = Settings::GraphicsAPI::Software;
    Settings::values.use_hw_shader = false;
    Settings::values.use_shader_jit = false;
    Settings::values.async_shader_compilation = false;
    Settings::values.spirv_shader_gen = false;
    Settings::values.use_disk_shader_cache = false;

    // Work around to map Android setting for enabling the frame limiter to the format Cytrus
    // expects
    if (core_config->GetBoolean("Renderer", "use_frame_limit", true)) {
        ReadSetting("Renderer", Settings::values.frame_limit);
    } else {
        Settings::values.frame_limit = 0;
    }

    ReadSetting("Renderer", Settings::values.render_3d);
    ReadSetting("Renderer", Settings::values.factor_3d);
    std::string default_shader = "none (builtin)";
    if (Settings::values.render_3d.GetValue() == Settings::StereoRenderOption::Anaglyph)
        default_shader = "dubois (builtin)";
    else if (Settings::values.render_3d.GetValue() == Settings::StereoRenderOption::Interlaced)
        default_shader = "horizontal (builtin)";
    Settings::values.pp_shader_name =
        core_config->GetString("Renderer", "pp_shader_name", default_shader);
    ReadSetting("Renderer", Settings::values.filter_mode);

    ReadSetting("Renderer", Settings::values.bg_red);
    ReadSetting("Renderer", Settings::values.bg_green);
    ReadSetting("Renderer", Settings::values.bg_blue);

    // Layout
    Settings::values.layout_option = static_cast<Settings::LayoutOption>(core_config->GetInteger(
        "Layout", "layout_option", static_cast<int>(Settings::LayoutOption::MobileLandscape)));
    ReadSetting("Layout", Settings::values.custom_layout);
    ReadSetting("Layout", Settings::values.custom_top_left);
    ReadSetting("Layout", Settings::values.custom_top_top);
    ReadSetting("Layout", Settings::values.custom_top_right);
    ReadSetting("Layout", Settings::values.custom_top_bottom);
    ReadSetting("Layout", Settings::values.custom_bottom_left);
    ReadSetting("Layout", Settings::values.custom_bottom_top);
    ReadSetting("Layout", Settings::values.custom_bottom_right);
    ReadSetting("Layout", Settings::values.custom_bottom_bottom);
    ReadSetting("Layout", Settings::values.cardboard_screen_size);
    ReadSetting("Layout", Settings::values.cardboard_x_shift);
    ReadSetting("Layout", Settings::values.cardboard_y_shift);

    // Utility
    ReadSetting("Utility", Settings::values.dump_textures);
    ReadSetting("Utility", Settings::values.custom_textures);
    ReadSetting("Utility", Settings::values.preload_textures);
    ReadSetting("Utility", Settings::values.async_custom_loading);

    // Audio
    ReadSetting("Audio", Settings::values.audio_emulation);
    ReadSetting("Audio", Settings::values.enable_audio_stretching);
    ReadSetting("Audio", Settings::values.volume);
    ReadSetting("Audio", Settings::values.output_type);
    ReadSetting("Audio", Settings::values.output_device);
    ReadSetting("Audio", Settings::values.input_type);
    ReadSetting("Audio", Settings::values.input_device);
    ReadSetting("Audio", Settings::values.enable_realtime_audio);

    // Data Storage
    ReadSetting("Data Storage", Settings::values.use_virtual_sd);

    // System
    ReadSetting("System", Settings::values.is_new_3ds);
    ReadSetting("System", Settings::values.lle_applets);
    ReadSetting("System", Settings::values.region_value);
    ReadSetting("System", Settings::values.init_clock);
    {
        std::string time = core_config->GetString("System", "init_time", "946681277");
        try {
            Settings::values.init_time = std::stoll(time);
        } catch (...) {
        }
    }
    ReadSetting("System", Settings::values.init_ticks_type);
    ReadSetting("System", Settings::values.init_ticks_override);
    ReadSetting("System", Settings::values.plugin_loader_enabled);
    ReadSetting("System", Settings::values.allow_plugin_loader);
    ReadSetting("System", Settings::values.steps_per_hour);

    // Camera
    using namespace Service::CAM;
    Settings::values.camera_name[OuterRightCamera] =
        core_config->GetString("Camera", "camera_outer_right_name", "ndk");
    Settings::values.camera_config[OuterRightCamera] = core_config->GetString(
        "Camera", "camera_outer_right_config", std::string{"av_rear"});
    Settings::values.camera_flip[OuterRightCamera] =
        core_config->GetInteger("Camera", "camera_outer_right_flip", 0);
    Settings::values.camera_name[InnerCamera] =
        core_config->GetString("Camera", "camera_inner_name", "ndk");
    Settings::values.camera_config[InnerCamera] = core_config->GetString(
        "Camera", "camera_inner_config", std::string{"av_front"});
    Settings::values.camera_flip[InnerCamera] =
        core_config->GetInteger("Camera", "camera_inner_flip", 0);
    Settings::values.camera_name[OuterLeftCamera] =
        core_config->GetString("Camera", "camera_outer_left_name", "ndk");
    Settings::values.camera_config[OuterLeftCamera] = core_config->GetString(
        "Camera", "camera_outer_left_config", std::string{"av_rear"});
    Settings::values.camera_flip[OuterLeftCamera] =
        core_config->GetInteger("Camera", "camera_outer_left_flip", 0);

    // Miscellaneous + debug/logging features are removed in this bridge build.
    Settings::values.log_filter = "*:Critical";
    Settings::values.record_frame_times = false;
    Settings::values.renderer_debug = false;
    Settings::values.use_gdbstub = false;
    Settings::values.gdbstub_port = 0;

    // Web Service
    NetSettings::values.enable_telemetry =
        core_config->GetBoolean("WebService", "enable_telemetry", false);
    NetSettings::values.web_api_url =
        core_config->GetString("WebService", "web_api_url", "https://manicemu.site");//Manic修改
    NetSettings::values.manicemu_username =
        core_config->GetString("WebService", "manic_username", "");//Manic修改
    NetSettings::values.manicemu_token = core_config->GetString("WebService", "manic_token", "");//Manic修改
}

void Configuration::Reload() {
    LoadINI(DefaultINI::core_config_file);
    ReadValues();
}
