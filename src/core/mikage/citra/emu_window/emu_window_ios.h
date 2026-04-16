#pragma once

#include "../../common/emu_window.h"

class EmuWindow_IOS final : public EmuWindow {
public:
    EmuWindow_IOS() : m_native_layer(nullptr) {}
    ~EmuWindow_IOS() override = default;

    void SwapBuffers() override {}
    void PollEvents() override {}
    void MakeCurrent() override {}
    void DoneCurrent() override {}

    void SetNativeLayer(void* native_layer) {
        m_native_layer = native_layer;
    }

    void* GetNativeLayer() const override {
        return m_native_layer;
    }

private:
    void* m_native_layer;
};
