// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include "interrupt.h"
#include <array>
#include <memory>

namespace HW {
namespace LCD {

// ============================================================================
// LCD Configuration
// ============================================================================

// 3DS LCD Parameters
constexpr u32 LCD_TOP_WIDTH = 400;
constexpr u32 LCD_TOP_HEIGHT = 240;
constexpr u32 LCD_BOTTOM_WIDTH = 320;
constexpr u32 LCD_BOTTOM_HEIGHT = 240;

// Pixel format
enum class PixelFormat {
    RGB565 = 0,    // 16-bit per pixel (R5G6B5)
    RGBA8888 = 1,  // 32-bit per pixel (R8G8B8A8)
};

// ============================================================================
// Framebuffer Configuration
// ============================================================================

struct FramebufferFormat {
    PixelFormat pixel_format;
    u16 width;
    u16 height;
    u32 stride;  // Bytes per line
    u32 phys_addr;  // Physical address in FCRAM
};

// ============================================================================
// LCD Screen (Top/Bottom)
// ============================================================================

class Screen {
public:
    Screen(const std::string& name, u32 width, u32 height);
    ~Screen() = default;
    
    // Framebuffer access
    const FramebufferFormat& GetFramebufferConfig() const { return framebuffer; }
    void SetFramebufferConfig(const FramebufferFormat& config);
    
    u32 GetPhysAddr() const { return framebuffer.phys_addr; }
    void SetPhysAddr(u32 addr) { framebuffer.phys_addr = addr; }
    
    // Screen control
    bool IsEnabled() const { return enabled; }
    void SetEnabled(bool e) { enabled = e; }
    
    // Video mode settings
    u32 GetVBlankInterval() const { return vblank_interval; }
    u32 GetHBlankInterval() const { return hblank_interval; }
    
    // Refresh rates (in lines)
    u32 GetCurrentLine() const { return current_line; }
    void AdvanceLine();
    void ResetLine() { current_line = 0; }

private:
    std::string name;
    FramebufferFormat framebuffer;
    bool enabled;
    u32 current_line;
    u32 vblank_interval;
    u32 hblank_interval;
};

// ============================================================================
// LCD Controller
// ============================================================================

class LCDController {
public:
    LCDController();
    ~LCDController() = default;
    
    // Screen access
    Screen& GetTopScreen() { return top_screen; }
    Screen& GetBottomScreen() { return bottom_screen; }
    
    const Screen& GetTopScreen() const { return top_screen; }
    const Screen& GetBottomScreen() const { return bottom_screen; }
    
    // Register access
    u32 GetLCDStatusRegister() const;
    void SetLCDStatusRegister(u32 value);
    
    u32 GetLCDControlRegister() const;
    void SetLCDControlRegister(u32 value);
    
    // Update (called every frame or periodically)
    void Update();
    
    // Mode switching (2D / 3D)
    bool Is3DEnabled() const { return mode_3d_enabled; }
    void Set3DMode(bool enabled);

private:
    Screen top_screen;
    Screen bottom_screen;
    
    bool mode_3d_enabled;
    u32 lcd_status;      // Status register
    u32 lcd_control;     // Control register
    u32 frame_count;     // Frame counter
};

// ============================================================================
// Global LCD Manager
// ============================================================================

extern LCDController* g_lcd_controller;

void InitLCDController();
void ShutdownLCDController();

} // namespace LCD
} // namespace HW
