#include "../../../include/services/amiibo_device.hpp"
#include "../../../include/services/nfc_types.hpp"
#include <cstring>

void AmiiboDevice::reset() {
	encrypted = false;
	loaded = false;
	modelNumber = 0;
	characterID = 0;
	series = 0;
	uid.fill(0);
}

// Load amiibo information from our raw 540 byte array
void AmiiboDevice::loadFromRaw() {
	Service::NFC::NTAG215File tag {};
	std::memcpy(&tag, raw.data(), sizeof(tag));

	uid = tag.uid;
	characterID = tag.model_info.character_id;
	modelNumber = tag.model_info.model_number;
	series = static_cast<u8>(tag.model_info.series);
	encrypted = false;
	loaded = true;
}
