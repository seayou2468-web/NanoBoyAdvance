#pragma once

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "common/common_paths.h"
#include "common/common_types.h"
#include "io_file.hpp"

namespace FileUtil {

enum class UserPath {
    NANDDir,
    SDMCDir,
    SysDataDir,
    LoadDir,
};

class IOFile {
public:
    IOFile() = default;
    explicit IOFile(const std::string& path, const char* permissions = "rb") : file_(path, permissions) {}
    explicit IOFile(FILE* handle) : file_(handle) {}
    virtual ~IOFile() = default;

    virtual bool IsOpen() const { return const_cast<::IOFile&>(file_).isOpen(); }
    virtual std::size_t ReadBytes(void* data, std::size_t count) {
        return file_.readBytes(data, count).second;
    }
    virtual std::size_t WriteBytes(const void* data, std::size_t count) {
        return file_.writeBytes(data, count).second;
    }
    virtual std::size_t GetSize() const {
        auto s = const_cast<::IOFile&>(file_).size();
        return s.value_or(0);
    }
    virtual bool Seek(s64 offset, int origin = SEEK_SET) { return file_.seek(offset, origin); }
    virtual bool Flush() { return file_.flush(); }
    virtual void Close() { file_.close(); }
    virtual bool SetSize(u64 size) { return file_.setSize(size); }

    virtual bool IsCrypto() const { return is_crypto_; }
    void SetCrypto(bool value) { is_crypto_ = value; }

protected:
    ::IOFile file_{};
    bool is_crypto_{};
};

class Z3DSReadIOFile : public IOFile {
public:
    explicit Z3DSReadIOFile(std::unique_ptr<IOFile> wrapped) : wrapped_(std::move(wrapped)) {}

    static std::optional<std::array<u8, 4>> GetUnderlyingFileMagic(IOFile*) {
        return std::nullopt;
    }

    bool IsOpen() const override { return wrapped_ && wrapped_->IsOpen(); }
    std::size_t ReadBytes(void* data, std::size_t count) override {
        return wrapped_ ? wrapped_->ReadBytes(data, count) : 0;
    }
    std::size_t WriteBytes(const void* data, std::size_t count) override {
        return wrapped_ ? wrapped_->WriteBytes(data, count) : 0;
    }
    std::size_t GetSize() const override { return wrapped_ ? wrapped_->GetSize() : 0; }
    bool Seek(s64 offset, int origin = SEEK_SET) override {
        return wrapped_ && wrapped_->Seek(offset, origin);
    }
    bool Flush() override { return wrapped_ && wrapped_->Flush(); }
    void Close() override {
        if (wrapped_) {
            wrapped_->Close();
        }
    }
    bool SetSize(u64 size) override { return wrapped_ && wrapped_->SetSize(size); }
    bool IsCrypto() const override { return wrapped_ && wrapped_->IsCrypto(); }

private:
    std::unique_ptr<IOFile> wrapped_;
};

class Z3DSWriteIOFile : public Z3DSReadIOFile {
public:
    static constexpr std::size_t DEFAULT_FRAME_SIZE = 1024 * 1024;

    Z3DSWriteIOFile(std::unique_ptr<IOFile> wrapped, [[maybe_unused]] std::array<u8, 4> magic,
                    [[maybe_unused]] std::size_t frame_size)
        : Z3DSReadIOFile(std::move(wrapped)) {}
};

inline std::string GetUserPath(UserPath path) {
    switch (path) {
    case UserPath::NANDDir:
        return ::IOFile::getNAND().string() + DIR_SEP;
    case UserPath::SDMCDir:
        return ::IOFile::getSDMC().string() + DIR_SEP;
    case UserPath::SysDataDir:
        return ::IOFile::getSysData().string() + DIR_SEP;
    case UserPath::LoadDir:
        return (::IOFile::getUserData() / "load").string() + DIR_SEP;
    }
    return {};
}

inline bool Exists(const std::string& path) { return std::filesystem::exists(path); }
inline bool IsDirectory(const std::string& path) { return std::filesystem::is_directory(path); }
inline bool CreateDir(const std::string& path) {
    std::error_code ec;
    if (std::filesystem::exists(path)) {
        return true;
    }
    return std::filesystem::create_directory(path, ec);
}
inline bool CreateFullPath(const std::string& path) {
    std::error_code ec;
    const auto parent = std::filesystem::path(path).parent_path();
    if (parent.empty()) {
        return true;
    }
    std::filesystem::create_directories(parent, ec);
    return !ec;
}
inline bool CreateEmptyFile(const std::string& path) {
    CreateFullPath(path);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    return f.good();
}
inline bool Delete(const std::string& path) {
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}
inline bool DeleteDir(const std::string& path) { return Delete(path); }
inline bool DeleteDirRecursively(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    return !ec;
}
inline bool Rename(const std::string& from, const std::string& to) {
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    return !ec;
}
inline u64 GetSize(const std::string& path) {
    std::error_code ec;
    return static_cast<u64>(std::filesystem::file_size(path, ec));
}

struct FSTEntry {
    std::string physicalName;
    bool isDirectory = false;
    u64 size = 0;
    std::vector<FSTEntry> children;
};

using DirectoryEntryCallable = std::function<bool(const std::filesystem::directory_entry&)>;

inline bool ForeachDirectoryEntry(const std::string& path, const DirectoryEntryCallable& cb) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        if (ec) {
            return false;
        }
        if (!cb(entry)) {
            break;
        }
    }
    return !ec;
}

inline bool ScanDirectoryTree(const std::string& path, FSTEntry& out) {
    out.physicalName = path;
    out.isDirectory = IsDirectory(path);
    if (!out.isDirectory) {
        out.size = GetSize(path);
        return true;
    }
    return ForeachDirectoryEntry(path, [&](const std::filesystem::directory_entry& entry) {
        FSTEntry child{};
        ScanDirectoryTree(entry.path().string(), child);
        out.children.push_back(std::move(child));
        return true;
    });
}

inline std::string SplitFilename83(const std::string& filename) { return filename; }

template <std::ios_base::openmode Mode, class Stream>
inline void OpenFStream(Stream& stream, const std::string& path) {
    stream.open(path, Mode);
}

} // namespace FileUtil
