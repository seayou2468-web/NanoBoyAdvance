// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include "geometry.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace GPU {
namespace Texture {

// ============================================================================
// Texture Format Enumeration
// ============================================================================

enum class TextureFormat : u32 {
    RGBA8 = 0x0,      // 8 bits per channel (32-bit total)
    RGB8 = 0x1,       // 8 bits per channel (24-bit total)
    RGBA5551 = 0x2,   // RGB 5 bits, A 1 bit
    RGB565 = 0x3,     // RGB mode: R=5, G=6, B=5
    RGBA4 = 0x4,      // 4 bits per channel
    LA8 = 0x5,        // Luminance 8, Alpha 8
    HiL8 = 0x6,       // 8-bit luminance (high quality)
    L8 = 0x7,         // 8-bit luminance
    A8 = 0x8,         // 8-bit alpha
    LA4 = 0x9,        // Luminance 4, Alpha 4
    L4 = 0xA,         // 4-bit luminance
};

// ============================================================================
// Texture Address Mode (Wrapping)
// ============================================================================

enum class AddressMode : u32 {
    Repeat = 0x0,     // Wrap around
    Clamp = 0x1,      // Clamp to edge
    Mirror = 0x2,     // Mirror repeat
    MirrorOnce = 0x3, // Mirror once then clamp
};

// ============================================================================
// Texture Filter Mode
// ============================================================================

enum class FilterMode : u32 {
    Nearest = 0x0,    // Point sampling
    Linear = 0x1,     // Bilinear filtering
};

// ============================================================================
// Texture Configuration
// ============================================================================

struct TextureConfig {
    u32 address;              // Texture base address in memory
    u16 width, height;        // Texture dimensions
    TextureFormat format;     // Pixel format
    AddressMode address_u;    // U axis wrapping
    AddressMode address_v;    // V axis wrapping
    FilterMode mag_filter;    // Magnification filter
    FilterMode min_filter;    // Minification filter
    bool enable_mipmaps;      // Mipmap enable
    u32 mipmap_level;         // Current mipmap level
};

// ============================================================================
// Texture Cache Entry
// ============================================================================

struct TextureCacheEntry {
    TextureConfig config;
    std::vector<u32> data;    // Decoded RGBA8 data
    u64 last_access_time;     // For LRU eviction
    bool valid;
};

// ============================================================================
// Texture Sampler
// ============================================================================

class TextureSampler {
public:
    TextureSampler();
    ~TextureSampler() = default;
    
    // Sample texture at normalized coordinates [0.0, 1.0]
    Vec4 Sample(const TextureConfig& config, f32 u, f32 v);
    
    // Sample with explicit address mode
    Vec4 SampleEx(const TextureConfig& config, f32 u, f32 v, 
                  AddressMode addr_u, AddressMode addr_v);

private:
    // Texture coordinate wrapping
    f32 WrapCoordinate(f32 coord, AddressMode mode, u16 size);
    
    // Read texel from decoded texture data
    Vec4 ReadTexel(const std::vector<u32>& data, u32 x, u32 y, u16 width, u16 height);
    
    // Bilinear interpolation
    Vec4 BilinearFilter(const Vec4& p00, const Vec4& p10, 
                        const Vec4& p01, const Vec4& p11,
                        f32 fx, f32 fy);
};

// ============================================================================
// Texture Cache Manager
// ============================================================================

class TextureCache {
public:
    TextureCache(u32 max_cache_size = 16 * 1024 * 1024); // 16MB default
    ~TextureCache() = default;
    
    // Get or load texture
    const TextureCacheEntry* GetTexture(const TextureConfig& config, 
                                        const u8* memory, u32 memory_size);
    
    // Clear cache entry
    void ClearCacheEntry(u32 address);
    
    // Clear entire cache
    void ClearCache();
    
    // Decode texture from memory
    std::vector<u32> DecodeTexture(const TextureConfig& config,
                                   const u8* memory, u32 memory_size);

private:
    u32 max_cache_size;
    u32 cache_used;
    std::unordered_map<u32, TextureCacheEntry> texture_cache;
    u64 access_counter;
    
    // Decode functions per format
    std::vector<u32> DecodeRGBA8(const u8* data, u16 width, u16 height);
    std::vector<u32> DecodeRGB8(const u8* data, u16 width, u16 height);
    std::vector<u32> DecodeRGBA5551(const u8* data, u16 width, u16 height);
    std::vector<u32> DecodeRGB565(const u8* data, u16 width, u16 height);
    std::vector<u32> DecodeRGBA4(const u8* data, u16 width, u16 height);
    std::vector<u32> DecodeLA8(const u8* data, u16 width, u16 height);
    std::vector<u32> DecodeL8(const u8* data, u16 width, u16 height);
    std::vector<u32> DecodeA8(const u8* data, u16 width, u16 height);
    
    // Evict least recently used entry if cache full
    void EvictLRU();
};

} // namespace Texture
} // namespace GPU
