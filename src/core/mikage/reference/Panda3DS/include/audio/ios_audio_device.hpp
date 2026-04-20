#pragma once
#include <atomic>

#include "audio/audio_device_interface.hpp"

#if defined(__APPLE__)
#include <AudioToolbox/AudioQueue.h>
#endif

class IOSAudioDevice final : public AudioDeviceInterface {
	static constexpr u32 sampleRate = 32768;
	static constexpr u32 channelCount = 2;
	static constexpr u32 queueBufferCount = 3;
	static constexpr u32 queueFramesPerBuffer = 512;

	bool initialized = false;

#if defined(__APPLE__)
	AudioQueueRef queue = nullptr;
	AudioQueueBufferRef buffers[queueBufferCount] = {};
#endif

	void fillOutputBuffer(s16* output, usize frameCount);

  public:
	IOSAudioDevice(const AudioDeviceConfig& audioSettings);
	void init(Samples& samples, bool safe = false) override;
	void close() override;
	void start() override;
	void stop() override;
	bool isInitialized() const { return initialized; }
};

