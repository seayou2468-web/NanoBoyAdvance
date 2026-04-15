#pragma once

#include <chrono>
#include <vector>

#include "../../common/common.h"
#include "../../common/emu_window.h"
#include "../renderer_base.h"

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

public:
    enum class PixelFormat {
        RGBA8,
        RGB8,
        RGB565,
        RGB5A1,
        RGBA4,
    };

private:
    struct ScreenInfo {
        size_t width = 0;
        size_t height = 0;
        std::vector<u8> pixels;
    };

    void PrepareRenderTarget();
    void LoadFBToScreenInfo(ScreenInfo& info,
                            const uint8_t* framebuffer_data,
                            size_t width,
                            size_t height,
                            size_t pixel_stride,
                            PixelFormat format);

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
    ScreenInfo m_screen_infos[3];
    std::chrono::steady_clock::time_point m_last_fps_tick;
    int m_frames_since_tick;
};
