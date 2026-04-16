// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../common/common_types.h"
#include "loader/ncch.h"
#include <string>
#include <memory>

namespace Loader {

// ============================================================================
// ROM File Identification
// ============================================================================

enum class ROMFormat {
    Unknown,
    NCCH,           // Nintendo Cartridge Content Header
    CXI,            // Cartridge backup image
    CCI,            // CTR Card Image
    CIA,            // CTR Importable Archive
    ELF,            // Executable and Linkable Format
    ThreeDS_X,      // Homebrew format
};

// ============================================================================
// ROM Manager
// ============================================================================

class ROMManager {
public:
    ROMManager();
    ~ROMManager() = default;
    
    // Identify ROM format
    static ROMFormat IdentifyROMFormat(const std::string& filename);
    
    // Load ROM
    bool LoadROM(const std::string& filename);
    
    // Get NCCH loader
    const std::shared_ptr<NCCHLoader>& GetNCCHLoader() const { return ncch_loader; }
    
    // Get entry point
    u32 GetEntryPoint() const;
    
    // Get program name
    const std::string& GetProgramName() const;
    
    // Load into memory
    bool LoadIntoMemory(u8* memory, u32 memory_size);

private:
    std::shared_ptr<NCCHLoader> ncch_loader;
    ROMFormat current_format;
    
    // Format-specific loaders
    bool LoadNCCH(const std::string& filename);
    bool LoadCXI(const std::string& filename);
    bool LoadELF(const std::string& filename);
};

} // namespace Loader
