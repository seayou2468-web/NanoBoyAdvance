#pragma once

#include "../../common/emu_window.h"

class EmuWindow_GLFW final : public EmuWindow {
public:
    EmuWindow_GLFW() = default;
    ~EmuWindow_GLFW() override = default;

    void SwapBuffers() override {}
    void PollEvents() override {}
    void MakeCurrent() override {}
    void DoneCurrent() override {}
};
