#include "video_core/renderer_software/renderer_software.h"

#include "common/log.h"
#include "core/hw/gpu.h"
#include "video_core/video_core.h"

extern "C" void MikageConvertBGR24ToRGBA8888Flipped(const uint8_t* src,
                                                      uint8_t* dst,
                                                      size_t width,
                                                      size_t height,
                                                      size_t dst_stride_pixels);

RendererSoftware::RendererSoftware() : m_render_window(nullptr) {
    m_framebuffer_rgba.resize(static_cast<size_t>(VideoCore::kScreenTopWidth) *
                              static_cast<size_t>(VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight) * 4);
}

RendererSoftware::~RendererSoftware() = default;

void RendererSoftware::SetWindow(EmuWindow* window) {
    m_render_window = window;
}

void RendererSoftware::Init() {
    NOTICE_LOG(RENDER, "Software renderer initialized");
}

void RendererSoftware::UploadFramebuffers() {
    constexpr size_t top_width = static_cast<size_t>(VideoCore::kScreenTopWidth);
    constexpr size_t top_height = static_cast<size_t>(VideoCore::kScreenTopHeight);
    constexpr size_t bottom_width = static_cast<size_t>(VideoCore::kScreenBottomWidth);
    constexpr size_t bottom_height = static_cast<size_t>(VideoCore::kScreenBottomHeight);

    u8* dst = m_framebuffer_rgba.data();
    MikageConvertBGR24ToRGBA8888Flipped(
        GPU::GetFramebufferPointer(GPU::g_regs.framebuffer_top_left_1),
        dst,
        top_width,
        top_height,
        top_width);

    const size_t bottom_offset_y = top_height;
    const size_t horizontal_offset = (top_width - bottom_width) / 2;
    MikageConvertBGR24ToRGBA8888Flipped(
        GPU::GetFramebufferPointer(GPU::g_regs.framebuffer_sub_left_1),
        dst + ((bottom_offset_y * top_width + horizontal_offset) * 4),
        bottom_width,
        bottom_height,
        top_width);
}

void RendererSoftware::SwapBuffers() {
    UploadFramebuffers();
    if (m_render_window) {
        m_render_window->PollEvents();
        m_render_window->SwapBuffers();
    }
    ++m_current_frame;
}

void RendererSoftware::ShutDown() {
    NOTICE_LOG(RENDER, "Software renderer shutdown");
}

