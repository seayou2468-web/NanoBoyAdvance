#include "renderer_metal.h"

#include "../../common/log.h"
#include "../video_core.h"

#if defined(__APPLE__)
#ifdef BOOL
#undef BOOL
#endif
#import <TargetConditionals.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

namespace {
#if defined(__APPLE__)
id<MTLDevice> g_metal_device = nil;
id<MTLCommandQueue> g_metal_queue = nil;
id<MTLTexture> g_frame_texture = nil;
#endif
}

RendererMetal::RendererMetal() : m_metal_available(false) {}

RendererMetal::~RendererMetal() = default;

bool RendererMetal::InitializeMetalResources() {
#if defined(__APPLE__)
    g_metal_device = MTLCreateSystemDefaultDevice();
    if (g_metal_device == nil) {
        return false;
    }

    g_metal_queue = [g_metal_device newCommandQueue];
    if (g_metal_queue == nil) {
        return false;
    }

    MTLTextureDescriptor* descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:VideoCore::kScreenTopWidth
                                                          height:VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight
                                                       mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
    descriptor.storageMode = MTLStorageModeShared;
    g_frame_texture = [g_metal_device newTextureWithDescriptor:descriptor];
    return g_frame_texture != nil;
#else
    return false;
#endif
}

void RendererMetal::Init() {
#if defined(__APPLE__)
    m_metal_available = InitializeMetalResources();
    if (m_metal_available) {
        NOTICE_LOG(RENDER, "Metal renderer initialized (iOS SDK)");
    } else {
        WARN_LOG(RENDER, "Metal device unavailable. Falling back to software path.");
    }
#else
    m_metal_available = false;
#endif
    RendererSoftware::Init();
}

void RendererMetal::PresentFrame() {
#if defined(__APPLE__)
    if (!m_metal_available || g_frame_texture == nil || g_metal_queue == nil) {
        return;
    }

    const std::vector<u8>& framebuffer = GetFrameBufferRGBA();
    const MTLRegion full_region = MTLRegionMake2D(0, 0, VideoCore::kScreenTopWidth,
                                                  VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight);
    [g_frame_texture replaceRegion:full_region
                       mipmapLevel:0
                         withBytes:framebuffer.data()
                       bytesPerRow:VideoCore::kScreenTopWidth * 4];

    id<MTLCommandBuffer> command_buffer = [g_metal_queue commandBuffer];
    if (command_buffer == nil) {
        return;
    }

    CAMetalLayer* metal_layer = nil;
    if (m_render_window != nullptr) {
        metal_layer = (__bridge CAMetalLayer*)m_render_window->GetNativeLayer();
    }

    if (metal_layer != nil) {
        id<CAMetalDrawable> drawable = [metal_layer nextDrawable];
        if (drawable != nil) {
            id<MTLBlitCommandEncoder> blit_encoder = [command_buffer blitCommandEncoder];
            if (blit_encoder != nil) {
                [blit_encoder copyFromTexture:g_frame_texture
                                  sourceSlice:0
                                  sourceLevel:0
                                 sourceOrigin:MTLOriginMake(0, 0, 0)
                                   sourceSize:MTLSizeMake(VideoCore::kScreenTopWidth,
                                                          VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight,
                                                          1)
                                    toTexture:drawable.texture
                             destinationSlice:0
                             destinationLevel:0
                            destinationOrigin:MTLOriginMake(0, 0, 0)];
                [blit_encoder endEncoding];
            }
            [command_buffer presentDrawable:drawable];
        }
    }

    [command_buffer commit];
#endif
}

void RendererMetal::SwapBuffers() {
    RendererSoftware::SwapBuffers();
}

void RendererMetal::ShutDown() {
#if defined(__APPLE__)
    g_frame_texture = nil;
    g_metal_queue = nil;
    g_metal_device = nil;
#endif
    RendererSoftware::ShutDown();
}
