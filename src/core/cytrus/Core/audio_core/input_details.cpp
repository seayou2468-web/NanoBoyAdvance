// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "audio_core/input_details.h"
#include "audio_core/null_input.h"
#include "audio_core/static_input.h"
#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif
#include "core/core.h"

namespace AudioCore {
namespace {
// input_details is ordered in terms of desirability, with the best choice at the top.
constexpr std::array input_details = {
#ifdef HAVE_CUBEB
    InputDetails{InputType::Cubeb, "Real Device (Cubeb)", true,
                 [](Core::System& system, std::string_view device_id) -> std::unique_ptr<Input> {
                     if (!system.HasMicPermission()) {
                         return std::make_unique<NullInput>();
                     }
                     return std::make_unique<CubebInput>(std::string(device_id));
                 },
                 &ListCubebInputDevices},
#endif
    InputDetails{InputType::Static, "Static Noise", false,
                 [](Core::System& system, std::string_view device_id) -> std::unique_ptr<Input> {
                     return std::make_unique<StaticInput>();
                 },
                 [] { return std::vector<std::string>{"Static Noise"}; }},
    InputDetails{InputType::Null, "None", false,
                 [](Core::System& system, std::string_view device_id) -> std::unique_ptr<Input> {
                     return std::make_unique<NullInput>();
                 },
                 [] { return std::vector<std::string>{"None"}; }},
};
} // Anonymous namespace

std::vector<InputDetails> ListInputs() {
    return {input_details.begin(), input_details.end()};
}

const InputDetails& GetInputDetails(InputType input_type) {
    auto iter = std::find_if(
        input_details.begin(), input_details.end(),
        [input_type](const auto& input_detail) { return input_detail.type == input_type; });

    if (input_type == InputType::Auto || iter == input_details.end()) {
        // Auto-select.
        // input_details is ordered in terms of desirability, with the best choice at the front.
        iter = input_details.begin();
    }

    return *iter;
}

} // namespace AudioCore
