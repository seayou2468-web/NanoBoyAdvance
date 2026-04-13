#include "common/settings.h"
#include "network/network_settings.h"

#import "Configuration.h"

Configuration::Configuration() {
    Reload();
}

Configuration::~Configuration() = default;

void Configuration::ReadValues() {
    // Core
    Settings::values.use_cpu_jit = false;

    // Renderer
    Settings::values.graphics_api = Settings::GraphicsAPI::Software;
    Settings::values.use_gles = false;
    Settings::values.use_hw_shader = false;
    Settings::values.use_shader_jit = false;
    Settings::values.use_disk_shader_cache = false;

    // Input defaults (self-contained, non-SDL backends)
    Settings::values.current_input_profile.motion_device =
        "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0";
    Settings::values.current_input_profile.touch_device = "engine:emu_window";
    Settings::values.current_input_profile.udp_input_address = "127.0.0.1";
    Settings::values.current_input_profile.udp_input_port = 26760;

    // Audio defaults (self-contained + platform SDK backends only)
    Settings::values.output_type = static_cast<AudioCore::SinkType>(0);
    Settings::values.input_type = static_cast<AudioCore::InputType>(0);

    // Network defaults
    NetSettings::values.web_api_url = "";
    NetSettings::values.citra_username = "";
    NetSettings::values.citra_token = "";
}

void Configuration::Reload() {
    ReadValues();
}
