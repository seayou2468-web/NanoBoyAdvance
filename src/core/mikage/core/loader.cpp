// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "../common/common_types.h"
#include "../common/file_util.h"
#include <array>
#include <cstddef>
#include <cstring>
#include <vector>

#include "loader.h"
#include "system.h"
#include "core.h"
#include "./file_sys/directory_file_system.h"
#include "./elf/elf_reader.h"
#include "./hle/kernel/kernel.h"
#include "mem_map.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Loads an extracted CXI from a directory
bool LoadDirectory_CXI(std::string &filename) {
    std::string full_path = filename;
    std::string path, file, extension;
    SplitPath(ReplaceAll(full_path, "\\", "/"), &path, &file, &extension);
    DirectoryFileSystem *fs = new DirectoryFileSystem(&System::g_ctr_file_system, path);
    System::g_ctr_file_system.Mount("fs:", fs);

    std::string final_name = "fs:/" + file + extension;
    File::IOFile f(filename, "rb");

    if (f.IsOpen()) {
        // TODO(ShizZy): read here to memory....
    }
    ERROR_LOG(TIME, "Unimplemented function!");
    return true;
}

/// Loads a CTR ELF file
bool Load_ELF(std::string &filename) {
    std::string full_path = filename;
    std::string path, file, extension;
    SplitPath(ReplaceAll(full_path, "\\", "/"), &path, &file, &extension);
    File::IOFile f(filename, "rb");

    if (f.IsOpen()) {
        u64 size = f.GetSize();
        u8* buffer = new u8[size];
        ElfReader* elf_reader = NULL;

        f.ReadBytes(buffer, size);

        elf_reader = new ElfReader(buffer);
        elf_reader->LoadInto(0x00100000);

        Kernel::LoadExec(elf_reader->GetEntryPoint());

        delete[] buffer;
        delete elf_reader;
    } else {
        return false;
    }
    f.Close();

    return true;
}

/// Loads a Launcher DAT file
bool Load_DAT(std::string &filename) {
    std::string full_path = filename;
    std::string path, file, extension;
    SplitPath(ReplaceAll(full_path, "\\", "/"), &path, &file, &extension);
    File::IOFile f(filename, "rb");

    if (f.IsOpen()) {
        u64 size = f.GetSize();
        u8* buffer = new u8[size];

        f.ReadBytes(buffer, size);

        /**
        * (mattvail) We shouldn't really support this type of file
        * but for the sake of making it easier... we'll temporarily/hackishly
        * allow it. No sense in making a proper reader for this.
        */
        u32 entry_point = 0x00100000; // write to same entrypoint as elf
        u32 payload_offset = 0xA150;
        
        const u8 *src = &buffer[payload_offset];
        u8 *dst = Memory::GetPointer(entry_point);
        u32 srcSize = size - payload_offset; //just load everything...
        u32 *s = (u32*)src;
        u32 *d = (u32*)dst;
        for (int j = 0; j < (int)(srcSize + 3) / 4; j++)
        {
            *d++ = (*s++);
        }
        
        Kernel::LoadExec(entry_point);


        delete[] buffer;
    }
    else {
        return false;
    }
    f.Close();

    return true;
}


/// Loads a CTR BIN file extracted from an ExeFS
bool Load_BIN(std::string &filename) {
    std::string full_path = filename;
    std::string path, file, extension;
    SplitPath(ReplaceAll(full_path, "\\", "/"), &path, &file, &extension);
    File::IOFile f(filename, "rb");

    if (f.IsOpen()) {
        u64 size = f.GetSize();
        u8* buffer = new u8[size];

        f.ReadBytes(buffer, size);

        u32 entry_point = 0x00100000; // Hardcoded, read from exheader
        
        const u8 *src = buffer;
        u8 *dst = Memory::GetPointer(entry_point);
        u32 srcSize = size;
        u32 *s = (u32*)src;
        u32 *d = (u32*)dst;
        for (int j = 0; j < (int)(srcSize + 3) / 4; j++)
        {
            *d++ = (*s++);
        }
        
        Kernel::LoadExec(entry_point);

        delete[] buffer;
    }
    else {
        return false;
    }
    f.Close();

    return true;
}

namespace Loader {

namespace {

constexpr u32 kMediaUnitSize = 0x200;
constexpr u32 kNCSDPartitionTableOffset = 0x120;
constexpr u32 kNCSDPartitionCount = 8;
constexpr u32 kNCCHExeFSOffsetOffset = 0x1A0;
constexpr u32 kNCCHExeFSSizeOffset = 0x1A4;
constexpr u32 kNCCHExHeaderOffset = 0x200;
constexpr u32 kNCCHExHeaderFlagsOffset = 0x0D;
constexpr u32 kExeFSHeaderSize = 0x200;
constexpr u32 kExHeaderTextAddressOffset = 0x10;
constexpr size_t kExeFSEntryCount = 8;

constexpr u32 MakeMagic(char a, char b, char c, char d) {
    return static_cast<u32>(a) |
           (static_cast<u32>(b) << 8) |
           (static_cast<u32>(c) << 16) |
           (static_cast<u32>(d) << 24);
}

struct ExeFSEntry {
    char name[8];
    u32 offset;
    u32 size;
};

struct CIAHeader {
    u32 header_size;
    u16 type;
    u16 version;
    u32 cert_chain_size;
    u32 ticket_size;
    u32 tmd_size;
    u32 meta_size;
    u64 content_size;
};

u64 AlignUp64(u64 value, u64 alignment) {
    if (alignment == 0) {
        return value;
    }
    const u64 remainder = value % alignment;
    if (remainder == 0) {
        return value;
    }
    return value + (alignment - remainder);
}

size_t LZSSGetDecompressedSize(const std::vector<u8>& buffer) {
    if (buffer.size() < sizeof(u32)) {
        return 0;
    }
    u32 offset_size = 0;
    std::memcpy(&offset_size, buffer.data() + buffer.size() - sizeof(u32), sizeof(u32));
    return static_cast<size_t>(offset_size) + buffer.size();
}

bool LZSSDecompress(const std::vector<u8>& compressed, std::vector<u8>& decompressed) {
    if (compressed.size() < 8 || decompressed.empty()) {
        return false;
    }

    const u8* footer = compressed.data() + compressed.size() - 8;
    u32 buffer_top_and_bottom = 0;
    std::memcpy(&buffer_top_and_bottom, footer, sizeof(u32));

    size_t out = decompressed.size();
    size_t index = compressed.size() - ((buffer_top_and_bottom >> 24) & 0xFF);
    const size_t stop_index = compressed.size() - (buffer_top_and_bottom & 0xFFFFFF);

    if (index > compressed.size() || stop_index > compressed.size()) {
        return false;
    }

    std::memset(decompressed.data(), 0, decompressed.size());
    std::memcpy(decompressed.data(), compressed.data(), compressed.size());

    while (index > stop_index) {
        u8 control = compressed[--index];
        for (unsigned i = 0; i < 8; i++) {
            if (index <= stop_index || index == 0 || out == 0) {
                break;
            }

            if ((control & 0x80) != 0) {
                if (index < 2) {
                    return false;
                }
                index -= 2;

                u32 segment_offset = compressed[index] | (compressed[index + 1] << 8);
                u32 segment_size = ((segment_offset >> 12) & 15) + 3;
                segment_offset = (segment_offset & 0x0FFF) + 2;

                if (out < segment_size) {
                    return false;
                }

                for (unsigned j = 0; j < segment_size; j++) {
                    if (out + segment_offset >= decompressed.size()) {
                        return false;
                    }
                    decompressed[--out] = decompressed[out + segment_offset];
                }
            } else {
                if (out < 1 || index < 1) {
                    return false;
                }
                decompressed[--out] = compressed[--index];
            }
            control <<= 1;
        }
    }
    return true;
}

bool ReadU32At(File::IOFile& file, u64 offset, u32& out) {
    if (!file.Seek(static_cast<s64>(offset), SEEK_SET)) {
        return false;
    }
    return file.ReadArray<u32>(&out, 1);
}

bool ReadCIAHeader(File::IOFile& file, CIAHeader& out_header) {
    if (!file.Seek(0, SEEK_SET)) {
        return false;
    }
    return file.ReadBytes(&out_header, sizeof(out_header));
}

bool LoadCodeFromNCCH(File::IOFile& file, u64 ncch_offset, std::string* error_string) {
    u32 exefs_offset_units = 0;
    u32 exefs_size_units = 0;
    if (!ReadU32At(file, ncch_offset + kNCCHExeFSOffsetOffset, exefs_offset_units) ||
        !ReadU32At(file, ncch_offset + kNCCHExeFSSizeOffset, exefs_size_units)) {
        if (error_string) *error_string = "Failed to read NCCH ExeFS metadata";
        return false;
    }

    if (exefs_offset_units == 0 || exefs_size_units == 0) {
        if (error_string) *error_string = "NCCH ExeFS section is missing";
        return false;
    }

    const u64 exefs_offset = ncch_offset + static_cast<u64>(exefs_offset_units) * kMediaUnitSize;
    const u64 exefs_size_bytes = static_cast<u64>(exefs_size_units) * kMediaUnitSize;
    std::array<ExeFSEntry, kExeFSEntryCount> entries{};
    if (!file.Seek(static_cast<s64>(exefs_offset), SEEK_SET) ||
        !file.ReadBytes(entries.data(), sizeof(entries))) {
        if (error_string) *error_string = "Failed to read ExeFS header";
        return false;
    }

    ExeFSEntry code_entry{};
    bool found_code = false;
    for (const auto& entry : entries) {
        if (std::strncmp(entry.name, ".code", 5) == 0) {
            code_entry = entry;
            found_code = true;
            break;
        }
    }
    if (!found_code) {
        if (error_string) *error_string = "ExeFS .code section not found (possibly encrypted)";
        return false;
    }

    if (code_entry.offset > exefs_size_bytes || code_entry.size > exefs_size_bytes ||
        static_cast<u64>(code_entry.offset) + static_cast<u64>(code_entry.size) >
            (exefs_size_bytes > kExeFSHeaderSize ? exefs_size_bytes - kExeFSHeaderSize : 0)) {
        if (error_string) *error_string = "ExeFS .code section has invalid bounds";
        return false;
    }

    const u64 code_file_offset = exefs_offset + kExeFSHeaderSize + static_cast<u64>(code_entry.offset);
    std::vector<u8> code(static_cast<size_t>(code_entry.size));
    if (!file.Seek(static_cast<s64>(code_file_offset), SEEK_SET) ||
        !file.ReadBytes(code.data(), code.size())) {
        if (error_string) *error_string = "Failed to read ExeFS .code data";
        return false;
    }

    u8 exheader_flag = 0;
    const bool has_flags =
        file.Seek(static_cast<s64>(ncch_offset + kNCCHExHeaderOffset + kNCCHExHeaderFlagsOffset), SEEK_SET) &&
        file.ReadBytes(&exheader_flag, sizeof(exheader_flag));
    const bool is_compressed_code = has_flags && ((exheader_flag & 0x1) != 0);
    if (is_compressed_code) {
        std::vector<u8> decompressed(LZSSGetDecompressedSize(code));
        if (decompressed.empty() || !LZSSDecompress(code, decompressed)) {
            if (error_string) *error_string = "Failed to decompress ExeFS .code section";
            return false;
        }
        code = std::move(decompressed);
    }

    u32 entry_point = 0x00100000;
    ReadU32At(file, ncch_offset + kNCCHExHeaderOffset + kExHeaderTextAddressOffset, entry_point);
    const bool entry_in_exefs_region =
        entry_point >= Memory::EXEFS_CODE_VADDR && entry_point < Memory::EXEFS_CODE_VADDR_END;
    if (!entry_in_exefs_region ||
        static_cast<u64>(entry_point - Memory::EXEFS_CODE_VADDR) + static_cast<u64>(code.size()) >
            static_cast<u64>(Memory::EXEFS_CODE_SIZE)) {
        entry_point = 0x00100000;
    }

    u8* dst = Memory::GetPointer(entry_point);
    if (!dst) {
        // Fallback to legacy load address used by older Mikage paths.
        entry_point = 0x00100000;
        dst = Memory::GetPointer(entry_point);
        if (!dst) {
            if (error_string) *error_string = "Failed to map load destination for NCCH code";
            return false;
        }
    }

    std::memcpy(dst, code.data(), code.size());
    Kernel::LoadExec(entry_point);
    return true;
}

bool LoadCXI(const std::string& filename, std::string* error_string) {
    File::IOFile file(filename, "rb");
    if (!file.IsOpen()) {
        if (error_string) *error_string = "Failed to open CXI file";
        return false;
    }
    return LoadCodeFromNCCH(file, 0, error_string);
}

bool LoadCCI(const std::string& filename, std::string* error_string) {
    File::IOFile file(filename, "rb");
    if (!file.IsOpen()) {
        if (error_string) *error_string = "Failed to open CCI file";
        return false;
    }

    std::array<u32, 16> partition_entries{};
    if (!file.Seek(kNCSDPartitionTableOffset, SEEK_SET) ||
        !file.ReadArray<u32>(partition_entries.data(), partition_entries.size())) {
        if (error_string) *error_string = "Failed to read NCSD partition table";
        return false;
    }

    for (u32 partition = 0; partition < kNCSDPartitionCount; ++partition) {
        const u32 partition_offset_units = partition_entries[partition * 2];
        if (partition_offset_units == 0) {
            continue;
        }
        const u64 ncch_offset = static_cast<u64>(partition_offset_units) * kMediaUnitSize;
        u32 magic = 0;
        if (!ReadU32At(file, ncch_offset + 0x100, magic)) {
            continue;
        }
        if (magic == MakeMagic('N', 'C', 'C', 'H')) {
            return LoadCodeFromNCCH(file, ncch_offset, error_string);
        }
    }
    if (error_string) *error_string = "NCSD executable NCCH partition not found";
    return false;
}

bool LoadCIA(const std::string& filename, std::string* error_string) {
    File::IOFile file(filename, "rb");
    if (!file.IsOpen()) {
        if (error_string) *error_string = "Failed to open CIA file";
        return false;
    }

    CIAHeader cia{};
    if (!ReadCIAHeader(file, cia)) {
        if (error_string) *error_string = "Failed to read CIA header";
        return false;
    }

    if (cia.header_size < sizeof(CIAHeader) || cia.content_size == 0) {
        if (error_string) *error_string = "Invalid CIA header";
        return false;
    }

    u64 section_offset = AlignUp64(cia.header_size, 64);
    section_offset += AlignUp64(cia.cert_chain_size, 64);
    section_offset += AlignUp64(cia.ticket_size, 64);
    section_offset += AlignUp64(cia.tmd_size, 64);

    u32 magic = 0;
    if (!ReadU32At(file, section_offset + 0x100, magic) ||
        magic != MakeMagic('N', 'C', 'C', 'H')) {
        if (error_string) *error_string = "CIA first content is not a bootable NCCH";
        return false;
    }

    return LoadCodeFromNCCH(file, section_offset, error_string);
}

FileType GuessFromExtension(const std::string& filename) {
    if (filename.size() < 4) {
        return FILETYPE_UNKNOWN;
    }
    std::string extension;
    const size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos) {
        extension = filename.substr(dot);
    }

    if (!strcasecmp(extension.c_str(), ".elf") || !strcasecmp(extension.c_str(), ".axf")) {
        return FILETYPE_CTR_ELF;
    }
    if (!strcasecmp(extension.c_str(), ".bin")) {
        return FILETYPE_CTR_BIN;
    }
    if (!strcasecmp(extension.c_str(), ".dat")) {
        return FILETYPE_LAUNCHER_DAT;
    }
    if (!strcasecmp(extension.c_str(), ".3dsx")) {
        return FILETYPE_THREEDSX;
    }
    if (!strcasecmp(extension.c_str(), ".3ds") || !strcasecmp(extension.c_str(), ".cci")) {
        return FILETYPE_CTR_CCI;
    }
    if (!strcasecmp(extension.c_str(), ".cxi") || !strcasecmp(extension.c_str(), ".app")) {
        return FILETYPE_CTR_CXI;
    }
    if (!strcasecmp(extension.c_str(), ".cia")) {
        return FILETYPE_CTR_CIA;
    }
    if (!strcasecmp(extension.c_str(), ".zip")) {
        return FILETYPE_ARCHIVE_ZIP;
    }
    if (!strcasecmp(extension.c_str(), ".rar") ||
        !strcasecmp(extension.c_str(), ".r00") ||
        !strcasecmp(extension.c_str(), ".r01")) {
        return FILETYPE_ARCHIVE_RAR;
    }
    return FILETYPE_UNKNOWN;
}

FileType IdentifyByMagic(const std::string& filename) {
    File::IOFile file(filename, "rb");
    if (!file.IsOpen()) {
        return FILETYPE_ERROR;
    }

    u32 magic = 0;
    if (!file.Seek(0x100, SEEK_SET) || !file.ReadArray<u32>(&magic, 1)) {
        return FILETYPE_ERROR;
    }
    if (magic == MakeMagic('N', 'C', 'S', 'D')) {
        return FILETYPE_CTR_CCI;
    }
    if (magic == MakeMagic('N', 'C', 'C', 'H')) {
        return FILETYPE_CTR_CXI;
    }

    if (!file.Seek(0, SEEK_SET) || !file.ReadArray<u32>(&magic, 1)) {
        return FILETYPE_ERROR;
    }
    if (magic == MakeMagic('3', 'D', 'S', 'X')) {
        return FILETYPE_THREEDSX;
    }
    if (magic == MakeMagic(0x20, 0x20, 0x00, 0x00) || magic == MakeMagic(0x7F, 'E', 'L', 'F')) {
        return FILETYPE_CTR_ELF;
    }
    return FILETYPE_UNKNOWN;
}

} // namespace

bool IsBootableDirectory() {
    ERROR_LOG(TIME, "Unimplemented function!");
    return true;
}

/**
 * Identifies the type of a bootable file
 * @param filename String filename of bootable file
 * @todo (ShizZy) this function sucks... make it actually check file contents etc.
 * @return FileType of file
 */
FileType IdentifyFile(std::string &filename) {
    if (filename.size() == 0) {
        ERROR_LOG(LOADER, "invalid filename %s", filename.c_str());
        return FILETYPE_ERROR;
    }

    if (File::IsDirectory(filename)) {
        if (IsBootableDirectory()) {
            return FILETYPE_DIRECTORY_CXI;
        }
        else {
            return FILETYPE_NORMAL_DIRECTORY;
        }
    }

    const FileType guessed = GuessFromExtension(filename);
    FileType identified = IdentifyByMagic(filename);
    if (identified == FILETYPE_ERROR) {
        identified = FILETYPE_UNKNOWN;
    }

    if (identified == FILETYPE_UNKNOWN) {
        return guessed;
    }
    if (guessed != FILETYPE_UNKNOWN && guessed != identified && guessed != FILETYPE_CTR_CIA) {
        WARN_LOG(LOADER, "File extension/type mismatch: ext=%d magic=%d", guessed, identified);
    }
    return identified;
}

/**
 * Identifies and loads a bootable file
 * @param filename String filename of bootable file
 * @param error_string Point to string to put error message if an error has occurred
 * @return True on success, otherwise false
 */
bool LoadFile(std::string &filename, std::string *error_string) {
    INFO_LOG(LOADER, "Identifying file...");

    // Note that this can modify filename!
    switch (IdentifyFile(filename)) {

    case FILETYPE_CTR_ELF:
        return Load_ELF(filename);

    case FILETYPE_CTR_BIN:
        return Load_BIN(filename);

    case FILETYPE_LAUNCHER_DAT:
        return Load_DAT(filename);

    case FILETYPE_DIRECTORY_CXI:
        return LoadDirectory_CXI(filename);

    case FILETYPE_CTR_CCI:
        return LoadCCI(filename, error_string);

    case FILETYPE_CTR_CXI:
        return LoadCXI(filename, error_string);

    case FILETYPE_CTR_CIA:
        return LoadCIA(filename, error_string);

    case FILETYPE_THREEDSX:
        *error_string = "CIA/3DSX loader is not integrated yet";
        break;

    case FILETYPE_ERROR:
        ERROR_LOG(LOADER, "Could not read file");
        *error_string = "Error reading file";
        break;

    case FILETYPE_ARCHIVE_RAR:
        *error_string = "RAR file detected (Require UnRAR)";
        break;

    case FILETYPE_ARCHIVE_ZIP:
        *error_string = "ZIP file detected (Require UnRAR)";
        break;

    case FILETYPE_NORMAL_DIRECTORY:
        ERROR_LOG(LOADER, "Just a directory.");
        *error_string = "Just a directory.";
        break;

    case FILETYPE_UNKNOWN_BIN:
    case FILETYPE_UNKNOWN_ELF:
    case FILETYPE_UNKNOWN:
    default:
        ERROR_LOG(LOADER, "Failed to identify file");
        *error_string = " Failed to identify file";
        break;
    }
    return false;
}

} // namespace
