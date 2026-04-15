#include "video_core/renderer_software/renderer_software.h"

#include <algorithm>

#include "common/log.h"
#include "core/hw/gpu.h"
#include "video_core/video_core.h"

extern "C" void MikageConvertBGR24ToRGBA8888Flipped(const uint8_t* src,
                                                      uint8_t* dst,
                                                      size_t width,
                                                      size_t height,
                                                      size_t dst_stride_pixels);
namespace {
void FillOpaqueBlack(uint8_t* dst, size_t width, size_t height, size_t dst_stride_pixels) {
    for (size_t y = 0; y < height; ++y) {
        uint8_t* row = dst + (y * dst_stride_pixels * 4);
        for (size_t x = 0; x < width; ++x) {
            row[x * 4 + 0] = 0;
            row[x * 4 + 1] = 0;
            row[x * 4 + 2] = 0;
            row[x * 4 + 3] = 0xFF;
        }
    }
}
} // namespace

RendererSoftware::RendererSoftware() : m_render_window(nullptr), m_frames_since_tick(0) {
    m_framebuffer_rgba.resize(static_cast<size_t>(VideoCore::kScreenTopWidth) *
                              static_cast<size_t>(VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight) * 4);
    m_last_fps_tick = std::chrono::steady_clock::now();
}

RendererSoftware::~RendererSoftware() = default;

void RendererSoftware::SetWindow(EmuWindow* window) {
    m_render_window = window;
}

void RendererSoftware::Init() {
    std::fill(m_framebuffer_rgba.begin(), m_framebuffer_rgba.end(), 0);
    m_current_frame = 0;
    m_current_fps = 0.0f;
    m_frames_since_tick = 0;
    m_last_fps_tick = std::chrono::steady_clock::now();
    NOTICE_LOG(RENDER, "Software renderer initialized");
}

void RendererSoftware::UploadFramebuffers() {
    constexpr size_t top_width = static_cast<size_t>(VideoCore::kScreenTopWidth);
    constexpr size_t top_height = static_cast<size_t>(VideoCore::kScreenTopHeight);
    constexpr size_t bottom_width = static_cast<size_t>(VideoCore::kScreenBottomWidth);
    constexpr size_t bottom_height = static_cast<size_t>(VideoCore::kScreenBottomHeight);

    u8* dst = m_framebuffer_rgba.data();
    std::fill(m_framebuffer_rgba.begin(), m_framebuffer_rgba.end(), 0);
    if (const uint8_t* top_src = GPU::GetFramebufferPointer(GPU::g_regs.framebuffer_top_left_1)) {
        MikageConvertBGR24ToRGBA8888Flipped(top_src, dst, top_width, top_height, top_width);
    } else {
        FillOpaqueBlack(dst, top_width, top_height, top_width);
    }

    const size_t bottom_offset_y = top_height;
    const size_t horizontal_offset = (top_width - bottom_width) / 2;
    uint8_t* const bottom_dst = dst + ((bottom_offset_y * top_width + horizontal_offset) * 4);
    if (const uint8_t* bottom_src = GPU::GetFramebufferPointer(GPU::g_regs.framebuffer_sub_left_1)) {
        MikageConvertBGR24ToRGBA8888Flipped(
            bottom_src,
            bottom_dst,
            bottom_width,
            bottom_height,
            top_width);
    } else {
        FillOpaqueBlack(bottom_dst, bottom_width, bottom_height, top_width);
    }
}

void RendererSoftware::UpdateFramerate() {
    ++m_frames_since_tick;
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<float> elapsed = now - m_last_fps_tick;
    if (elapsed.count() >= 1.0f) {
        m_current_fps = static_cast<f32>(m_frames_since_tick / elapsed.count());
        m_frames_since_tick = 0;
        m_last_fps_tick = now;
    }
}

void RendererSoftware::PresentFrame() {
    // Software path keeps the composed RGBA frame in memory.
}

void RendererSoftware::SwapBuffers() {
    UploadFramebuffers();
    PresentFrame();
    if (m_render_window) {
        m_render_window->PollEvents();
        m_render_window->SwapBuffers();
    }
    UpdateFramerate();
    ++m_current_frame;
}

void RendererSoftware::ShutDown() {
    NOTICE_LOG(RENDER, "Software renderer shutdown");
}
