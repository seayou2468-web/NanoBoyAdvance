#pragma once

#include <chrono>
#include <vector>

#include "common/common.h"
#include "common/emu_window.h"
#include "video_core/renderer_base.h"

class RendererSoftware : virtual public RendererBase {
public:
    RendererSoftware();
    ~RendererSoftware() override;

    void SwapBuffers() override;
    void SetWindow(EmuWindow* window) override;
    void Init() override;
    void ShutDown() override;

    const std::vector<u8>& Framebuffer() const {
        return m_framebuffer_rgba;
    }

private:
    void UploadFramebuffers();
    void UpdateFramerate();

protected:
    virtual void PresentFrame();
    const std::vector<u8>& GetFrameBufferRGBA() const {
        return m_framebuffer_rgba;
    }

    EmuWindow* m_render_window;

private:
    std::vector<u8> m_framebuffer_rgba;
    std::chrono::steady_clock::time_point m_last_fps_tick;
    int m_frames_since_tick;
};
