#pragma once

#include <filesystem>
#include <optional>

namespace Platform {
	std::optional<std::filesystem::path> GetSharedFontReplacementBundlePath();
}
