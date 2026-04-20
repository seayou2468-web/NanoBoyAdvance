#pragma once
#include <atomic>
#include <AudioToolbox/AudioQueue.h>

#include "audio/audio_device_interface.hpp"

class IOSAudioDevice final : public AudioDeviceInterface {
	static constexpr u32 sampleRate = 32768;
	static constexpr u32 channelCount = 2;
	static constexpr u32 queueBufferCount = 3;
	static constexpr u32 queueFramesPerBuffer = 512;

	bool initialized = false;

	AudioQueueRef queue = nullptr;
	AudioQueueBufferRef buffers[queueBufferCount] = {};

	void fillOutputBuffer(s16* output, usize frameCount);

  public:
	IOSAudioDevice(const AudioDeviceConfig& audioSettings);
	void init(Samples& samples, bool safe = false) override;
	void close() override;
	void start() override;
	void stop() override;
	bool isInitialized() const { return initialized; }
};
