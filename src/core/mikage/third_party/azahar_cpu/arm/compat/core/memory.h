#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Memory {
class PageTable;

class MemorySystem {
  public:
	virtual ~MemorySystem() = default;

	virtual std::uint8_t Read8(std::uint32_t address) const = 0;
	virtual std::uint16_t Read16(std::uint32_t address) const = 0;
	virtual std::uint32_t Read32(std::uint32_t address) const = 0;
	virtual std::uint64_t Read64(std::uint32_t address) const = 0;

	virtual void Write8(std::uint32_t address, std::uint8_t value) = 0;
	virtual void Write16(std::uint32_t address, std::uint16_t value) = 0;
	virtual void Write32(std::uint32_t address, std::uint32_t value) = 0;
	virtual void Write64(std::uint32_t address, std::uint64_t value) = 0;

	virtual std::shared_ptr<PageTable> GetCurrentPageTable() const { return nullptr; }

	virtual bool WriteExclusive8(std::uint32_t vaddr, std::uint8_t value, std::uint8_t expected) {
		if (Read8(vaddr) != expected) {
			return false;
		}
		Write8(vaddr, value);
		return true;
	}

	virtual bool WriteExclusive16(std::uint32_t vaddr, std::uint16_t value, std::uint16_t expected) {
		if (Read16(vaddr) != expected) {
			return false;
		}
		Write16(vaddr, value);
		return true;
	}

	virtual bool WriteExclusive32(std::uint32_t vaddr, std::uint32_t value, std::uint32_t expected) {
		if (Read32(vaddr) != expected) {
			return false;
		}
		Write32(vaddr, value);
		return true;
	}

	virtual bool WriteExclusive64(std::uint32_t vaddr, std::uint64_t value, std::uint64_t expected) {
		if (Read64(vaddr) != expected) {
			return false;
		}
		Write64(vaddr, value);
		return true;
	}
};
}
