#pragma once

#include "helpers.hpp"

namespace CRC {
	// CRC-16 with polynomial 0x1021, initial remainder 0, final XOR 0, no input/output reflection.
	// This matches boost::crc<16, 0x1021, 0, 0, false, false>.
	static inline u16 crc16_1021_no_reflect(const void* data, usize size) {
		const auto* bytes = static_cast<const u8*>(data);
		u16 remainder = 0;

		for (usize i = 0; i < size; i++) {
			remainder ^= u16(bytes[i]) << 8;

			for (int bit = 0; bit < 8; bit++) {
				if (remainder & 0x8000) {
					remainder = u16((remainder << 1) ^ 0x1021);
				} else {
					remainder <<= 1;
				}
			}
		}

		return remainder;
	}
}  // namespace CRC
