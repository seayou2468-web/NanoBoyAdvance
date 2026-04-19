// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "fs_hid_cfg.h"
#include "../../common/log.h"
#include <cstring>

namespace HLE {
namespace Service {

// ============================================================================
// FS Service Implementation
// ============================================================================

FS_Service_Extended::FS_Service_Extended()
    : mounted_media(MediaType::NAND), sd_mounted(false),
      next_file_handle(0x1000), next_dir_handle(0x2000) {
}

void FS_Service_Extended::MountSdmc(u32* buffer) {
    if (!buffer) return;
    
    sd_mounted = true;
    mounted_media = MediaType::SD;
    
    buffer[0] = 0x20;
    buffer[1] = 0;  // Success
    
    NOTICE_LOG(KERNEL, "FS: MountSdmc (SD card mounted)");
}

void FS_Service_Extended::UnmountSdmc(u32* buffer) {
    if (!buffer) return;
    
    sd_mounted = false;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    
    NOTICE_LOG(KERNEL, "FS: UnmountSdmc");
}

void FS_Service_Extended::OpenFile(u32* buffer) {
    if (!buffer) return;
    
    // Path is passed via IPC descriptor
    // For now, use dummy path
    std::string path = "/rom.3ds";
    
    auto file = std::make_shared<FSFile>(++next_file_handle, path);
    open_files[next_file_handle] = file;
    
    buffer[0] = 0x20;
    buffer[1] = 0;  // Result code
    buffer[2] = next_file_handle;
    
    NOTICE_LOG(KERNEL, "FS: OpenFile(%s) -> handle=0x%x", path.c_str(), next_file_handle);
}

void FS_Service_Extended::OpenDirectory(u32* buffer) {
    if (!buffer) return;
    
    std::string path = "/";
    
    auto dir = std::make_shared<FSDirectory>(++next_dir_handle, path);
    open_directories[next_dir_handle] = dir;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = next_dir_handle;
    
    NOTICE_LOG(KERNEL, "FS: OpenDirectory(%s) -> handle=0x%x", path.c_str(), next_dir_handle);
}

void FS_Service_Extended::DeleteFile(u32* buffer) {
    if (!buffer) return;
    
    // File path in IPC buffer
    NOTICE_LOG(KERNEL, "FS: DeleteFile");
    
    buffer[0] = 0x20;
    buffer[1] = 0;
}

void FS_Service_Extended::ReadFile(u32* buffer) {
    if (!buffer) return;
    
    u32 file_handle = buffer[0];
    u32 read_size = buffer[1];
    
    auto it = open_files.find(file_handle);
    if (it != open_files.end()) {
        auto& file = it->second;
        
        // For now, return dummy data
        // In real impl, would read from actual file
        
        buffer[0] = 0x20;
        buffer[1] = 0;  // Result code
        buffer[2] = 0;  // Bytes read (0 = EOF or empty)
        
        NOTICE_LOG(KERNEL, "FS: ReadFile(handle=0x%x, size=%d) -> 0 bytes", 
                   file_handle, read_size);
    } else {
        buffer[0] = 0x20;
        buffer[1] = 0xD8400060;  // INVALID_HANDLE
        WARN_LOG(KERNEL, "FS: ReadFile - invalid handle 0x%x", file_handle);
    }
}

void FS_Service_Extended::WriteFile(u32* buffer) {
    if (!buffer) return;
    
    NOTICE_LOG(KERNEL, "FS: WriteFile");
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = 0;  // Bytes written
}

void FS_Service_Extended::CloseFile(u32* buffer) {
    if (!buffer) return;
    
    u32 file_handle = buffer[0];
    open_files.erase(file_handle);
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    
    NOTICE_LOG(KERNEL, "FS: CloseFile(handle=0x%x)", file_handle);
}

void FS_Service_Extended::CloseDirectory(u32* buffer) {
    if (!buffer) return;
    
    u32 dir_handle = buffer[0];
    open_directories.erase(dir_handle);
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    
    NOTICE_LOG(KERNEL, "FS: CloseDirectory(handle=0x%x)", dir_handle);
}

void FS_Service_Extended::CreateFile(u32* buffer) {
    if (!buffer) return;
    
    NOTICE_LOG(KERNEL, "FS: CreateFile");
    
    buffer[0] = 0x20;
    buffer[1] = 0;
}

void FS_Service_Extended::CreateDirectory(u32* buffer) {
    if (!buffer) return;
    
    NOTICE_LOG(KERNEL, "FS: CreateDirectory");
    
    buffer[0] = 0x20;
    buffer[1] = 0;
}

void FS_Service_Extended::GetFileSize(u32* buffer) {
    if (!buffer) return;
    
    u32 file_handle = buffer[0];
    
    auto it = open_files.find(file_handle);
    if (it != open_files.end()) {
        buffer[0] = 0x20;
        buffer[1] = 0;
        buffer[2] = it->second->GetSize() & 0xFFFFFFFF;
        buffer[3] = (it->second->GetSize() >> 32) & 0xFFFFFFFF;
        
        NOTICE_LOG(KERNEL, "FS: GetFileSize(handle=0x%x) -> %u", 
                   file_handle, it->second->GetSize());
    } else {
        buffer[1] = 0xD8400060;
    }
}

void FS_Service_Extended::SetFileSize(u32* buffer) {
    if (!buffer) return;
    
    NOTICE_LOG(KERNEL, "FS: SetFileSize");
    
    buffer[0] = 0x20;
    buffer[1] = 0;
}

void FS_Service_Extended::FormatSdmc(u32* buffer) {
    if (!buffer) return;
    
    NOTICE_LOG(KERNEL, "FS: FormatSdmc (would erase SD card)");
    
    buffer[0] = 0x20;
    buffer[1] = 0;
}

// ============================================================================
// HID Service Implementation
// ============================================================================

HID_Service_Extended::HID_Service_Extended()
    : home_pressed(0), power_pressed(0), 
      touch_x(0), touch_y(0), touch_pressed(false) {
    current_input.buttons = 0;
    current_input.circle_pad_x = 0;
    current_input.circle_pad_y = 0;
    current_input.cstick_x = 0;
    current_input.cstick_y = 0;
}

void HID_Service_Extended::GetKeysHeld(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;  // Result code
    buffer[2] = current_input.buttons;
    
    NOTICE_LOG(KERNEL, "HID: GetKeysHeld -> 0x%08x", current_input.buttons);
}

void HID_Service_Extended::GetCirclePadXY(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = (static_cast<u32>(current_input.circle_pad_x) & 0xFFFF) |
                (static_cast<u32>(current_input.circle_pad_y) << 16);
    
    NOTICE_LOG(KERNEL, "HID: GetCirclePadXY -> (%d, %d)", 
               current_input.circle_pad_x, current_input.circle_pad_y);
}

void HID_Service_Extended::GetHomeButtonState(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = home_pressed;
    
    NOTICE_LOG(KERNEL, "HID: GetHomeButtonState -> %d", home_pressed);
}

void HID_Service_Extended::GetPowerButtonState(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = 0;  // Power on
    
    NOTICE_LOG(KERNEL, "HID: GetPowerButtonState");
}

void HID_Service_Extended::GetTouchPadXY(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = (static_cast<u32>(touch_x) & 0xFFFF) |
                (static_cast<u32>(touch_y) << 16);
    
    NOTICE_LOG(KERNEL, "HID: GetTouchPadXY -> (%d, %d)", touch_x, touch_y);
}

void HID_Service_Extended::GetAccelerometerData(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    // X, Y, Z accelerometer values (for now, return 0)
    buffer[2] = 0;
    buffer[3] = 0;
    buffer[4] = 0;
    
    NOTICE_LOG(KERNEL, "HID: GetAccelerometerData");
}

void HID_Service_Extended::GetGyroscopeData(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    // X, Y, Z gyroscope values (for now, return 0)
    buffer[2] = 0;
    buffer[3] = 0;
    buffer[4] = 0;
    
    NOTICE_LOG(KERNEL, "HID: GetGyroscopeData");
}

// ============================================================================
// CFG Service Implementation
// ============================================================================

CFG_Service_Extended::CFG_Service_Extended()
    : system_model(4), region(0), language(0), 
      username("Aurora"), wifi_ssid("") {
}

void CFG_Service_Extended::GetSystemModel(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = system_model;  // 4 = New 3DS
    
    NOTICE_LOG(KERNEL, "CFG: GetSystemModel -> %d", system_model);
}

void CFG_Service_Extended::GetModelNintendo2DS(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = (system_model == 3) ? 1 : 0;  // 1 if 2DS
    
    NOTICE_LOG(KERNEL, "CFG: GetModelNintendo2DS");
}

void CFG_Service_Extended::GetRegionCanSet(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = region;
    
    NOTICE_LOG(KERNEL, "CFG: GetRegionCanSet -> %d", region);
}

void CFG_Service_Extended::GetLanguage(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = language;  // 0 = Japanese, 1 = English, etc
    
    NOTICE_LOG(KERNEL, "CFG: GetLanguage -> %d", language);
}

void CFG_Service_Extended::GetUsername(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    // Username typically returned via IPC buffer descriptor
    
    NOTICE_LOG(KERNEL, "CFG: GetUsername -> %s", username.c_str());
}

void CFG_Service_Extended::SetUsername(u32* buffer) {
    if (!buffer) return;
    
    NOTICE_LOG(KERNEL, "CFG: SetUsername");
    
    buffer[0] = 0x20;
    buffer[1] = 0;
}

void CFG_Service_Extended::GetWifiNetwork(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    
    NOTICE_LOG(KERNEL, "CFG: GetWifiNetwork");
}

void CFG_Service_Extended::GetBirthday(u32* buffer) {
    if (!buffer) return;
    
    buffer[0] = 0x20;
    buffer[1] = 0;
    buffer[2] = 0x0401;  // Month=04, Day=01
    
    NOTICE_LOG(KERNEL, "CFG: GetBirthday");
}

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<FS_Service_Extended> CreateFSService() {
    return std::make_unique<FS_Service_Extended>();
}

std::unique_ptr<HID_Service_Extended> CreateHIDService() {
    return std::make_unique<HID_Service_Extended>();
}

std::unique_ptr<CFG_Service_Extended> CreateCFGService() {
    return std::make_unique<CFG_Service_Extended>();
}

} // namespace Service
} // namespace HLE
