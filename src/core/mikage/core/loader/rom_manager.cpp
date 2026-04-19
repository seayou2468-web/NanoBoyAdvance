// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "rom_manager.h"
#include "../common/log.h"
#include <fstream>
#include <cstring>

namespace Loader {

// ============================================================================
// ROM Manager Implementation
// ============================================================================

ROMManager::ROMManager() 
    : current_format(ROMFormat::Unknown) {
}

ROMFormat ROMManager::IdentifyROMFormat(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        WARN_LOG(LOADER, "Cannot open file: %s", filename.c_str());
        return ROMFormat::Unknown;
    }
    
    u32 magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.close();
    
    // Check magic numbers
    if (magic == 0x4843434E) {  // "NCCH"
        NOTICE_LOG(LOADER, "Detected NCCH format");
        return ROMFormat::NCCH;
    } else if (magic == 0x7846534E) {  // "NFSx" (CXI)
        NOTICE_LOG(LOADER, "Detected CXI format");
        return ROMFormat::CXI;
    } else if (magic == 0x49534349) {  // "ISCI" (CIA)
        NOTICE_LOG(LOADER, "Detected CIA format");
        return ROMFormat::CIA;
    } else if (magic == 0x464C457F) {  // ELF
        NOTICE_LOG(LOADER, "Detected ELF format");
        return ROMFormat::ELF;
    }
    
    WARN_LOG(LOADER, "Unknown ROM format (magic: 0x%x)", magic);
    return ROMFormat::Unknown;
}

bool ROMManager::LoadROM(const std::string& filename) {
    NOTICE_LOG(LOADER, "Loading ROM: %s", filename.c_str());
    
    // Identify format
    current_format = IdentifyROMFormat(filename);
    
    switch (current_format) {
        case ROMFormat::NCCH:
            return LoadNCCH(filename);
        
        case ROMFormat::CXI:
            return LoadCXI(filename);
        
        case ROMFormat::ELF:
            return LoadELF(filename);
        
        default:
            ERROR_LOG(LOADER, "Unsupported ROM format");
            return false;
    }
}

bool ROMManager::LoadNCCH(const std::string& filename) {
    NOTICE_LOG(LOADER, "Loading NCCH ROM");
    
    ncch_loader = std::make_shared<NCCHLoader>();
    if (!ncch_loader->LoadROM(filename)) {
        ERROR_LOG(LOADER, "Failed to load NCCH ROM");
        ncch_loader = nullptr;
        return false;
    }
    
    NOTICE_LOG(LOADER, "NCCH ROM loaded successfully");
    return true;
}

bool ROMManager::LoadCXI(const std::string& filename) {
    // CXI format is essentially a single NCCH in a container
    // For simplicity, we'll treat it as NCCH
    NOTICE_LOG(LOADER, "Loading CXI ROM (treating as NCCH)");
    
    ncch_loader = std::make_shared<NCCHLoader>();
    if (!ncch_loader->LoadROM(filename)) {
        ERROR_LOG(LOADER, "Failed to load CXI ROM");
        ncch_loader = nullptr;
        return false;
    }
    
    NOTICE_LOG(LOADER, "CXI ROM loaded successfully");
    return true;
}

bool ROMManager::LoadELF(const std::string& filename) {
    ERROR_LOG(LOADER, "ELF format not yet implemented");
    return false;
}

u32 ROMManager::GetEntryPoint() const {
    if (ncch_loader) {
        return ncch_loader->GetEntryPoint();
    }
    return 0;
}

const std::string& ROMManager::GetProgramName() const {
    if (ncch_loader) {
        return ncch_loader->GetProgramName();
    }
    
    static std::string unknown = "Unknown";
    return unknown;
}

bool ROMManager::LoadIntoMemory(u8* memory, u32 memory_size) {
    if (!ncch_loader) {
        ERROR_LOG(LOADER, "No ROM loaded");
        return false;
    }
    
    return ncch_loader->LoadIntoMemory(memory, memory_size);
}

} // namespace Loader
