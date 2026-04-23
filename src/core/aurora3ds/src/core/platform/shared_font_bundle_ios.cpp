#include "../../../include/platform/shared_font_bundle.hpp"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE

#include <CoreFoundation/CoreFoundation.h>

namespace Platform {
	std::optional<std::filesystem::path> GetSharedFontReplacementBundlePath() {
		CFBundleRef bundle = CFBundleGetMainBundle();
		if (bundle == nullptr) {
			return std::nullopt;
		}

		CFStringRef resourceName = CFSTR("SharedFontReplacement");
		CFStringRef resourceType = CFSTR("bin");
		CFStringRef subdir = CFSTR("fonts");
		CFURLRef resourceURL = CFBundleCopyResourceURL(bundle, resourceName, resourceType, subdir);
		if (resourceURL == nullptr) {
			return std::nullopt;
		}

		CFStringRef pathRef = CFURLCopyFileSystemPath(resourceURL, kCFURLPOSIXPathStyle);
		CFRelease(resourceURL);
		if (pathRef == nullptr) {
			return std::nullopt;
		}

		char pathBuffer[4096];
		const Boolean ok = CFStringGetCString(pathRef, pathBuffer, sizeof(pathBuffer), kCFStringEncodingUTF8);
		CFRelease(pathRef);
		if (!ok) {
			return std::nullopt;
		}

		return std::filesystem::path(pathBuffer);
	}
}  // namespace Platform

#endif
