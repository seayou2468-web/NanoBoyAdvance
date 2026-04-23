#include "../../../include/platform/shared_font_bundle.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if !defined(__APPLE__) || !TARGET_OS_IPHONE

namespace Platform {
	std::optional<std::filesystem::path> GetSharedFontReplacementBundlePath() {
		return std::nullopt;
	}
}  // namespace Platform

#endif
