// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "geometry.h"
#include "texture.h"
#include "../../common/common_types.h"
#include <vector>
#include <cstring>
#include <memory>

namespace GPU {
namespace Rasterizer {

// ============================================================================
// Rasterization Output (Fragment)
// ============================================================================

struct Fragment {
    u32 x, y;           // Pixel position
    f32 depth;          // Z-buffer depth
    Vec4 color;         // RGBA color
    Vec2 tex_coord;     // Texture coordinates
};

// ============================================================================
// Rasterizer
// ============================================================================

class Rasterizer {
public:
    Rasterizer(u32 width, u32 height);
    ~Rasterizer() = default;
    
    // Rasterize triangle
    std::vector<Fragment> RasterizeTriangle(
        const Geometry::Vertex& v0,
        const Geometry::Vertex& v1,
        const Geometry::Vertex& v2);
    
    // Clear framebuffer
    void Clear(u32 color);
    
    // Get framebuffer
    const std::vector<u32>& GetFramebuffer() const { return framebuffer; }
    std::vector<u32>& GetFramebuffer() { return framebuffer; }
    
    // Depth buffer access
    f32 GetDepthBuffer(u32 x, u32 y) const;
    void SetDepthBuffer(u32 x, u32 y, f32 depth);
    
    // Write fragment to framebuffer
    void WriteFragment(const Fragment& frag);

private:
    u32 width, height;
    std::vector<u32> framebuffer;      // Color framebuffer (RGBA8)
    std::vector<f32> depth_buffer;     // Depth buffer (Z)
    
    // Helper functions
    bool IsInsideTriangle(f32 x, f32 y,
                         const Geometry::Vertex& v0,
                         const Geometry::Vertex& v1,
                         const Geometry::Vertex& v2);
    
    f32 EdgeFunction(f32 px, f32 py, f32 ax, f32 ay, f32 bx, f32 by);
};

// ============================================================================
// Fragment Shader
// ============================================================================

class FragmentShader {
public:
    FragmentShader();
    ~FragmentShader() = default;
    
    // Execute fragment shader
    Vec4 Execute(const Fragment& frag);
    
    // Set texture configuration
    void SetTextureConfig(const Texture::TextureConfig& config);
    
    // Set texture cache
    void SetTextureCache(std::shared_ptr<Texture::TextureCache> cache);
    
    // Set texture sampler
    void SetTextureSampler(std::shared_ptr<Texture::TextureSampler> sampler);

private:
    std::shared_ptr<Texture::TextureCache> texture_cache;
    std::shared_ptr<Texture::TextureSampler> texture_sampler;
    Texture::TextureConfig current_texture;
};

} // namespace Rasterizer
} // namespace GPU
