// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "audio_core/null_sink.h"
#include "audio_core/sink_details.h"
#ifdef HAVE_CUBEB
#include "audio_core/cubeb_sink.h"
#endif
#ifdef HAVE_COREAUDIO
#include "audio_core/coreaudio_sink.h"
#endif

namespace AudioCore {
namespace {
// sink_details is ordered in terms of desirability, with the best choice at the top.
constexpr std::array sink_details = {
#ifdef HAVE_CUBEB
    SinkDetails{SinkType::Cubeb, "Cubeb",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<CubebSink>(device_id);
                },
                &ListCubebSinkDevices},
#endif
#ifdef HAVE_COREAUDIO
    SinkDetails{SinkType::CoreAudio, "CoreAudio",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<CoreAudioSink>(std::string(device_id));
                },
                &ListCoreAudioSinkDevices},
#endif
    SinkDetails{SinkType::Null, "None",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<NullSink>(device_id);
                },
                [] { return std::vector<std::string>{"None"}; }},
};
} // Anonymous namespace

std::vector<SinkDetails> ListSinks() {
    return {sink_details.begin(), sink_details.end()};
}

const SinkDetails& GetSinkDetails(SinkType sink_type) {
    auto iter = std::find_if(
        sink_details.begin(), sink_details.end(),
        [sink_type](const auto& sink_detail) { return sink_detail.type == sink_type; });

    if (sink_type == SinkType::Auto || iter == sink_details.end()) {
        // Auto-select.
        // sink_details is ordered in terms of desirability, with the best choice at the front.
        iter = sink_details.begin();
    }

    return *iter;
}

} // namespace AudioCore
