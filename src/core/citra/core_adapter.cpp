#include "../core_adapter.hpp"

namespace core {

extern const CoreAdapter kMelonDSAdapter;

#ifndef CITRA_LITE_DISABLE_STATE_CHEATS
#define CITRA_LITE_DISABLE_STATE_CHEATS 1
#endif

namespace {

bool UnsupportedStateSave(void*, void*, size_t, size_t*, std::string& last_error) {
    last_error = "citra-lite: save state disabled for lightweight iOS profile";
    return false;
}

bool UnsupportedStateLoad(void*, const void*, size_t, std::string& last_error) {
    last_error = "citra-lite: load state disabled for lightweight iOS profile";
    return false;
}

bool UnsupportedCheatApply(void*, const char*, std::string& last_error) {
    last_error = "citra-lite: cheat API disabled for lightweight iOS profile";
    return false;
}

}  // namespace

const CoreAdapter kCitraAdapter = {
    .name = "citra",
    .type = EMULATOR_CORE_TYPE_3DS,
    .create_runtime = kMelonDSAdapter.create_runtime,
    .destroy_runtime = kMelonDSAdapter.destroy_runtime,
    .load_bios_from_path = kMelonDSAdapter.load_bios_from_path,
    .load_rom_from_path = kMelonDSAdapter.load_rom_from_path,
    .load_rom_from_memory = kMelonDSAdapter.load_rom_from_memory,
    .step_frame = kMelonDSAdapter.step_frame,
    .set_key_status = kMelonDSAdapter.set_key_status,
    .get_video_spec = kMelonDSAdapter.get_video_spec,
    .get_framebuffer_rgba = kMelonDSAdapter.get_framebuffer_rgba,
#if CITRA_LITE_DISABLE_STATE_CHEATS
    .save_state_to_buffer = UnsupportedStateSave,
    .load_state_from_buffer = UnsupportedStateLoad,
    .apply_cheat_code = UnsupportedCheatApply,
#else
    .save_state_to_buffer = kMelonDSAdapter.save_state_to_buffer,
    .load_state_from_buffer = kMelonDSAdapter.load_state_from_buffer,
    .apply_cheat_code = kMelonDSAdapter.apply_cheat_code,
#endif
};

}  // namespace core
