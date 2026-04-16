// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace HLE {
namespace Service {

// ============================================================================
// File System (FS) Service
// ============================================================================

enum class MediaType : u32 {
    NAND = 0,
    SD = 1,
    GameCard = 2,
};

enum class FSPathType : u32 {
    Invalid = 0,
    Binary = 1,
    ASCII = 2,
    Unicode = 3,
};

class FSFile {
public:
    FSFile(u32 handle, const std::string& path) 
        : handle(handle), path(path), position(0), size(0) {}
    
    u32 GetHandle() const { return handle; }
    const std::string& GetPath() const { return path; }
    u32 GetPosition() const { return position; }
    u32 GetSize() const { return size; }
    
    void SetPosition(u32 pos) { position = pos; }
    void SetSize(u32 s) { size = s; }

private:
    u32 handle;
    std::string path;
    u32 position;
    u32 size;
};

class FSDirectory {
public:
    FSDirectory(u32 handle, const std::string& path) 
        : handle(handle), path(path) {}
    
    u32 GetHandle() const { return handle; }
    const std::string& GetPath() const { return path; }

private:
    u32 handle;
    std::string path;
};

class FS_Service_Extended {
public:
    FS_Service_Extended();
    
    // Mount SD card
    void MountSdmc(u32* buffer);
    
    // Unmount SD card
    void UnmountSdmc(u32* buffer);
    
    // Open file for reading/writing
    // Path format: "/path/to/file"
    void OpenFile(u32* buffer);
    
    // Open directory
    void OpenDirectory(u32* buffer);
    
    // Delete file
    void DeleteFile(u32* buffer);
    
    // Read file
    void ReadFile(u32* buffer);
    
    // Write file
    void WriteFile(u32* buffer);
    
    // Close file
    void CloseFile(u32* buffer);
    
    // Close directory
    void CloseDirectory(u32* buffer);
    
    // Create file
    void CreateFile(u32* buffer);
    
    // Create directory
    void CreateDirectory(u32* buffer);
    
    // Get file size
    void GetFileSize(u32* buffer);
    
    // Set file size (truncate)
    void SetFileSize(u32* buffer);
    
    // Format SD card
    void FormatSdmc(u32* buffer);

private:
    MediaType mounted_media;
    bool sd_mounted;
    
    std::map<u32, std::shared_ptr<FSFile>> open_files;
    std::map<u32, std::shared_ptr<FSDirectory>> open_directories;
    
    u32 next_file_handle;
    u32 next_dir_handle;
};

// ============================================================================
// HID Service Extended (Input Device)
// ============================================================================

enum class HIDPadButtonsBitField {
    A = (1 << 0),
    B = (1 << 1),
    Select = (1 << 2),
    Start = (1 << 3),
    DRight = (1 << 4),
    DLeft = (1 << 5),
    DUp = (1 << 6),
    DDown = (1 << 7),
    R = (1 << 8),
    L = (1 << 9),
    X = (1 << 10),
    Y = (1 << 11),
    ZL = (1 << 14),
    ZR = (1 << 15),
    C_Right = (1 << 16),
    C_Left = (1 << 17),
    C_Up = (1 << 18),
    C_Down = (1 << 19),
};

struct HIDInputState {
    u32 buttons;
    s16 circle_pad_x;   // -160 to 160
    s16 circle_pad_y;   // -160 to 160
    s16 cstick_x;       // New 3DS C-stick
    s16 cstick_y;
};

class HID_Service_Extended {
public:
    HID_Service_Extended();
    
    // Get current input state
    void GetKeysHeld(u32* buffer);
    
    // Get circle pad position
    void GetCirclePadXY(u32* buffer);
    
    // Get home button state
    void GetHomeButtonState(u32* buffer);
    
    // Get power button state (New 3DS)
    void GetPowerButtonState(u32* buffer);
    
    // Get touch screen state
    void GetTouchPadXY(u32* buffer);
    
    // Get accelerometer data
    void GetAccelerometerData(u32* buffer);
    
    // Get gyroscope data
    void GetGyroscopeData(u32* buffer);

private:
    HIDInputState current_input;
    u32 home_pressed;
    u32 power_pressed;
    
    // Touch panel state (0-319 x, 0-239 y)
    s16 touch_x;
    s16 touch_y;
    bool touch_pressed;
};

// ============================================================================
// CFG Service Extended (Configuration)
// ============================================================================

class CFG_Service_Extended {
public:
    CFG_Service_Extended();
    
    // Get system model (3DS / New 3DS / 2DS / etc)
    void GetSystemModel(u32* buffer);
    
    // Check if New 3DS
    void GetModelNintendo2DS(u32* buffer);
    
    // Get region
    // 0: Japan, 1: America, 2: Europe, 3-6: misc
    void GetRegionCanSet(u32* buffer);
    
    // Get language setting
    void GetLanguage(u32* buffer);
    
    // Get username
    void GetUsername(u32* buffer);
    
    // Set username
    void SetUsername(u32* buffer);
    
    // Get WiFi SSID
    void GetWifiNetwork(u32* buffer);
    
    // Get birthday
    void GetBirthday(u32* buffer);

private:
    u32 system_model;    // 4 = New 3DS, 3 = 3DS, etc
    u8 region;
    u8 language;
    std::string username;
    std::string wifi_ssid;
};

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<FS_Service_Extended> CreateFSService();
std::unique_ptr<HID_Service_Extended> CreateHIDService();
std::unique_ptr<CFG_Service_Extended> CreateCFGService();

} // namespace Service
} // namespace HLE
