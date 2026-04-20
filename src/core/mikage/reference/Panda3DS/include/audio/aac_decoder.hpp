#pragma once
#include <functional>
#include <vector>

#include "audio/aac.hpp"
#include "helpers.hpp"

#if defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace Audio::AAC {
	class Decoder {
		using PaddrCallback = std::function<u8*(u32)>;
#if defined(__APPLE__)
		AudioConverterRef converter = nullptr;
		AudioStreamBasicDescription inputFormat {};
		AudioStreamBasicDescription outputFormat {};
		std::vector<u8> inputPacket;
#endif

		bool initialized = false;
		u32 sampleRate = 48000;
		u32 channelCount = 2;

		bool isInitialized() const { return initialized; }
		void initialize(u32 sampleRate, u32 channelCount);

	  public:
		// Decode function. Takes in a reference to the AAC response & request, and a callback for paddr -> pointer conversions
		// We also allow for optionally muting the AAC output (setting all of it to 0) instead of properly decoding it, for debug/research purposes
		void decode(AAC::Message& response, const AAC::Message& request, PaddrCallback paddrCallback, bool enableAudio = true);
		~Decoder();
	};
}  // namespace Audio::AAC
