#pragma once
#include <filesystem>
#include <string>

#include "./audio/dsp_core.hpp"
#include "./renderer.hpp"
#include "./screen_layout.hpp"
#include "./services/region_codes.hpp"

struct AudioDeviceConfig {
	// Audio curve to use for volumes between 0-100
	enum class VolumeCurve : int {
		Cubic = 0,   // Samples are scaled by volume ^ 3
		Linear = 1,  // Samples are scaled by volume
	};

	float volumeRaw = 1.0f;
	VolumeCurve volumeCurve = VolumeCurve::Cubic;

	bool muteAudio = false;

	float getVolume() const {
		if (muteAudio) {
			return 0.0f;
		}

		return volumeRaw;
	}

	static VolumeCurve volumeCurveFromString(std::string inString);
	static const char* volumeCurveToString(VolumeCurve curve);
};

	// Remember to initialize every field here to its default value otherwise bad things will happen
	struct EmulatorConfig {
	static constexpr bool ubershaderDefault = false;
	static constexpr bool accelerateShadersDefault = true;
	static constexpr bool audioEnabledDefault = true;

	static constexpr bool hashTexturesDefault = false;
	static constexpr RendererType rendererDefault = RendererType::Software;

	static constexpr bool enableFastmemDefault = true;

	bool useUbershaders = ubershaderDefault;
	bool accelerateShaders = accelerateShadersDefault;
	bool fastmemEnabled = enableFastmemDefault;
	bool hashTextures = hashTexturesDefault;

	ScreenLayout::Layout screenLayout = ScreenLayout::Layout::Default;
	float topScreenSize = 0.5;

	bool accurateShaderMul = false;

	// Toggles whether to force shadergen when there's more than N lights active and we're using the ubershader, for better performance
	bool forceShadergenForLights = true;
	int lightShadergenThreshold = 1;

	RendererType rendererType = rendererDefault;
	Audio::DSPCore::Type dspType = Audio::DSPCore::Type::HLE;

	bool sdCardInserted = true;
	bool sdWriteProtected = false;
	bool circlePadProEnabled = true;
	bool isNew3DS = true;

	bool audioEnabled = audioEnabledDefault;
	bool vsyncEnabled = true;
	bool aacEnabled = true;  // Enable AAC audio?

	bool printDSPFirmware = false;

	bool chargerPlugged = true;
	// Default to 3% battery to make users suffer
	int batteryPercentage = 3;

	LanguageCodes systemLanguage = LanguageCodes::English;

	std::filesystem::path filePath;
	AudioDeviceConfig audioDeviceConfig;

	EmulatorConfig(const std::filesystem::path& path);
	void load();
	void save();
	static LanguageCodes languageCodeFromString(std::string inString);
	static const char* languageCodeToString(LanguageCodes code);
};
