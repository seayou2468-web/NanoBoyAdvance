// SPDX-FileCopyrightText: Copyright 2025 Panda3DS Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "host_memory/host_memory.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "helpers.hpp"

namespace Common {

HostMemory::HostMemory(size_t backing_size_, size_t virtual_size_, bool useFastmem)
	: backing_size(backing_size_), virtual_size(virtual_size_) {
	fallback_buffer = std::make_unique<Common::VirtualBuffer<u8>>(backing_size);
	backing_base = fallback_buffer->data();
	virtual_base = nullptr;
	virtual_base_offset = 0;

	if (useFastmem) {
		Helpers::warn("HostMemory fastmem mapping is unavailable on this iOS-focused build; using fallback buffer");
	}
}

HostMemory::~HostMemory() = default;

HostMemory::HostMemory(HostMemory&& other) noexcept
	: backing_size{std::exchange(other.backing_size, 0)},
	  virtual_size{std::exchange(other.virtual_size, 0)},
	  backing_base{std::exchange(other.backing_base, nullptr)},
	  virtual_base{std::exchange(other.virtual_base, nullptr)},
	  virtual_base_offset{std::exchange(other.virtual_base_offset, 0)},
	  fallback_buffer{std::move(other.fallback_buffer)} {}

HostMemory& HostMemory::operator=(HostMemory&& other) noexcept {
	if (this == &other) {
		return *this;
	}

	backing_size = std::exchange(other.backing_size, 0);
	virtual_size = std::exchange(other.virtual_size, 0);
	backing_base = std::exchange(other.backing_base, nullptr);
	virtual_base = std::exchange(other.virtual_base, nullptr);
	virtual_base_offset = std::exchange(other.virtual_base_offset, 0);
	fallback_buffer = std::move(other.fallback_buffer);
	return *this;
}

void HostMemory::Map([[maybe_unused]] size_t virtual_offset, [[maybe_unused]] size_t host_offset,
	[[maybe_unused]] size_t length, [[maybe_unused]] MemoryPermission perms, [[maybe_unused]] bool separate_heap) {
	// iOS-focused fallback path intentionally does not map a 1:1 4GiB mirrored virtual arena.
}

void HostMemory::Unmap([[maybe_unused]] size_t virtual_offset, [[maybe_unused]] size_t length, [[maybe_unused]] bool separate_heap) {
	// iOS-focused fallback path intentionally does not map a 1:1 4GiB mirrored virtual arena.
}

void HostMemory::Protect([[maybe_unused]] size_t virtual_offset, [[maybe_unused]] size_t length, [[maybe_unused]] MemoryPermission perms) {
	// No-op on fallback buffer.
}

void HostMemory::EnableDirectMappedAddress() {
	// Unsupported in fallback mode.
}

void HostMemory::ClearBackingRegion(size_t physical_offset, size_t length, u32 fill_value) {
	if (physical_offset >= backing_size) {
		return;
	}

	const size_t safe_length = std::min(length, backing_size - physical_offset);
	std::memset(backing_base + physical_offset, static_cast<int>(fill_value & 0xFF), safe_length);
}

}  // namespace Common
