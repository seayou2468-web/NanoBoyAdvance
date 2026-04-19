// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "texture.h"
#include "../../common/log.h"
#include <algorithm>
#include <cmath>

namespace GPU {
namespace Texture {

// ============================================================================
// Texture Sampler Implementation
// ============================================================================

TextureSampler::TextureSampler() {
}

f32 TextureSampler::WrapCoordinate(f32 coord, AddressMode mode, u16 size) {
    // Normalize coord to [0, size)
    f32 normalized = coord * size;
    
    switch (mode) {
        case AddressMode::Repeat:
            // Wrap: use modulo
            {
                i32 i = static_cast<i32>(normalized);
                return static_cast<f32>(i % size);
            }
        
        case AddressMode::Clamp:
            // Clamp: [0, size-1]
            return std::clamp(normalized, 0.0f, static_cast<f32>(size - 1));
        
        case AddressMode::Mirror:
            // Mirror repeat
            {
                i32 i = static_cast<i32>(normalized);
                i32 period = size * 2;
                i = ((i % period) + period) % period;
                if (i >= size) {
                    i = period - i - 1;
                }
                return static_cast<f32>(i);
            }
        
        case AddressMode::MirrorOnce:
            // Mirror once then clamp
            {
                if (normalized < 0) {
                    return -normalized;
                } else if (normalized >= size) {
                    return 2.0f * size - normalized - 1.0f;
                }
                return normalized;
            }
        
        default:
            return std::clamp(normalized, 0.0f, static_cast<f32>(size - 1));
    }
}

Vec4 TextureSampler::ReadTexel(const std::vector<u32>& data, u32 x, u32 y, 
                               u16 width, u16 height) {
    if (x >= width || y >= height || data.empty()) {
        return Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    u32 idx = y * width + x;
    if (idx >= data.size()) {
        return Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    u32 rgba = data[idx];
    Vec4 result;
    result.x = static_cast<f32>((rgba >> 16) & 0xFF) / 255.0f; // R
    result.y = static_cast<f32>((rgba >> 8) & 0xFF) / 255.0f;  // G
    result.z = static_cast<f32>(rgba & 0xFF) / 255.0f;         // B
    result.w = static_cast<f32>((rgba >> 24) & 0xFF) / 255.0f; // A
    
    return result;
}

Vec4 TextureSampler::BilinearFilter(const Vec4& p00, const Vec4& p10,
                                     const Vec4& p01, const Vec4& p11,
                                     f32 fx, f32 fy) {
    // Bilinear interpolation
    // p00 --- p10
    // |        |
    // p01 --- p11
    
    Vec4 p0 = p00 * (1.0f - fx) + p10 * fx;
    Vec4 p1 = p01 * (1.0f - fx) + p11 * fx;
    
    return p0 * (1.0f - fy) + p1 * fy;
}

Vec4 TextureSampler::Sample(const TextureConfig& config, f32 u, f32 v) {
    return SampleEx(config, u, v, config.address_u, config.address_v);
}

Vec4 TextureSampler::SampleEx(const TextureConfig& config, f32 u, f32 v,
                               AddressMode addr_u, AddressMode addr_v) {
    // Clamp normalized coordinates to [0, 1]
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    
    f32 x_wrapped = WrapCoordinate(u, addr_u, config.width);
    f32 y_wrapped = WrapCoordinate(v, addr_v, config.height);
    
    if (config.mag_filter == FilterMode::Nearest) {
        // Nearest neighbor
        u32 x = static_cast<u32>(x_wrapped);
        u32 y = static_cast<u32>(y_wrapped);
        
        // This will need actual texture data from cache
        // For now, return white
        return Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    } else {
        // Bilinear filtering
        f32 x_floor = std::floor(x_wrapped);
        f32 y_floor = std::floor(y_wrapped);
        f32 fx = x_wrapped - x_floor;
        f32 fy = y_wrapped - y_floor;
        
        u32 x0 = static_cast<u32>(x_floor);
        u32 y0 = static_cast<u32>(y_floor);
        u32 x1 = (x0 + 1) % config.width;
        u32 y1 = (y0 + 1) % config.height;
        
        // Return interpolated result (would need actual texture data)
        return Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

// ============================================================================
// Texture Cache Implementation
// ============================================================================

TextureCache::TextureCache(u32 max_cache_size)
    : max_cache_size(max_cache_size), cache_used(0), access_counter(0) {
}

const TextureCacheEntry* TextureCache::GetTexture(const TextureConfig& config,
                                                   const u8* memory, u32 memory_size) {
    u32 key = config.address;
    
    // Check if texture already in cache
    auto it = texture_cache.find(key);
    if (it != texture_cache.end()) {
        it->second.last_access_time = access_counter++;
        NOTICE_LOG(GPU, "Texture cache hit: addr=0x%x", config.address);
        return &it->second;
    }
    
    // Texture not in cache, decode and add
    TextureCacheEntry entry;
    entry.config = config;
    entry.data = DecodeTexture(config, memory, memory_size);
    entry.last_access_time = access_counter++;
    entry.valid = !entry.data.empty();
    
    u32 entry_size = entry.data.size() * sizeof(u32);
    
    // Evict if necessary
    while ((cache_used + entry_size) > max_cache_size && !texture_cache.empty()) {
        EvictLRU();
    }
    
    cache_used += entry_size;
    texture_cache[key] = entry;
    
    NOTICE_LOG(GPU, "Cached texture: addr=0x%x, %dx%d, format=%d",
               config.address, config.width, config.height, static_cast<u32>(config.format));
    
    return &texture_cache[key];
}

void TextureCache::ClearCacheEntry(u32 address) {
    auto it = texture_cache.find(address);
    if (it != texture_cache.end()) {
        cache_used -= it->second.data.size() * sizeof(u32);
        texture_cache.erase(it);
        NOTICE_LOG(GPU, "Cleared texture cache: addr=0x%x", address);
    }
}

void TextureCache::ClearCache() {
    texture_cache.clear();
    cache_used = 0;
    NOTICE_LOG(GPU, "Texture cache cleared");
}

void TextureCache::EvictLRU() {
    u32 lru_key = 0;
    u64 lru_time = UINT64_MAX;
    
    for (auto& [key, entry] : texture_cache) {
        if (entry.last_access_time < lru_time) {
            lru_time = entry.last_access_time;
            lru_key = key;
        }
    }
    
    auto it = texture_cache.find(lru_key);
    if (it != texture_cache.end()) {
        cache_used -= it->second.data.size() * sizeof(u32);
        texture_cache.erase(it);
    }
}

std::vector<u32> TextureCache::DecodeTexture(const TextureConfig& config,
                                             const u8* memory, u32 memory_size) {
    if (!memory || config.address >= memory_size) {
        return std::vector<u32>();
    }
    
    const u8* tex_data = memory + config.address;
    u32 available = memory_size - config.address;
    
    switch (config.format) {
        case TextureFormat::RGBA8:
            return DecodeRGBA8(tex_data, config.width, config.height);
        case TextureFormat::RGB8:
            return DecodeRGB8(tex_data, config.width, config.height);
        case TextureFormat::RGBA5551:
            return DecodeRGBA5551(tex_data, config.width, config.height);
        case TextureFormat::RGB565:
            return DecodeRGB565(tex_data, config.width, config.height);
        case TextureFormat::RGBA4:
            return DecodeRGBA4(tex_data, config.width, config.height);
        case TextureFormat::LA8:
            return DecodeLA8(tex_data, config.width, config.height);
        case TextureFormat::L8:
            return DecodeL8(tex_data, config.width, config.height);
        case TextureFormat::A8:
            return DecodeA8(tex_data, config.width, config.height);
        default:
            return DecodeRGBA8(tex_data, config.width, config.height);
    }
}

std::vector<u32> TextureCache::DecodeRGBA8(const u8* data, u16 width, u16 height) {
    std::vector<u32> result;
    result.resize(width * height);
    
    for (u32 i = 0; i < width * height; i++) {
        u8 r = data[i * 4 + 0];
        u8 g = data[i * 4 + 1];
        u8 b = data[i * 4 + 2];
        u8 a = data[i * 4 + 3];
        result[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    
    return result;
}

std::vector<u32> TextureCache::DecodeRGB8(const u8* data, u16 width, u16 height) {
    std::vector<u32> result;
    result.resize(width * height);
    
    for (u32 i = 0; i < width * height; i++) {
        u8 r = data[i * 3 + 0];
        u8 g = data[i * 3 + 1];
        u8 b = data[i * 3 + 2];
        result[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
    
    return result;
}

std::vector<u32> TextureCache::DecodeRGBA5551(const u8* data, u16 width, u16 height) {
    std::vector<u32> result;
    result.resize(width * height);
    
    for (u32 i = 0; i < width * height; i++) {
        u16 val = (data[i * 2 + 0] << 8) | data[i * 2 + 1];
        u8 r = ((val >> 11) & 0x1F) << 3;
        u8 g = ((val >> 6) & 0x1F) << 3;
        u8 b = ((val >> 1) & 0x1F) << 3;
        u8 a = (val & 0x1) ? 0xFF : 0x00;
        result[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    
    return result;
}

std::vector<u32> TextureCache::DecodeRGB565(const u8* data, u16 width, u16 height) {
    std::vector<u32> result;
    result.resize(width * height);
    
    for (u32 i = 0; i < width * height; i++) {
        u16 val = (data[i * 2 + 0] << 8) | data[i * 2 + 1];
        u8 r = ((val >> 11) & 0x1F) << 3;
        u8 g = ((val >> 5) & 0x3F) << 2;
        u8 b = (val & 0x1F) << 3;
        result[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
    
    return result;
}

std::vector<u32> TextureCache::DecodeRGBA4(const u8* data, u16 width, u16 height) {
    std::vector<u32> result;
    result.resize(width * height);
    
    for (u32 i = 0; i < width * height; i++) {
        u16 val = (data[i * 2 + 0] << 8) | data[i * 2 + 1];
        u8 r = ((val >> 12) & 0xF) << 4;
        u8 g = ((val >> 8) & 0xF) << 4;
        u8 b = ((val >> 4) & 0xF) << 4;
        u8 a = (val & 0xF) << 4;
        result[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    
    return result;
}

std::vector<u32> TextureCache::DecodeLA8(const u8* data, u16 width, u16 height) {
    std::vector<u32> result;
    result.resize(width * height);
    
    for (u32 i = 0; i < width * height; i++) {
        u8 l = data[i * 2 + 0];
        u8 a = data[i * 2 + 1];
        result[i] = (a << 24) | (l << 16) | (l << 8) | l;
    }
    
    return result;
}

std::vector<u32> TextureCache::DecodeL8(const u8* data, u16 width, u16 height) {
    std::vector<u32> result;
    result.resize(width * height);
    
    for (u32 i = 0; i < width * height; i++) {
        u8 l = data[i];
        result[i] = 0xFF000000 | (l << 16) | (l << 8) | l;
    }
    
    return result;
}

std::vector<u32> TextureCache::DecodeA8(const u8* data, u16 width, u16 height) {
    std::vector<u32> result;
    result.resize(width * height);
    
    for (u32 i = 0; i < width * height; i++) {
        u8 a = data[i];
        result[i] = (a << 24) | 0xFFFFFF;
    }
    
    return result;
}

} // namespace Texture
} // namespace GPU
