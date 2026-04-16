// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "lcd.h"
#include "../../common/log.h"

namespace HW {
namespace LCD {

// ============================================================================
// Screen Implementation
// ============================================================================

Screen::Screen(const std::string& screen_name, u32 width, u32 height)
    : name(screen_name), enabled(true), current_line(0),
      vblank_interval(240), hblank_interval(400) {
    
    framebuffer.pixel_format = PixelFormat::RGB565;
    framebuffer.width = width;
    framebuffer.height = height;
    framebuffer.stride = width * 2;  // RGB565 = 2 bytes/pixel
    framebuffer.phys_addr = 0;
    
    NOTICE_LOG(HW, "Screen '%s' created: %dx%d", name.c_str(), width, height);
}

void Screen::SetFramebufferConfig(const FramebufferFormat& config) {
    framebuffer = config;
    NOTICE_LOG(HW, "Screen '%s': Framebuffer updated (addr=0x%x, stride=%d)",
               name.c_str(), config.phys_addr, config.stride);
}

void Screen::AdvanceLine() {
    current_line = (current_line + 1) % vblank_interval;
}

// ============================================================================
// LCD Controller Implementation
// ============================================================================

LCDController* g_lcd_controller = nullptr;

LCDController::LCDController()
    : top_screen("Top", LCD_TOP_WIDTH, LCD_TOP_HEIGHT),
      bottom_screen("Bottom", LCD_BOTTOM_WIDTH, LCD_BOTTOM_HEIGHT),
      mode_3d_enabled(false), lcd_status(0), lcd_control(0),
      frame_count(0) {
    
    // Default framebuffer addresses (3DS physical memory layout)
    top_screen.SetPhysAddr(0x1F000000);      // Top screen FCRAM
    bottom_screen.SetPhysAddr(0x1F046500);   // Bottom screen FCRAM
    
    NOTICE_LOG(HW, "LCD Controller initialized");
}

u32 LCDController::GetLCDStatusRegister() const {
    // Bit 0: V-Blank (1 = in V-Blank period)
    // Bit 1: H-Blank (1 = in H-Blank period)
    // Bits 8-9: PowerControl (3 = enabled)
    
    u32 status = 0;
    
    // Simulate V-Blank on every 240th line
    if (top_screen.GetCurrentLine() >= top_screen.GetVBlankInterval() - 10) {
        status |= 0x1;  // In V-Blank
    }
    
    // Power control bits
    status |= (0x3 << 8);  // Both screens enabled
    
    return status;
}

void LCDController::SetLCDStatusRegister(u32 value) {
    lcd_status = value;
    NOTICE_LOG(HW, "LCD Status Register: 0x%08x", value);
}

u32 LCDController::GetLCDControlRegister() const {
    // Bits 0-1: Screen select (0=both, 1=top, 2=bottom)
    // Bits 4-5: Backlight control
    
    u32 control = 0;
    control |= 0x0;  // Both screens
    control |= (0x3 << 4);  // Backlight enabled (both)
    
    if (mode_3d_enabled) {
        control |= (1 << 8);  // 3D mode bit
    }
    
    return control;
}

void LCDController::SetLCDControlRegister(u32 value) {
    lcd_control = value;
    NOTICE_LOG(HW, "LCD Control Register: 0x%08x", value);
}

void LCDController::Update() {
    // Advance line counters
    top_screen.AdvanceLine();
    bottom_screen.AdvanceLine();
    
    // Check for V-Blank
    if (top_screen.GetCurrentLine() == 0) {
        frame_count++;
        
        // V-Blank occurred - could signal interrupt here
        NOTICE_LOG(HW, "LCD: V-Blank! Frame #%d", frame_count);
    }
}

void LCDController::Set3DMode(bool enabled) {
    mode_3d_enabled = enabled;
    NOTICE_LOG(HW, "LCD: 3D Mode %s", enabled ? "enabled" : "disabled");
}

void InitLCDController() {
    if (!g_lcd_controller) {
        g_lcd_controller = new LCDController();
    }
}

void ShutdownLCDController() {
    if (g_lcd_controller) {
        delete g_lcd_controller;
        g_lcd_controller = nullptr;
    }
}

} // namespace LCD
} // namespace HW
