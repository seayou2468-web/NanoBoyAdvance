// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "audio_core/time_stretch.h"
#include "common/logging/log.h"

namespace AudioCore {

TimeStretcher::TimeStretcher() = default;
TimeStretcher::~TimeStretcher() = default;

void TimeStretcher::SetOutputSampleRate(unsigned int sample_rate) {
    output_sample_rate = sample_rate == 0 ? native_sample_rate : sample_rate;
}

std::size_t TimeStretcher::Process(const s16* in, std::size_t num_in, s16* out,
                                   std::size_t num_out) {
    if (num_in == 0 || num_out == 0) {
        return 0;
    }

    const double ratio = static_cast<double>(num_in) / static_cast<double>(num_out);
    stretch_ratio = std::clamp(ratio, 0.05, 8.0);
    LOG_TRACE(Audio, "{:5}/{:5} ratio:{:0.6f}", num_in, num_out, stretch_ratio);

    for (std::size_t frame = 0; frame < num_out; ++frame) {
        const std::size_t src_frame =
            std::min(static_cast<std::size_t>(std::floor(frame * stretch_ratio)), num_in - 1);
        out[frame * 2 + 0] = in[src_frame * 2 + 0];
        out[frame * 2 + 1] = in[src_frame * 2 + 1];
    }
    return num_out;
}

void TimeStretcher::Clear() {}

void TimeStretcher::Flush() {}

} // namespace AudioCore
