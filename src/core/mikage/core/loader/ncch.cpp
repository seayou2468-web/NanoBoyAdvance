// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "ncch.h"
#include "../../common/log.h"
#include <fstream>
#include <cstring>

namespace Loader {

// ============================================================================
// NCCH Loader Implementation
// ============================================================================

NCCHLoader::NCCHLoader()
    : entry_point(0), code_address(0), rodata_address(0), data_address(0) {
    std::memset(&ncch_header, 0, sizeof(ncch_header));
    std::memset(&ex_header, 0, sizeof(ex_header));
    program_name = "Unknown";
}

bool NCCHLoader::LoadROM(const std::string& filename) {
    NOTICE_LOG(LOADER, "Loading NCCH ROM: %s", filename.c_str());
    
    // Read NCCH header
    if (!ReadNCCHHeader(filename)) {
        ERROR_LOG(LOADER, "Failed to read NCCH header");
        return false;
    }
    
    // Verify magic
    if (ncch_header.magic != NCCH_MAGIC) {
        ERROR_LOG(LOADER, "Invalid NCCH magic: 0x%x", ncch_header.magic);
        return false;
    }
    
    NOTICE_LOG(LOADER, "NCCH Header loaded");
    NOTICE_LOG(LOADER, "Content size: 0x%x", ncch_header.content_size);
    NOTICE_LOG(LOADER, "Content type: %d", ncch_header.content_type);
    
    // Read Extended Header
    if (!ReadExHeader(filename)) {
        ERROR_LOG(LOADER, "Failed to read Extended Header");
        return false;
    }
    
    // Parse ExHeader to get addresses
    ParseExHeader();
    
    // Read sections
    if (!ReadSections(filename)) {
        ERROR_LOG(LOADER, "Failed to read sections");
        return false;
    }
    
    NOTICE_LOG(LOADER, "ROM loaded successfully");
    NOTICE_LOG(LOADER, "Code:   0x%x (size 0x%x)", code_address, code_section.size());
    NOTICE_LOG(LOADER, "RoData: 0x%x (size 0x%x)", rodata_address, rodata_section.size());
    NOTICE_LOG(LOADER, "Data:   0x%x (size 0x%x)", data_address, data_section.size());
    NOTICE_LOG(LOADER, "Entry point: 0x%x", entry_point);
    
    return true;
}

bool NCCHLoader::ReadNCCHHeader(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        ERROR_LOG(LOADER, "Cannot open file: %s", filename.c_str());
        return false;
    }
    
    // Read NCCH header (512 bytes)
    file.read(reinterpret_cast<char*>(&ncch_header), sizeof(ncch_header));
    if (!file) {
        ERROR_LOG(LOADER, "Cannot read NCCH header");
        return false;
    }
    
    file.close();
    return true;
}

bool NCCHLoader::ReadExHeader(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // ExHeader starts right after NCCH header (offset 0x200)
    file.seekg(0x200);
    file.read(reinterpret_cast<char*>(&ex_header), sizeof(ex_header));
    
    if (!file) {
        ERROR_LOG(LOADER, "Cannot read Extended Header");
        return false;
    }
    
    file.close();
    return true;
}

void NCCHLoader::ParseExHeader() {
    // Extract code and data addresses from ExHeader
    code_address = ex_header.sci.arm11_code_address;
    
    // Typical layout for 3DS programs:
    // .code:   0x00100000
    // .rodata: 0x00200000 (approximate)
    // .data:   0x00300000 (approximate)
    
    rodata_address = code_address + (code_section.size() + 0xFFF) & ~0xFFF;
    data_address = rodata_address + (rodata_section.size() + 0xFFF) & ~0xFFF;
    
    // Entry point is at code start
    entry_point = code_address;
    
    NOTICE_LOG(LOADER, "ExHeader parsed:");
    NOTICE_LOG(LOADER, "ARM11 code address: 0x%x", ex_header.sci.arm11_code_address);
    NOTICE_LOG(LOADER, "ARM11 code pages: %d", ex_header.sci.arm11_code_size);
}

bool NCCHLoader::ReadSections(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // Get content unit size in bytes (1 << content_unit_size)
    u32 unit_size = 1 << (ncch_header.content_unit_size);
    if (unit_size == 0) {
        unit_size = 16;  // Default is 16 bytes
    }
    
    NOTICE_LOG(LOADER, "Content unit size: %d bytes", unit_size);
    
    // Read .code section (section 0)
    if (ncch_header.sections[0].size > 0) {
        u64 offset = ncch_header.sections[0].offset * unit_size;
        u32 size = ncch_header.sections[0].size * unit_size;
        
        NOTICE_LOG(LOADER, ".code: offset=0x%llx, size=0x%x", offset, size);
        
        code_section.resize(size);
        file.seekg(offset);
        file.read(reinterpret_cast<char*>(code_section.data()), size);
        
        if (!file) {
            ERROR_LOG(LOADER, "Failed to read .code section");
            return false;
        }
    }
    
    // Read .rodata section (section 1)
    if (ncch_header.sections[1].size > 0) {
        u64 offset = ncch_header.sections[1].offset * unit_size;
        u32 size = ncch_header.sections[1].size * unit_size;
        
        NOTICE_LOG(LOADER, ".rodata: offset=0x%llx, size=0x%x", offset, size);
        
        rodata_section.resize(size);
        file.seekg(offset);
        file.read(reinterpret_cast<char*>(rodata_section.data()), size);
        
        if (!file) {
            ERROR_LOG(LOADER, "Failed to read .rodata section");
            return false;
        }
    }
    
    // Read .data section (section 2)
    if (ncch_header.sections[2].size > 0) {
        u64 offset = ncch_header.sections[2].offset * unit_size;
        u32 size = ncch_header.sections[2].size * unit_size;
        
        NOTICE_LOG(LOADER, ".data: offset=0x%llx, size=0x%x", offset, size);
        
        data_section.resize(size);
        file.seekg(offset);
        file.read(reinterpret_cast<char*>(data_section.data()), size);
        
        if (!file) {
            ERROR_LOG(LOADER, "Failed to read .data section");
            return false;
        }
    }
    
    file.close();
    return true;
}

bool NCCHLoader::LoadIntoMemory(u8* memory, u32 memory_size) {
    if (!memory) {
        ERROR_LOG(LOADER, "Invalid memory pointer");
        return false;
    }
    
    // Load .code section
    if (!code_section.empty()) {
        if (code_address + code_section.size() > memory_size) {
            ERROR_LOG(LOADER, ".code section exceeds memory bounds");
            return false;
        }
        
        std::memcpy(memory + code_address, code_section.data(), code_section.size());
        NOTICE_LOG(LOADER, "Loaded .code: 0x%x -> 0x%x (0x%x bytes)",
                   code_address, code_address + code_section.size(), code_section.size());
    }
    
    // Load .rodata section
    if (!rodata_section.empty()) {
        if (rodata_address + rodata_section.size() > memory_size) {
            ERROR_LOG(LOADER, ".rodata section exceeds memory bounds");
            return false;
        }
        
        std::memcpy(memory + rodata_address, rodata_section.data(), rodata_section.size());
        NOTICE_LOG(LOADER, "Loaded .rodata: 0x%x -> 0x%x (0x%x bytes)",
                   rodata_address, rodata_address + rodata_section.size(), rodata_section.size());
    }
    
    // Load .data section
    if (!data_section.empty()) {
        if (data_address + data_section.size() > memory_size) {
            ERROR_LOG(LOADER, ".data section exceeds memory bounds");
            return false;
        }
        
        std::memcpy(memory + data_address, data_section.data(), data_section.size());
        NOTICE_LOG(LOADER, "Loaded .data: 0x%x -> 0x%x (0x%x bytes)",
                   data_address, data_address + data_section.size(), data_section.size());
    }
    
    NOTICE_LOG(LOADER, "All sections loaded into memory successfully");
    return true;
}

} // namespace Loader
