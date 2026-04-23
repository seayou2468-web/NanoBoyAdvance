#pragma once
#include <array>

#include "../helpers.hpp"
#include "../io_file.hpp"
#include "./nfc_types.hpp"

class AmiiboDevice {
	bool loaded = false;
	bool encrypted = false;
	u32 modelNumber = 0;
	u16 characterID = 0;
	u8 series = 0;
	std::array<u8, 7> uid {};

  public:
	static constexpr size_t tagSize = 0x21C;
	std::array<u8, tagSize> raw;

	void loadFromRaw();
	void reset();
	bool isLoaded() const { return loaded; }
	u32 getModelNumber() const { return modelNumber; }
	u16 getCharacterID() const { return characterID; }
	u8 getSeries() const { return series; }
	const std::array<u8, 7>& getUID() const { return uid; }
};
