#pragma once

#include "./audio_device_interface.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE
#include "./ios_audio_device.hpp"
using AudioDevice = IOSAudioDevice;
#else
class NoopAudioDevice final : public AudioDeviceInterface {
  public:
    explicit NoopAudioDevice(const AudioDeviceConfig& audioSettings)
        : AudioDeviceInterface(nullptr, audioSettings) {}

    void init(Samples& sampleBuffer, bool /*safe*/ = false) override {
        samples = &sampleBuffer;
        running = false;
    }

    void close() override {
        running = false;
        samples = nullptr;
    }

    void start() override { running = true; }
    void stop() override { running = false; }
    bool isInitialized() const { return true; }
};

using AudioDevice = NoopAudioDevice;
#endif
