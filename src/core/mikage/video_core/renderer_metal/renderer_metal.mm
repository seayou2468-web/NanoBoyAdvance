#include "video_core/renderer_metal/renderer_metal.h"

#include "common/log.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#endif

RendererMetal::RendererMetal() : m_metal_available(false) {}

RendererMetal::~RendererMetal() = default;

void RendererMetal::Init() {
#if defined(__APPLE__)
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    m_metal_available = (device != nil);
    if (m_metal_available) {
        NOTICE_LOG(RENDER, "Metal renderer initialized (iOS SDK)");
    } else {
        WARNING_LOG(RENDER, "Metal device unavailable. Falling back to software path.");
    }
#else
    m_metal_available = false;
#endif
    RendererSoftware::Init();
}

void RendererMetal::SwapBuffers() {
#if defined(__APPLE__)
    if (m_metal_available) {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device != nil) {
            id<MTLCommandQueue> queue = [device newCommandQueue];
            if (queue != nil) {
                id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
                [command_buffer commit];
            }
        }
    }
#endif
    RendererSoftware::SwapBuffers();
}

void RendererMetal::ShutDown() {
    RendererSoftware::ShutDown();
}

