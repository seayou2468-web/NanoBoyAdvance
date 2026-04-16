// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "pica200.h"
#include "../../common/log.h"
#include <cstring>
#include <algorithm>

namespace GPU {
namespace PICA200 {

// ============================================================================
// PICA200 Implementation
// ============================================================================

PICA200::PICA200() 
    : fifo_head(0), fifo_tail(0), gpu_busy(false), frame_count(0) {
    
    // Initialize registers
    registers.fill(0);
    
    // Default framebuffer config (400x240)
    framebuffer_config.width = 400;
    framebuffer_config.height = 240;
    framebuffer_config.color_address = 0x1F000000;
    framebuffer_config.depth_address = 0x1F046500;
    framebuffer_config.color_format = 0;  // RGBA8
    framebuffer_config.depth_format = 0;  // D16
    
    // Default viewport
    viewport_config.x = 0.0f;
    viewport_config.y = 0.0f;
    viewport_config.width = 400.0f;
    viewport_config.height = 240.0f;
    viewport_config.near_plane = 0.0f;
    viewport_config.far_plane = 1.0f;
    
    // Initialize rasterizer
    rasterizer = std::make_unique<Rasterizer::Rasterizer>(400, 240);
    fragment_shader = std::make_unique<Rasterizer::FragmentShader>();
    
    // Initialize texture system
    texture_cache = std::make_shared<Texture::TextureCache>(16 * 1024 * 1024); // 16MB cache
    texture_sampler = std::make_shared<Texture::TextureSampler>();
    
    // Configure fragment shader with texture resources
    fragment_shader->SetTextureCache(texture_cache);
    fragment_shader->SetTextureSampler(texture_sampler);
    
    // Default texture config
    current_texture_config.address = 0;
    current_texture_config.width = 256;
    current_texture_config.height = 256;
    current_texture_config.format = Texture::TextureFormat::RGBA8;
    current_texture_config.address_u = Texture::AddressMode::Repeat;
    current_texture_config.address_v = Texture::AddressMode::Repeat;
    current_texture_config.mag_filter = Texture::FilterMode::Linear;
    current_texture_config.min_filter = Texture::FilterMode::Linear;
    current_texture_config.enable_mipmaps = false;
    current_texture_config.mipmap_level = 0;
    
    NOTICE_LOG(GPU, "PICA200 GPU initialized (400x240) with texture system");
}

void PICA200::ProcessCommand(const GPUCommand& cmd) {
    // Add to FIFO
    if (command_fifo.size() < 256) {
        command_fifo.push_back(cmd);
        NOTICE_LOG(GPU, "GPU Command queued: ID=0x%x, P0=0x%x", cmd.cmd_id, cmd.param0);
    } else {
        WARN_LOG(GPU, "GPU Command FIFO overflow!");
    }
}

void PICA200::ProcessCommandFIFO() {
    while (!command_fifo.empty()) {
        const GPUCommand& cmd = command_fifo.front();
        
        u32 cmd_id = cmd.cmd_id >> 16;
        
        if (cmd_id >= 0x0000 && cmd_id < 0x0100) {
            ExecuteDrawCommand(cmd);
        } else if (cmd_id >= 0x0100 && cmd_id < 0x0200) {
            ExecuteMemoryTransferCommand(cmd);
        } else {
            ExecuteConfigCommand(cmd);
        }
        
        command_fifo.erase(command_fifo.begin());
    }
}

u32 PICA200::ReadRegister(GPURegister reg) {
    u32 reg_offset = static_cast<u32>(reg);
    if (reg_offset < registers.size()) {
        return registers[reg_offset];
    }
    return 0;
}

void PICA200::WriteRegister(GPURegister reg, u32 value) {
    u32 reg_offset = static_cast<u32>(reg);
    if (reg_offset < registers.size()) {
        registers[reg_offset] = value;
        NOTICE_LOG(GPU, "GPU Register 0x%x = 0x%x", reg_offset, value);
    }
}

void PICA200::SetFramebufferConfig(const FramebufferConfig& config) {
    framebuffer_config = config;
    NOTICE_LOG(GPU, "Framebuffer configured: %dx%d @ 0x%x (color), 0x%x (depth)",
               config.width, config.height, config.color_address, config.depth_address);
}

void PICA200::SetViewport(const ViewportConfig& config) {
    viewport_config = config;
    NOTICE_LOG(GPU, "Viewport: (%.0f, %.0f) size (%.0f x %.0f)", 
               config.x, config.y, config.width, config.height);
}

void PICA200::ExecuteDrawCommand(const GPUCommand& cmd) {
    u32 cmd_type = (cmd.cmd_id >> 16) & 0xFF;
    
    NOTICE_LOG(GPU, "GPU Draw Command: type=0x%x", cmd_type);
    
    // In real implementation, would:
    // 1. Fetch vertex data
    // 2. Run vertex shader
    // 3. Rasterize
    // 4. Run fragment shader
    // 5. Blend to framebuffer
}

void PICA200::ExecuteMemoryTransferCommand(const GPUCommand& cmd) {
    NOTICE_LOG(GPU, "GPU Memory Transfer Command: src=0x%x, dst=0x%x, size=%d",
               cmd.param0, cmd.param1, cmd.param2);
    
    // In real implementation, would DMA copy memory
}

void PICA200::ExecuteConfigCommand(const GPUCommand& cmd) {
    NOTICE_LOG(GPU, "GPU Config Command: 0x%x", cmd.cmd_id);
}

void PICA200::RenderFrame() {
    gpu_busy = true;
    
    // Clear framebuffer
    if (rasterizer) {
        rasterizer->Clear(0xFF000000); // Black
    }
    
    // Process command FIFO
    ProcessCommandFIFO();
    
    frame_count++;
    if (frame_count % 60 == 0) {
        NOTICE_LOG(GPU, "GPU: Rendered %d frames", frame_count);
    }
    
    gpu_busy = false;
}

void PICA200::Reset() {
    registers.fill(0);
    command_fifo.clear();
    fifo_head = fifo_tail = 0;
    gpu_busy = false;
    frame_count = 0;
    NOTICE_LOG(GPU, "GPU reset");
}

} // namespace PICA200
} // namespace GPU
