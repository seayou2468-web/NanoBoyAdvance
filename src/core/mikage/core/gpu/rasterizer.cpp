// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "rasterizer.h"
#include <algorithm>
#include <cmath>

namespace GPU {
namespace Rasterizer {

// ============================================================================
// Rasterizer Implementation
// ============================================================================

Rasterizer::Rasterizer(u32 width, u32 height)
    : width(width), height(height) {
    framebuffer.resize(width * height, 0xFF000000); // Black
    depth_buffer.resize(width * height, 1.0f);      // Far plane
}

f32 Rasterizer::EdgeFunction(f32 px, f32 py, f32 ax, f32 ay, f32 bx, f32 by) {
    return (px - bx) * (ay - by) - (py - by) * (ax - bx);
}

bool Rasterizer::IsInsideTriangle(f32 x, f32 y,
                                   const Geometry::Vertex& v0,
                                   const Geometry::Vertex& v1,
                                   const Geometry::Vertex& v2) {
    // Use edge function for point-in-triangle test
    f32 area = EdgeFunction(v0.position.x, v0.position.y,
                            v1.position.x, v1.position.y,
                            v2.position.x, v2.position.y);
    
    if (area == 0) return false; // Degenerate triangle
    
    f32 w0 = EdgeFunction(x, y, v1.position.x, v1.position.y,
                          v2.position.x, v2.position.y);
    f32 w1 = EdgeFunction(x, y, v2.position.x, v2.position.y,
                          v0.position.x, v0.position.y);
    f32 w2 = EdgeFunction(x, y, v0.position.x, v0.position.y,
                          v1.position.x, v1.position.y);
    
    // Check if all same sign
    f32 sign = (area > 0) ? 1.0f : -1.0f;
    return (w0 * sign >= 0) && (w1 * sign >= 0) && (w2 * sign >= 0);
}

std::vector<Fragment> Rasterizer::RasterizeTriangle(
    const Geometry::Vertex& v0,
    const Geometry::Vertex& v1,
    const Geometry::Vertex& v2) {
    
    std::vector<Fragment> fragments;
    
    // Get bounding box
    f32 min_x = std::min({v0.position.x, v1.position.x, v2.position.x});
    f32 max_x = std::max({v0.position.x, v1.position.x, v2.position.x});
    f32 min_y = std::min({v0.position.y, v1.position.y, v2.position.y});
    f32 max_y = std::max({v0.position.y, v1.position.y, v2.position.y});
    
    // Clamp to framebuffer bounds
    u32 x_start = static_cast<u32>(std::max(0.0f, min_x));
    u32 x_end = static_cast<u32>(std::min(static_cast<f32>(width - 1), max_x));
    u32 y_start = static_cast<u32>(std::max(0.0f, min_y));
    u32 y_end = static_cast<u32>(std::min(static_cast<f32>(height - 1), max_y));
    
    // Compute barycentric coordinates
    f32 area = EdgeFunction(v0.position.x, v0.position.y,
                            v1.position.x, v1.position.y,
                            v2.position.x, v2.position.y);
    
    if (area == 0) return fragments; // Degenerate triangle
    
    f32 area_inv = 1.0f / area;
    
    // Scan-convert
    for (u32 y = y_start; y <= y_end; y++) {
        for (u32 x = x_start; x <= x_end; x++) {
            f32 fx = static_cast<f32>(x) + 0.5f; // Center of pixel
            f32 fy = static_cast<f32>(y) + 0.5f;
            
            f32 w0 = EdgeFunction(fx, fy, v1.position.x, v1.position.y,
                                  v2.position.x, v2.position.y) * area_inv;
            f32 w1 = EdgeFunction(fx, fy, v2.position.x, v2.position.y,
                                  v0.position.x, v0.position.y) * area_inv;
            f32 w2 = EdgeFunction(fx, fy, v0.position.x, v0.position.y,
                                  v1.position.x, v1.position.y) * area_inv;
            
            // Point inside triangle if all weights >= 0
            if (w0 >= 0 && w1 >= 0 && w2 >= 0 && (w0 + w1 + w2) >= 0.99f) {
                // Interpolate depth
                f32 depth = v0.position.z * w0 + v1.position.z * w1 + v2.position.z * w2;
                
                // Interpolate texture coordinates
                Vec2 tex_coord = v0.tex_coord * w0 + v1.tex_coord * w1 + v2.tex_coord * w2;
                
                // Interpolate color
                Vec4 color = v0.color * w0 + v1.color * w1 + v2.color * w2;
                
                // Create fragment
                Fragment frag;
                frag.x = x;
                frag.y = y;
                frag.depth = depth;
                frag.tex_coord = tex_coord;
                frag.color = color;
                
                fragments.push_back(frag);
            }
        }
    }
    
    return fragments;
}

void Rasterizer::Clear(u32 color) {
    std::fill(framebuffer.begin(), framebuffer.end(), color);
    std::fill(depth_buffer.begin(), depth_buffer.end(), 1.0f);
}

f32 Rasterizer::GetDepthBuffer(u32 x, u32 y) const {
    if (x >= width || y >= height) return 1.0f;
    return depth_buffer[y * width + x];
}

void Rasterizer::SetDepthBuffer(u32 x, u32 y, f32 depth) {
    if (x >= width || y >= height) return;
    depth_buffer[y * width + x] = depth;
}

void Rasterizer::WriteFragment(const Fragment& frag) {
    if (frag.x >= width || frag.y >= height) return;
    
    // Depth test
    u32 index = frag.y * width + frag.x;
    if (frag.depth < depth_buffer[index]) {
        // Convert Vec4 (0.0-1.0) to RGBA8
        u32 r = static_cast<u32>(frag.color.x * 255.0f);
        u32 g = static_cast<u32>(frag.color.y * 255.0f);
        u32 b = static_cast<u32>(frag.color.z * 255.0f);
        u32 a = static_cast<u32>(frag.color.w * 255.0f);
        
        u32 color = (a << 24) | (r << 16) | (g << 8) | b;
        framebuffer[index] = color;
        depth_buffer[index] = frag.depth;
    }
}

// ============================================================================
// Fragment Shader Implementation
// ============================================================================

FragmentShader::FragmentShader()
    : texture_cache(nullptr), texture_sampler(nullptr) {
}

void FragmentShader::SetTextureConfig(const Texture::TextureConfig& config) {
    current_texture = config;
}

void FragmentShader::SetTextureCache(std::shared_ptr<Texture::TextureCache> cache) {
    texture_cache = cache;
}

void FragmentShader::SetTextureSampler(std::shared_ptr<Texture::TextureSampler> sampler) {
    texture_sampler = sampler;
}

Vec4 FragmentShader::Execute(const Fragment& frag) {
    Vec4 final_color = frag.color;
    
    // TODO: Apply texture sampling if texture cache and sampler available
    // if (texture_cache && texture_sampler) {
    //     Vec4 texel = texture_sampler->Sample(current_texture, 
    //                                           frag.tex_coord.x, 
    //                                           frag.tex_coord.y);
    //     final_color = final_color * texel;  // Modulate
    // }
    
    return final_color;
}

} // namespace Rasterizer
} // namespace GPU
