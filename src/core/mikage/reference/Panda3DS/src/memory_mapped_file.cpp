#include "memory_mapped_file.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

MemoryMappedFile::MemoryMappedFile() : opened(false), filePath(""), pointer(nullptr) {}
MemoryMappedFile::MemoryMappedFile(const std::filesystem::path& path) { open(path); }
MemoryMappedFile::~MemoryMappedFile() { close(); }

// TODO: This should probably also return the error one way or another eventually
bool MemoryMappedFile::open(const std::filesystem::path& path) {
	fd = ::open(path.c_str(), O_RDWR);
	if (fd < 0) {
		opened = false;
		return false;
	}

	struct stat st{};
	if (fstat(fd, &st) != 0 || st.st_size <= 0) {
		::close(fd);
		fd = -1;
		opened = false;
		return false;
	}

	mappingSize = static_cast<size_t>(st.st_size);
	void* mapped = mmap(nullptr, mappingSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapped == MAP_FAILED) {
		::close(fd);
		fd = -1;
		mappingSize = 0;
		opened = false;
		return false;
	}

	filePath = path;
	pointer = static_cast<u8*>(mapped);
	opened = true;
	return true;
}

void MemoryMappedFile::close() {
	if (opened) {
		opened = false;
		if (pointer != nullptr && mappingSize > 0) {
			munmap(pointer, mappingSize);
		}
		pointer = nullptr;
		mappingSize = 0;

		if (fd >= 0) {
			::close(fd);
			fd = -1;
		}
	}
}

std::error_code MemoryMappedFile::flush() {
	if (!opened || pointer == nullptr || mappingSize == 0) {
		return std::make_error_code(std::errc::bad_file_descriptor);
	}
	if (msync(pointer, mappingSize, MS_SYNC) != 0) {
		return std::error_code(errno, std::generic_category());
	}
	return {};
}
