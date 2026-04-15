#pragma once

#include <vector>

#include "common/common.h"
#include "common/emu_window.h"
#include "video_core/renderer_base.h"

class RendererSoftware : virtual public RendererBase {
public:
    RendererSoftware();
    ~RendererSoftware();

    void SwapBuffers() override;
    void SetWindow(EmuWindow* window) override;
    void Init() override;
    void ShutDown() override;

    const std::vector<u8>& Framebuffer() const {
        return m_framebuffer_rgba;
    }

private:
    void UploadFramebuffers();

    EmuWindow* m_render_window;
    std::vector<u8> m_framebuffer_rgba;
};

