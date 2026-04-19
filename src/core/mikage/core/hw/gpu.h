// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"

namespace GPU {

enum class PixelFormat : u32 {
    RGBA8 = 0,
    RGB8 = 1,
    RGB565 = 2,
    RGB5A1 = 3,
    RGBA4 = 4,
};

struct FramebufferConfig {
    u32 left_address_1 = 0;
    u32 left_address_2 = 0;
    u32 right_address_1 = 0;
    u32 right_address_2 = 0;
    u32 stride_bytes = 0;
    u32 width = 0;
    u32 height = 0;
    PixelFormat format = PixelFormat::RGB8;
    u32 active_fb = 0;
};

struct Registers {
    enum Id : u32 {
        FramebufferTopLeft1     = 0x1EF00468,   // Main LCD, first framebuffer for 3D left
        FramebufferTopLeft2     = 0x1EF0046C,   // Main LCD, second framebuffer for 3D left
        FramebufferTopRight1    = 0x1EF00494,   // Main LCD, first framebuffer for 3D right
        FramebufferTopRight2    = 0x1EF00498,   // Main LCD, second framebuffer for 3D right
        FramebufferSubLeft1     = 0x1EF00568,   // Sub LCD, first framebuffer
        FramebufferSubLeft2     = 0x1EF0056C,   // Sub LCD, second framebuffer
        FramebufferSubRight1    = 0x1EF00594,   // Sub LCD, unused first framebuffer
        FramebufferSubRight2    = 0x1EF00598,   // Sub LCD, unused second framebuffer

        CommandListSize         = 0x1EF018E0,
        CommandListAddress      = 0x1EF018E8,
        ProcessCommandList      = 0x1EF018F0,
    };

    u32 framebuffer_top_left_1;
    u32 framebuffer_top_left_2;
    u32 framebuffer_top_right_1;
    u32 framebuffer_top_right_2;
    u32 framebuffer_sub_left_1;
    u32 framebuffer_sub_left_2;
    u32 framebuffer_sub_right_1;
    u32 framebuffer_sub_right_2;

    u32 command_list_size;
    u32 command_list_address;
    u32 command_processing_enabled;

    FramebufferConfig top_config;
    FramebufferConfig bottom_config;
};

extern Registers g_regs;

enum {
    TOP_ASPECT_X        = 0x5,
    TOP_ASPECT_Y        = 0x3,

    TOP_HEIGHT          = 240,
    TOP_WIDTH           = 400,
    BOTTOM_WIDTH        = 320,

    // Physical addresses in FCRAM used by ARM9 applications - these are correct for real hardware 
    PADDR_FRAMEBUFFER_SEL       = 0x20184E59,
    PADDR_TOP_LEFT_FRAME1       = 0x20184E60,
    PADDR_TOP_LEFT_FRAME2       = 0x201CB370,
    PADDR_TOP_RIGHT_FRAME1      = 0x20282160,
    PADDR_TOP_RIGHT_FRAME2      = 0x202C8670,
    PADDR_SUB_FRAME1            = 0x202118E0,
    PADDR_SUB_FRAME2            = 0x20249CF0,

    // Physical addresses in VRAM - I'm not sure how these are actually allocated (so not real)
    PADDR_VRAM_FRAMEBUFFER_SEL  = 0x18184E59,
    PADDR_VRAM_TOP_LEFT_FRAME1  = 0x18184E60,
    PADDR_VRAM_TOP_LEFT_FRAME2  = 0x181CB370,
    PADDR_VRAM_TOP_RIGHT_FRAME1 = 0x18282160,
    PADDR_VRAM_TOP_RIGHT_FRAME2 = 0x182C8670,
    PADDR_VRAM_SUB_FRAME1       = 0x182118E0,
    PADDR_VRAM_SUB_FRAME2       = 0x18249CF0,
};

/// Framebuffer location
enum FramebufferLocation {
    FRAMEBUFFER_LOCATION_UNKNOWN,   ///< Framebuffer location is unknown
    FRAMEBUFFER_LOCATION_FCRAM,     ///< Framebuffer is in the GSP heap
    FRAMEBUFFER_LOCATION_VRAM,      ///< Framebuffer is in VRAM
};

/**
 * Sets whether the framebuffers are in the GSP heap (FCRAM) or VRAM
 * @param 
 */
void SetFramebufferLocation(const FramebufferLocation mode);

/**
 * Gets a read-only pointer to a framebuffer in memory
 * @param address Physical address of framebuffer
 * @return Returns const pointer to raw framebuffer
 */
const u8* GetFramebufferPointer(const u32 address);

/// Gets top-screen software scanout configuration.
const FramebufferConfig& GetTopFramebufferConfig();

/// Gets bottom-screen software scanout configuration.
const FramebufferConfig& GetBottomFramebufferConfig();

/**
 * Gets the location of the framebuffers
 */
const FramebufferLocation GetFramebufferLocation();

template <typename T>
inline void Read(T &var, const u32 addr);

template <typename T>
inline void Write(u32 addr, const T data);

/// Update hardware
void Update();

/// Initialize hardware
void InitHardware();
inline void Init() { InitHardware(); }

/// Shutdown hardware
void Shutdown();


} // namespace
