// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include "rasterizer.h"
#include "texture.h"
#include <array>
#include <vector>
#include <memory>

namespace GPU {
namespace PICA200 {

// ============================================================================
// PICA200 GPU Register Map (3DS)
// ============================================================================

// GPU Registers (base: 0x10400000)
// PICA200 has ~320 registers organized in 0x10 byte blocks

enum class GPURegister : u32 {
    // Command FIFO
    CommandFIFO = 0x0000,      // Write command to FIFO
    
    // Control registers
    Control = 0x0001,
    Status = 0x0002,
    
    // Vertex processing
    VertexAttributeFormat = 0x0100,
    VertexAttributePermutation = 0x0101,
    PrimitiveConfig = 0x0200,
    IndexBufferConfig = 0x0201,
    
    // Triangle setup
    TriangleSetup = 0x0280,
    
    // Viewport
    Viewport = 0x0300,
    DepthRange = 0x0301,
    
    // Framebuffer config
    FramebufferConfig = 0x0400,
    FramebufferAddress0 = 0x0401,
    FramebufferAddress1 = 0x0402,
    ColorBufferFormat = 0x0403,
    DepthBufferFormat = 0x0404,
    
    // Blend settings
    BlendControl = 0x0500,
    BlendFunc = 0x0501,
    BlendConstant = 0x0502,
    
    // Texture
    TextureConfig = 0x0600,
    TextureAddress = 0x0601,
    TextureBorder = 0x0602,
    TextureEnvironment = 0x0603,
};

// ============================================================================
// PICA200 Command Format
// ============================================================================

struct GPUCommand {
    u32 cmd_id;
    u32 param0;
    u32 param1;
    u32 param2;
    u32 param3;
};

// ============================================================================
// Framebuffer Configuration
// ============================================================================

struct FramebufferConfig {
    u32 color_address;
    u32 depth_address;
    u32 width;
    u32 height;
    u8 color_format;    // 0=RGBA8, 1=RGB8, etc
    u8 depth_format;    // 0=D16, 1=D24, etc
};

// ============================================================================
// Viewport
// ============================================================================

struct ViewportConfig {
    float x, y;          // Viewport origin
    float width, height; // Viewport size
    float near_plane;
    float far_plane;
};

// ============================================================================
// PICA200 GPU Engine
// ============================================================================

class PICA200 {
public:
    PICA200();
    ~PICA200() = default;
    
    // Command processing
    void ProcessCommand(const GPUCommand& cmd);
    void ProcessCommandFIFO();
    
    // Register access
    u32 ReadRegister(GPURegister reg);
    void WriteRegister(GPURegister reg, u32 value);
    
    // Framebuffer configuration
    const FramebufferConfig& GetFramebufferConfig() const { return framebuffer_config; }
    void SetFramebufferConfig(const FramebufferConfig& config);
    
    // Viewport
    const ViewportConfig& GetViewport() const { return viewport_config; }
    void SetViewport(const ViewportConfig& config);
    
    // Render a frame
    void RenderFrame();
    
    // Get status
    bool IsBusy() const { return gpu_busy; }
    
    // Reset GPU
    void Reset();

private:
    // Command FIFO
    std::vector<GPUCommand> command_fifo;
    u32 fifo_head, fifo_tail;
    
    // Registers
    std::array<u32, 0x1000> registers;
    
    // Configuration
    FramebufferConfig framebuffer_config;
    ViewportConfig viewport_config;
    
    // Rasterizer & Fragment Shader
    std::unique_ptr<Rasterizer::Rasterizer> rasterizer;
    std::unique_ptr<Rasterizer::FragmentShader> fragment_shader;
    
    // Texture Management
    std::shared_ptr<Texture::TextureCache> texture_cache;
    std::shared_ptr<Texture::TextureSampler> texture_sampler;
    Texture::TextureConfig current_texture_config;
    
    bool gpu_busy;
    u32 frame_count;
    
    // Command execution
    void ExecuteDrawCommand(const GPUCommand& cmd);
    void ExecuteMemoryTransferCommand(const GPUCommand& cmd);
    void ExecuteConfigCommand(const GPUCommand& cmd);
};

} // namespace PICA200
} // namespace GPU
