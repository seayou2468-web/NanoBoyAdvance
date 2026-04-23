#pragma once

#include <functional>

#include "./aac.hpp"

namespace Audio::AAC {
	class Decoder {
		using PaddrCallback = std::function<u8*(u32)>;

	  public:
		void decode(AAC::Message& response, const AAC::Message& request, PaddrCallback paddrCallback, bool enableAudio = true);
		~Decoder();
	};
}  // namespace Audio::AAC
