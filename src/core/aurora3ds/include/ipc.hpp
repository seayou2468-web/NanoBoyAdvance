#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

#include "./bitfield.hpp"
#include "./common/common_types.h"

namespace IPC {

enum DescriptorType : u32 {
	StaticBuffer = 0x02,
	PXIBuffer = 0x04,
	MappedBuffer = 0x08,
	CopyHandle = 0x00,
	MoveHandle = 0x10,
	CallingPid = 0x20,
};

union Header {
	u32 raw;
	BitField<0, 6, u32> translate_params_size;
	BitField<6, 6, u32> normal_params_size;
	BitField<16, 16, u32> command_id;
};

inline constexpr u32 MakeHeader(u16 command_id, u32 normal_params_size, u32 translate_params_size) {
	Header header{};
	header.raw = 0;
	header.command_id = command_id;
	header.normal_params_size = normal_params_size;
	header.translate_params_size = translate_params_size;
	return header.raw;
}

constexpr u32 responseHeader(u32 commandID, u32 normalResponses, u32 translateResponses) {
	return MakeHeader(commandID, normalResponses, translateResponses);
}

constexpr u32 MoveHandleDesc(u32 num_handles = 1) {
	return MoveHandle | ((num_handles - 1) << 26);
}

constexpr u32 CopyHandleDesc(u32 num_handles = 1) {
	return CopyHandle | ((num_handles - 1) << 26);
}

constexpr u32 CallingPidDesc() {
	return CallingPid;
}

constexpr bool IsHandleDescriptor(u32 descriptor) {
	return (descriptor & 0xF) == 0x0;
}

constexpr u32 HandleNumberFromDesc(u32 handle_descriptor) {
	return (handle_descriptor >> 26) + 1;
}

union StaticBufferDescInfo {
	u32 raw;
	BitField<0, 4, u32> descriptor_type;
	BitField<10, 4, u32> buffer_id;
	BitField<14, 18, u32> size;
};

inline u32 StaticBufferDesc(std::size_t size, u8 buffer_id) {
	StaticBufferDescInfo info{};
	info.raw = 0;
	info.descriptor_type = StaticBuffer;
	info.buffer_id = buffer_id;
	info.size = static_cast<u32>(size);
	return info.raw;
}

inline u32 PXIBufferDesc(u32 size, unsigned buffer_id, bool is_read_only) {
	u32 type = PXIBuffer;
	if (is_read_only)
		type |= 0x2;
	return type | (size << 8) | ((buffer_id & 0xF) << 4);
}

enum MappedBufferPermissions : u32 {
	R = 1,
	W = 2,
	RW = R | W,
};

union MappedBufferDescInfo {
	u32 raw;
	BitField<0, 4, u32> flags;
	BitField<1, 2, MappedBufferPermissions> perms;
	BitField<4, 28, u32> size;
};

inline u32 MappedBufferDesc(std::size_t size, MappedBufferPermissions perms) {
	MappedBufferDescInfo info{};
	info.raw = 0;
	info.flags = MappedBuffer;
	info.perms = perms;
	info.size = static_cast<u32>(size);
	return info.raw;
}

namespace BufferType {
	enum : std::uint32_t {
		Send = 1,
		Receive = 2,
	};
}

class RequestHelperBase {
  protected:
	u32 messagePointer;
	class Memory& mem;
	std::size_t index = 0;
	Header header;

  public:
	RequestHelperBase(u32 ptr, class Memory& memory) : messagePointer(ptr), mem(memory) {
		header.raw = memory.read32(ptr);
		index = 1;
	}

	RequestHelperBase(u32 ptr, class Memory& memory, u32 h) : messagePointer(ptr), mem(memory) {
		header.raw = h;
		index = 0;
	}

	std::size_t TotalSize() const {
		return 1 + header.normal_params_size + header.translate_params_size;
	}

	u32 GetCommandID() const { return header.command_id; }
};

class RequestBuilder : public RequestHelperBase {
  public:
	RequestBuilder(u32 ptr, class Memory& memory, u32 h) : RequestHelperBase(ptr, memory, h) {
		mem.write32(messagePointer, h);
		index = 1;
	}

	void Push(u32 value) {
		mem.write32(messagePointer + (index++ * 4), value);
	}

	template <typename T>
	void PushRaw(const T& value) {
		static_assert(std::is_trivially_copyable<T>(), "Raw types should be trivially copyable");
		const u32 words = (sizeof(T) + 3) / 4;
		for (u32 i = 0; i < words; ++i) {
			u32 word = 0;
			std::memcpy(&word, reinterpret_cast<const u8*>(&value) + (i * 4), std::min<size_t>(4, sizeof(T) - (i * 4)));
			Push(word);
		}
	}

	void Push(u64 value) {
		Push(static_cast<u32>(value & 0xFFFFFFFF));
		Push(static_cast<u32>(value >> 32));
	}

	void Push(bool value) {
		Push(static_cast<u32>(value ? 1 : 0));
	}
};

class RequestParser : public RequestHelperBase {
  public:
	RequestParser(u32 ptr, class Memory& memory) : RequestHelperBase(ptr, memory) {}

	u32 Pop() {
		return mem.read32(messagePointer + (index++ * 4));
	}

	template <typename T>
	T PopRaw() {
		static_assert(std::is_trivially_copyable<T>(), "Raw types should be trivially copyable");
		T value;
		const u32 words = (sizeof(T) + 3) / 4;
		std::vector<u32> buffer(words);
		for (u32 i = 0; i < words; ++i) {
			buffer[i] = Pop();
		}
		std::memcpy(&value, buffer.data(), sizeof(T));
		return value;
	}

	u64 Pop64() {
		u32 low = Pop();
		u32 high = Pop();
		return (static_cast<u64>(high) << 32) | low;
	}

	void Skip(u32 words) {
		index += words;
	}

	RequestBuilder MakeBuilder(u32 normal_params_size, u32 translate_params_size) {
		return RequestBuilder(messagePointer, mem, MakeHeader(header.command_id, normal_params_size, translate_params_size));
	}
};

}  // namespace IPC
