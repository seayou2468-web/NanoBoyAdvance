// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <sys/mman.h>

#include "../../../include/host_memory/virtual_buffer.h"

namespace Common {
void* AllocateMemoryPages(std::size_t size) noexcept {
	void* base{mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0)};
	if (base == MAP_FAILED) {
		base = nullptr;
	}

	if (!base) {
		Helpers::panic("Failed to allocate memory pages");
	}

	return base;
}

void FreeMemoryPages(void* base, [[maybe_unused]] std::size_t size) noexcept {
	if (!base) {
		return;
	}
	if (munmap(base, size) != 0) {
		Helpers::panic("Failed to free memory pages");
	}
}

}  // namespace Common
