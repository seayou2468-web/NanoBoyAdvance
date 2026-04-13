#include "../core_adapter.hpp"

namespace core {

extern const CoreAdapter kMelonDSAdapter;

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
    .save_state_to_buffer = kMelonDSAdapter.save_state_to_buffer,
    .load_state_from_buffer = kMelonDSAdapter.load_state_from_buffer,
    .apply_cheat_code = kMelonDSAdapter.apply_cheat_code,
};

}  // namespace core
