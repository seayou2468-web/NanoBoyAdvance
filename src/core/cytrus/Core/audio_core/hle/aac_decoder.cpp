// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/hle/aac_decoder.h"

namespace AudioCore::HLE {

AACDecoder::AACDecoder(Memory::MemorySystem& memory_) : memory(memory_) {
    Reset();
}

AACDecoder::~AACDecoder() = default;

BinaryMessage AACDecoder::ProcessRequest(const BinaryMessage& request) {
    if (request.header.codec != DecoderCodec::DecodeAAC) {
        LOG_ERROR(Audio_DSP, "AAC decoder received unsupported codec: {}",
                  static_cast<u16>(request.header.codec));
        return {.header = {.result = ResultStatus::Error}};
    }

    switch (request.header.cmd) {
    case DecoderCommand::Init: {
        BinaryMessage response = request;
        response.header.result = OpenNewDecoder() ? ResultStatus::Success : ResultStatus::Error;
        return response;
    }
    case DecoderCommand::EncodeDecode:
        return Decode(request);
    case DecoderCommand::Shutdown:
    case DecoderCommand::SaveState:
    case DecoderCommand::LoadState: {
        BinaryMessage response = request;
        response.header.result = ResultStatus::Success;
        return response;
    }
    default:
        LOG_ERROR(Audio_DSP, "Got unknown AAC binary request: {}",
                  static_cast<u16>(request.header.cmd));
        return {.header = {.result = ResultStatus::Error}};
    }
}

void AACDecoder::Reset() {
    decoder_initialized = false;
    decoder = nullptr;
    OpenNewDecoder();
}

BinaryMessage AACDecoder::Decode(const BinaryMessage& request) {
    BinaryMessage response{};
    response.header.codec = request.header.codec;
    response.header.cmd = request.header.cmd;
    response.decode_aac_response.size = request.decode_aac_request.size;
    response.decode_aac_response.sample_rate = DecoderSampleRate::Rate48000;
    response.decode_aac_response.num_channels = 2;
    response.decode_aac_response.num_samples = 0;
    response.header.result = decoder_initialized ? ResultStatus::Success : ResultStatus::Error;

    LOG_WARNING(Audio_DSP, "AAC decode fallback active (no external FAAD2 dependency).");
    return response;
}

bool AACDecoder::OpenNewDecoder() {
    decoder_initialized = true;
    return true;
}

} // namespace AudioCore::HLE
