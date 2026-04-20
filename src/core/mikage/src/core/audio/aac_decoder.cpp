#include "../../../include/audio/aac_decoder.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

using namespace Audio;

namespace {
	static u32 sampleRateFromAdtsIndex(u8 index) {
		switch (index) {
			case 0: return 96000;
			case 1: return 88200;
			case 2: return 64000;
			case 3: return 48000;
			case 4: return 44100;
			case 5: return 32000;
			case 6: return 24000;
			case 7: return 22050;
			case 8: return 16000;
			case 9: return 12000;
			case 10: return 11025;
			case 11: return 8000;
			default: return 48000;
		}
	}

	static u32 sampleRateEnumFromHz(u32 rate) {
		switch (rate) {
			case 8000: return AAC::SampleRate::Rate8000;
			case 11025: return AAC::SampleRate::Rate11025;
			case 12000: return AAC::SampleRate::Rate12000;
			case 16000: return AAC::SampleRate::Rate16000;
			case 22050: return AAC::SampleRate::Rate22050;
			case 24000: return AAC::SampleRate::Rate24000;
			case 32000: return AAC::SampleRate::Rate32000;
			case 44100: return AAC::SampleRate::Rate44100;
			case 48000:
			default: return AAC::SampleRate::Rate48000;
		}
	}

	struct AACFrame {
		const u8* payload = nullptr;
		u32 payloadSize = 0;
		u32 sampleRate = 48000;
		u32 channels = 2;
	};

	static std::vector<AACFrame> parseAdtsFrames(const u8* input, u32 size) {
		std::vector<AACFrame> frames;
		u32 offset = 0;
		while (offset + 7 <= size) {
			const u8* hdr = input + offset;
			if (hdr[0] != 0xFF || (hdr[1] & 0xF0) != 0xF0) {
				break;
			}

			const bool protectionAbsent = (hdr[1] & 0x01) != 0;
			const u8 freqIndex = (hdr[2] >> 2) & 0x0F;
			const u32 channels = ((hdr[2] & 0x01) << 2) | ((hdr[3] >> 6) & 0x03);
			const u32 frameLength = (u32(hdr[3] & 0x03) << 11) | (u32(hdr[4]) << 3) | (u32(hdr[5] >> 5) & 0x07);
			const u32 headerLength = protectionAbsent ? 7 : 9;
			if (frameLength < headerLength || offset + frameLength > size) {
				break;
			}

			AACFrame frame;
			frame.payload = hdr + headerLength;
			frame.payloadSize = frameLength - headerLength;
			frame.sampleRate = sampleRateFromAdtsIndex(freqIndex);
			frame.channels = channels == 0 ? 2 : channels;
			frames.push_back(frame);

			offset += frameLength;
		}
		return frames;
	}
}  // namespace

void AAC::Decoder::decode(AAC::Message& response, const AAC::Message& request, AAC::Decoder::PaddrCallback paddrCallback, bool enableAudio) {
	response.command = request.command;
	response.mode = request.mode;
	response.decodeResponse.size = request.decodeRequest.size;
	response.resultCode = AAC::ResultCode::Success;
	response.decodeResponse.channelCount = 2;
	response.decodeResponse.sampleCount = 1024;
	response.decodeResponse.sampleRate = AAC::SampleRate::Rate48000;

	u8* input = paddrCallback(request.decodeRequest.address);
	u8* outputLeft = paddrCallback(request.decodeRequest.destAddrLeft);
	u8* outputRight = nullptr;

	if (input == nullptr || outputLeft == nullptr) {
		Helpers::warn("Invalid pointers passed to AAC decoder");
		return;
	}

	auto frames = parseAdtsFrames(input, request.decodeRequest.size);
	if (frames.empty()) {
		Helpers::warn("AAC ADTS parse failed");
		return;
	}

	sampleRate = frames[0].sampleRate;
	channelCount = std::min<u32>(frames[0].channels, 2);
	response.decodeResponse.channelCount = channelCount;
	response.decodeResponse.sampleRate = sampleRateEnumFromHz(sampleRate);
	response.decodeResponse.sampleCount = 1024;

	if (channelCount > 1) {
		outputRight = paddrCallback(request.decodeRequest.destAddrRight);
		if (outputRight == nullptr) {
			Helpers::warn("Right AAC output channel doesn't point to valid physical address");
			return;
		}
	}

	std::array<std::vector<s16>, 2> streams;
	for (auto& frame : frames) {
		if (!isInitialized()) {
			initialize(frame.sampleRate, channelCount);
		}

		if (!isInitialized()) {
			Helpers::warn("Failed to initialize iOS AAC decoder");
			return;
		}

		inputPacket.assign(frame.payload, frame.payload + frame.payloadSize);
		std::array<s16, 4096> pcm {};
		AudioBufferList outList {
			1, {AudioBuffer{u32(channelCount * sizeof(s16)), u32(pcm.size() * sizeof(s16)), pcm.data()}}
		};

		auto inputProc = [](AudioConverterRef, UInt32* ioNumberDataPackets, AudioBufferList* ioData, AudioStreamPacketDescription** outPacketDesc, void* userData)
			-> OSStatus {
			auto* self = reinterpret_cast<AAC::Decoder*>(userData);
			if (self->inputPacket.empty()) {
				*ioNumberDataPackets = 0;
				return -1;
			}

			ioData->mNumberBuffers = 1;
			ioData->mBuffers[0].mNumberChannels = self->channelCount;
			ioData->mBuffers[0].mDataByteSize = u32(self->inputPacket.size());
			ioData->mBuffers[0].mData = self->inputPacket.data();
			*ioNumberDataPackets = 1;

			static AudioStreamPacketDescription desc;
			desc.mStartOffset = 0;
			desc.mVariableFramesInPacket = 1024;
			desc.mDataByteSize = u32(self->inputPacket.size());
			*outPacketDesc = &desc;

			self->inputPacket.clear();
			return noErr;
		};

		UInt32 outPackets = 1024;
		OSStatus status = AudioConverterFillComplexBuffer(converter, inputProc, this, &outPackets, &outList, nullptr);
		if (status != noErr) {
			Helpers::warn("AudioConverter AAC decode failed");
			return;
		}

		const s16* decoded = reinterpret_cast<const s16*>(pcm.data());
		if (enableAudio) {
			for (u32 i = 0; i < outPackets; i++) {
				streams[0].push_back(decoded[i * channelCount]);
				if (channelCount > 1) {
					streams[1].push_back(decoded[i * channelCount + 1]);
				}
			}
		} else {
			streams[0].resize(streams[0].size() + outPackets, 0);
			if (channelCount > 1) {
				streams[1].resize(streams[1].size() + outPackets, 0);
			}
		}
	}

	if (!streams[0].empty()) {
		std::memcpy(outputLeft, streams[0].data(), streams[0].size() * sizeof(s16));
	}
	if (channelCount > 1 && !streams[1].empty() && outputRight != nullptr) {
		std::memcpy(outputRight, streams[1].data(), streams[1].size() * sizeof(s16));
	}
}

void AAC::Decoder::initialize(u32 newSampleRate, u32 newChannelCount) {
	if (converter != nullptr) {
		AudioConverterDispose(converter);
		converter = nullptr;
	}

	inputFormat = {};
	inputFormat.mFormatID = kAudioFormatMPEG4AAC;
	inputFormat.mSampleRate = newSampleRate;
	inputFormat.mChannelsPerFrame = newChannelCount;
	inputFormat.mFramesPerPacket = 1024;

	outputFormat = {};
	outputFormat.mFormatID = kAudioFormatLinearPCM;
	outputFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	outputFormat.mSampleRate = newSampleRate;
	outputFormat.mChannelsPerFrame = newChannelCount;
	outputFormat.mBitsPerChannel = 16;
	outputFormat.mFramesPerPacket = 1;
	outputFormat.mBytesPerFrame = (outputFormat.mBitsPerChannel / 8) * outputFormat.mChannelsPerFrame;
	outputFormat.mBytesPerPacket = outputFormat.mBytesPerFrame;

	if (AudioConverterNew(&inputFormat, &outputFormat, &converter) != noErr) {
		converter = nullptr;
		initialized = false;
		return;
	}

	initialized = true;
}

AAC::Decoder::~Decoder() {
	if (converter != nullptr) {
		AudioConverterDispose(converter);
		converter = nullptr;
	}
	initialized = false;
}
