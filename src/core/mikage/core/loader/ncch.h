// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include <string>
#include <vector>
#include <memory>

namespace Loader {

// ============================================================================
// NCCH Format Definitions (Nintendo Cartridge Content Header)
// ============================================================================

// NCCH magic number
#define NCCH_MAGIC 0x4843434E  // "NCCH"

// Encryption flags
enum class EncryptionType : u32 {
    NoEncryption = 0x0,
    FixedKeyEncryption = 0x1,
    NormalKeyEncryption = 0x2,
};

// Content type flags
enum class ContentType : u32 {
    Application = 0x0,
    SystemUpdate = 0x1,
    SystemModule = 0x2,
    Firmware = 0x3,
    TWl = 0x4,
};

// ============================================================================
// NCCH Header Structure
// ============================================================================

struct NCCHHeader {
    u32 magic;                          // 0x00: "NCCH"
    u32 content_size;                   // 0x04: Size in 16-byte units
    u8 partition_id[8];                 // 0x08: Partition ID
    u16 version;                        // 0x10: Content version
    u32 content_type;                   // 0x12: Content type
    u32 content_unit_size;              // 0x16: Content unit size (1 << value)
    
    // Flags
    u8 flags;                           // 0x1A: Flags
    u8 reserved1[3];                   // 0x1B: Reserved
    
    // Section offsets (in content units)
    u32 exheader_offset;                // 0x1E: ExHeader offset
    u32 exheader_size;                  // 0x22: ExHeader size
    u32 reserved2;                      // 0x26: Reserved
    u64 reserved3;                      // 0x2A: Reserved
    
    // Section info (4 entries × 8 bytes = 32 bytes)
    struct SectionInfo {
        u32 offset;                     // Offset in content units
        u32 size;                       // Size in content units
    } sections[4];                      // 0x32: .code, .rodata, .data, .bss
    
    u8 reserved4[16];                   // 0x52: Reserved
    u8 dependencies[48];                // 0x62: Program ID dependencies
    u8 reserved5[176];                  // 0x92: Reserved
    
} __attribute__((packed));

// ============================================================================
// Extended Header (ExHeader)
// ============================================================================

struct ExHeader {
    struct SciRegion {
        u32 arm11_code_address;         // ARM11 code address
        u32 arm11_code_size;            // ARM11 code size (pages)
        u32 arm9_code_address;          // ARM9 code address
        u32 arm9_code_size;             // ARM9 code size (pages)
    } sci;
    
    struct AccessControl {
        u32 rw_flags[3];                // Read/Write flags for memory regions
        u32 io_registers;               // I/O register access
        u8 reserved[256];               // Reserved
    } access_control;
    
    u8 arm11_local_capabilities[256];   // ARM11 local capabilities
    u8 arm9_access_control[16];         // ARM9 access control
};

// ============================================================================
// ROM Loader
// ============================================================================

class NCCHLoader {
public:
    NCCHLoader();
    ~NCCHLoader() = default;
    
    // Load NCCH ROM from file
    bool LoadROM(const std::string& filename);
    
    // Get program entry point
    u32 GetEntryPoint() const { return entry_point; }
    
    // Get program name
    const std::string& GetProgramName() const { return program_name; }
    
    // Load into memory
    bool LoadIntoMemory(u8* memory, u32 memory_size);
    
    // Get loaded sections
    const std::vector<u8>& GetCodeSection() const { return code_section; }
    const std::vector<u8>& GetRodataSection() const { return rodata_section; }
    const std::vector<u8>& GetDataSection() const { return data_section; }
    
    // Get section addresses
    u32 GetCodeAddress() const { return code_address; }
    u32 GetRodataAddress() const { return rodata_address; }
    u32 GetDataAddress() const { return data_address; }

private:
    NCCHHeader ncch_header;
    ExHeader ex_header;
    
    std::vector<u8> code_section;
    std::vector<u8> rodata_section;
    std::vector<u8> data_section;
    
    u32 entry_point;
    u32 code_address;
    u32 rodata_address;
    u32 data_address;
    std::string program_name;
    
    // Helper functions
    bool ReadNCCHHeader(const std::string& filename);
    bool ReadExHeader(const std::string& filename);
    bool ReadSections(const std::string& filename);
    void ParseExHeader();
};

// ============================================================================
// Executable Loader Interface
// ============================================================================

class ExecutableLoader {
public:
    virtual ~ExecutableLoader() = default;
    
    // Load executable
    virtual bool Load(const std::string& filename) = 0;
    
    // Get entry point
    virtual u32 GetEntryPoint() const = 0;
    
    // Load into memory
    virtual bool LoadIntoMemory(u8* memory, u32 memory_size) = 0;
};

} // namespace Loader
