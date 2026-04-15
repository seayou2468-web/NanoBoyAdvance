#include "renderer_software.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "../../common/log.h"
#include "../../core/hw/gpu.h"
#include "../video_core.h"

namespace {


struct RGBA {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

inline RGBA DecodeRGBA8(const uint8_t* pixel) {
    return RGBA{pixel[0], pixel[1], pixel[2], pixel[3]};
}

inline RGBA DecodeRGB8(const uint8_t* pixel) {
    return RGBA{pixel[0], pixel[1], pixel[2], 0xFF};
}

inline RGBA DecodeRGB565(const uint8_t* pixel) {
    const uint16_t v = static_cast<uint16_t>(pixel[0] | (pixel[1] << 8));
    const uint8_t r = static_cast<uint8_t>(((v >> 11) & 0x1F) * 255 / 31);
    const uint8_t g = static_cast<uint8_t>(((v >> 5) & 0x3F) * 255 / 63);
    const uint8_t b = static_cast<uint8_t>((v & 0x1F) * 255 / 31);
    return RGBA{r, g, b, 0xFF};
}

inline RGBA DecodeRGB5A1(const uint8_t* pixel) {
    const uint16_t v = static_cast<uint16_t>(pixel[0] | (pixel[1] << 8));
    const uint8_t r = static_cast<uint8_t>(((v >> 11) & 0x1F) * 255 / 31);
    const uint8_t g = static_cast<uint8_t>(((v >> 6) & 0x1F) * 255 / 31);
    const uint8_t b = static_cast<uint8_t>(((v >> 1) & 0x1F) * 255 / 31);
    const uint8_t a = (v & 0x1) ? 0xFF : 0x00;
    return RGBA{r, g, b, a};
}

inline RGBA DecodeRGBA4(const uint8_t* pixel) {
    const uint16_t v = static_cast<uint16_t>(pixel[0] | (pixel[1] << 8));
    const uint8_t r = static_cast<uint8_t>(((v >> 12) & 0xF) * 17);
    const uint8_t g = static_cast<uint8_t>(((v >> 8) & 0xF) * 17);
    const uint8_t b = static_cast<uint8_t>(((v >> 4) & 0xF) * 17);
    const uint8_t a = static_cast<uint8_t>((v & 0xF) * 17);
    return RGBA{r, g, b, a};
}

inline int BytesPerPixel(GPU::PixelFormat format) {
    switch (format) {
        case GPU::PixelFormat::RGBA8:
            return 4;
        case GPU::PixelFormat::RGB8:
            return 3;
        case GPU::PixelFormat::RGB565:
        case GPU::PixelFormat::RGB5A1:
        case GPU::PixelFormat::RGBA4:
            return 2;
    }
    return 3;
}

inline RGBA DecodePixel(GPU::PixelFormat format, const uint8_t* pixel) {
    switch (format) {
        case GPU::PixelFormat::RGBA8:
            return DecodeRGBA8(pixel);
        case GPU::PixelFormat::RGB8:
            return DecodeRGB8(pixel);
        case GPU::PixelFormat::RGB565:
            return DecodeRGB565(pixel);
        case GPU::PixelFormat::RGB5A1:
            return DecodeRGB5A1(pixel);
        case GPU::PixelFormat::RGBA4:
            return DecodeRGBA4(pixel);
    }
    return RGBA{0, 0, 0, 0xFF};
}

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

}  // namespace

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

void RendererSoftware::LoadFBToScreenInfo(ScreenInfo& info,
                                          const uint8_t* framebuffer_data,
                                          size_t width,
                                          size_t height,
                                          size_t pixel_stride,
                                          GPU::PixelFormat format) {
    info.width = width;
    info.height = height;
    info.pixels.resize(width * height * 4);

    if (framebuffer_data == nullptr) {
        FillOpaqueBlack(info.pixels.data(), width, height, width);
        return;
    }

    const size_t bpp = static_cast<size_t>(BytesPerPixel(format));
    // Cytrus software renderer flips scanout from GPU memory to display coordinates.
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const size_t src_x = width - 1 - x;
            const uint8_t* src_pixel = framebuffer_data + (y * pixel_stride + src_x) * bpp;
            const RGBA decoded = DecodePixel(format, src_pixel);
            const size_t out_coord = (x * height + y) * 4;
            info.pixels[out_coord + 0] = decoded.r;
            info.pixels[out_coord + 1] = decoded.g;
            info.pixels[out_coord + 2] = decoded.b;
            info.pixels[out_coord + 3] = decoded.a;
        }
    }
}

void RendererSoftware::PrepareRenderTarget() {
    const GPU::FramebufferConfig& top = GPU::GetTopFramebufferConfig();
    const GPU::FramebufferConfig& bottom = GPU::GetBottomFramebufferConfig();
    const size_t top_width = top.width ? static_cast<size_t>(top.width) : static_cast<size_t>(VideoCore::kScreenTopWidth);
    const size_t top_height = top.height ? static_cast<size_t>(top.height) : static_cast<size_t>(VideoCore::kScreenTopHeight);
    const size_t bottom_width = bottom.width ? static_cast<size_t>(bottom.width) : static_cast<size_t>(VideoCore::kScreenBottomWidth);
    const size_t bottom_height = bottom.height ? static_cast<size_t>(bottom.height) : static_cast<size_t>(VideoCore::kScreenBottomHeight);
    const size_t top_bpp = static_cast<size_t>(BytesPerPixel(top.format));
    const size_t bottom_bpp = static_cast<size_t>(BytesPerPixel(bottom.format));
    const size_t top_stride = top.stride_bytes ? static_cast<size_t>(top.stride_bytes / top_bpp) : top_width;
    const size_t bottom_stride = bottom.stride_bytes ? static_cast<size_t>(bottom.stride_bytes / bottom_bpp) : bottom_width;

    const u32 top_left_address = top.active_fb == 0 ? top.left_address_1 : top.left_address_2;
    const u32 top_right_address = top.active_fb == 0 ? top.right_address_1 : top.right_address_2;
    const u32 bottom_left_address = bottom.active_fb == 0 ? bottom.left_address_1 : bottom.left_address_2;
    const u32 top_right_or_left = top_right_address ? top_right_address : top_left_address;

    LoadFBToScreenInfo(
        m_screen_infos[0],
        GPU::GetFramebufferPointer(top_left_address),
        top_width,
        top_height,
        top_stride,
        top.format);

    LoadFBToScreenInfo(
        m_screen_infos[1],
        GPU::GetFramebufferPointer(top_right_or_left),
        top_width,
        top_height,
        top_stride,
        top.format);

    LoadFBToScreenInfo(
        m_screen_infos[2],
        GPU::GetFramebufferPointer(bottom_left_address),
        bottom_width,
        bottom_height,
        bottom_stride,
        bottom.format);
}

void RendererSoftware::UploadFramebuffers() {
    PrepareRenderTarget();

    constexpr size_t top_width = static_cast<size_t>(VideoCore::kScreenTopWidth);
    constexpr size_t top_height = static_cast<size_t>(VideoCore::kScreenTopHeight);
    constexpr size_t bottom_width = static_cast<size_t>(VideoCore::kScreenBottomWidth);

    std::fill(m_framebuffer_rgba.begin(), m_framebuffer_rgba.end(), 0);

    if (!m_screen_infos[0].pixels.empty()) {
        const size_t top_copy = std::min(m_framebuffer_rgba.size(), m_screen_infos[0].pixels.size());
        std::memcpy(m_framebuffer_rgba.data(), m_screen_infos[0].pixels.data(), top_copy);
    }

    const size_t bottom_offset_y = top_height;
    const size_t horizontal_offset = (top_width - bottom_width) / 2;
    uint8_t* const bottom_dst = m_framebuffer_rgba.data() + ((bottom_offset_y * top_width + horizontal_offset) * 4);

    if (!m_screen_infos[2].pixels.empty()) {
        const size_t bottom_rows = std::min(m_screen_infos[2].height, static_cast<size_t>(VideoCore::kScreenBottomHeight));
        const size_t bottom_copy_width = std::min(m_screen_infos[2].width, bottom_width);
        for (size_t row = 0; row < bottom_rows; ++row) {
            const uint8_t* src = m_screen_infos[2].pixels.data() + row * m_screen_infos[2].width * 4;
            uint8_t* dst = bottom_dst + row * top_width * 4;
            std::memcpy(dst, src, bottom_copy_width * 4);
        }
    } else {
        FillOpaqueBlack(bottom_dst, bottom_width, VideoCore::kScreenBottomHeight, top_width);
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
