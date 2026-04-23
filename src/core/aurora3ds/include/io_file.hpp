#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>

class IOFile {
	FILE* handle = nullptr;
	static inline std::filesystem::path appData = "";       // Per-title directory on virtual NAND.
	static inline std::filesystem::path userRoot = "";      // Root directory containing virtual NAND/SDMC/sysdata.
	static inline std::filesystem::path nandRoot = "";
	static inline std::filesystem::path nandDataRoot = "";
	static inline std::filesystem::path sdmcRoot = "";
	static inline std::filesystem::path sharedFilesRoot = "";
	static inline std::filesystem::path sysDataRoot = "";

  public:
	IOFile() : handle(nullptr) {}
	IOFile(FILE* handle) : handle(handle) {}
	IOFile(const std::filesystem::path& path, const char* permissions = "rb");

	bool isOpen() { return handle != nullptr; }
	bool open(const std::filesystem::path& path, const char* permissions = "rb");
	bool open(const char* filename, const char* permissions = "rb");
	void close();

	std::pair<bool, std::size_t> read(void* data, std::size_t length, std::size_t dataSize);
	std::pair<bool, std::size_t> readBytes(void* data, std::size_t count);

	std::pair<bool, std::size_t> write(const void* data, std::size_t length, std::size_t dataSize);
	std::pair<bool, std::size_t> writeBytes(const void* data, std::size_t count);

	std::optional<std::uint64_t> size();

	bool seek(std::int64_t offset, int origin = SEEK_SET);
	bool rewind();
	bool flush();
	FILE* getHandle();
	static void setUserDataDir(const std::filesystem::path& dir);
	static void setAppDataDir(const std::filesystem::path& dir);
	static std::filesystem::path getUserData() { return userRoot; }
	static std::filesystem::path getAppData() { return appData; }
	static std::filesystem::path getNAND() { return nandRoot; }
	static std::filesystem::path getNANDData() { return nandDataRoot; }
	static std::filesystem::path getSDMC() { return sdmcRoot; }
	static std::filesystem::path getSharedFiles() { return sharedFilesRoot; }
	static std::filesystem::path getSysData() { return sysDataRoot; }

	// Sets the size of the file to "size" and returns whether it succeeded or not
	bool setSize(std::uint64_t size);
};
