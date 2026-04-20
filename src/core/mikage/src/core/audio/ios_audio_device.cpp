#include "../../../include/audio/ios_audio_device.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

IOSAudioDevice::IOSAudioDevice(const AudioDeviceConfig& audioSettings) : AudioDeviceInterface(nullptr, audioSettings), initialized(false) {
	running = false;
}

void IOSAudioDevice::fillOutputBuffer(s16* output, usize frameCount) {
	usize samplesWritten = samples->pop(output, frameCount * channelCount);
	if (samplesWritten != 0) {
		std::memcpy(&lastStereoSample[0], &output[(samplesWritten - 1) * 2], sizeof(lastStereoSample));
	}

	float audioVolume = audioSettings.getVolume();
	if (audioVolume != 1.0f) {
		s16* sample = output;
		if (audioVolume > 1.0f) {
			audioVolume = 0.6f + 20.0f * std::log10(audioVolume);
			constexpr s32 min = s32(std::numeric_limits<s16>::min());
			constexpr s32 max = s32(std::numeric_limits<s16>::max());
			for (usize i = 0; i < samplesWritten; i += 2) {
				s16 l = s16(std::clamp<s32>(s32(float(sample[0]) * audioVolume), min, max));
				s16 r = s16(std::clamp<s32>(s32(float(sample[1]) * audioVolume), min, max));
				*sample++ = l;
				*sample++ = r;
			}
		} else {
			if (audioSettings.volumeCurve == AudioDeviceConfig::VolumeCurve::Cubic) {
				audioVolume = audioVolume * audioVolume * audioVolume;
			}
			for (usize i = 0; i < samplesWritten; i += 2) {
				s16 l = s16(float(sample[0]) * audioVolume);
				s16 r = s16(float(sample[1]) * audioVolume);
				*sample++ = l;
				*sample++ = r;
			}
		}
	}

	s16* pointer = &output[samplesWritten * 2];
	s16 l = lastStereoSample[0];
	s16 r = lastStereoSample[1];
	for (usize i = samplesWritten; i < frameCount; i++) {
		*pointer++ = l;
		*pointer++ = r;
	}
}

void IOSAudioDevice::init(Samples& samples, bool safe) {
	this->samples = &samples;
	running = false;
	lastStereoSample = {0, 0};

	if (safe) {
		initialized = true;
		return;
	}

	AudioStreamBasicDescription format {};
	format.mSampleRate = sampleRate;
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	format.mFramesPerPacket = 1;
	format.mChannelsPerFrame = channelCount;
	format.mBitsPerChannel = 16;
	format.mBytesPerFrame = (format.mBitsPerChannel / 8) * format.mChannelsPerFrame;
	format.mBytesPerPacket = format.mBytesPerFrame;

	const OSStatus status = AudioQueueNewOutput(
		&format,
		[](void* userData, AudioQueueRef q, AudioQueueBufferRef buffer) {
			auto* self = reinterpret_cast<IOSAudioDevice*>(userData);
			s16* out = reinterpret_cast<s16*>(buffer->mAudioData);
			const usize frameCount = queueFramesPerBuffer;
			if (self->running) {
				self->fillOutputBuffer(out, frameCount);
			} else {
				std::memset(out, 0, frameCount * channelCount * sizeof(s16));
			}
			buffer->mAudioDataByteSize = u32(frameCount * channelCount * sizeof(s16));
			AudioQueueEnqueueBuffer(q, buffer, 0, nullptr);
		},
		this, nullptr, nullptr, 0, &queue
	);

	if (status != noErr || queue == nullptr) {
		initialized = false;
		Helpers::warn("Failed to initialize iOS audio queue");
		return;
	}

	for (u32 i = 0; i < queueBufferCount; i++) {
		if (AudioQueueAllocateBuffer(queue, queueFramesPerBuffer * channelCount * sizeof(s16), &buffers[i]) != noErr) {
			initialized = false;
			Helpers::warn("Failed to allocate iOS audio queue buffer");
			return;
		}
		std::memset(buffers[i]->mAudioData, 0, queueFramesPerBuffer * channelCount * sizeof(s16));
		buffers[i]->mAudioDataByteSize = queueFramesPerBuffer * channelCount * sizeof(s16);
		AudioQueueEnqueueBuffer(queue, buffers[i], 0, nullptr);
	}

	initialized = true;
}

void IOSAudioDevice::start() {
	if (!initialized) {
		Helpers::warn("iOS audio device not initialized");
		return;
	}
	if (!running) {
		running = true;
		if (queue != nullptr) {
			AudioQueueStart(queue, nullptr);
		}
	}
}

void IOSAudioDevice::stop() {
	if (running) {
		running = false;
		if (queue != nullptr) {
			AudioQueuePause(queue);
		}
	}
}

void IOSAudioDevice::close() {
	stop();
	if (queue != nullptr) {
		AudioQueueDispose(queue, true);
		queue = nullptr;
	}
	initialized = false;
}
