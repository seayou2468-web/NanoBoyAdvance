#pragma once

#include <algorithm>
#include <simd/simd.h>

#include "helpers.hpp"
#include "services/hid.hpp"

// Convert generic mobile sensor readings to 3DS format.
// Android values are expected in m/s^2 for acceleration and rad/s for rotation.
namespace Sensors {
	// Convert rotation sensor data to HID-ready values.
	// Returns [pitch, roll, yaw]
	static simd_float3 convertRotation(simd_float3 rotation) {
		// Annoyingly, Android doesn't support the <numbers> header yet so we define pi ourselves
		static constexpr double pi = 3.141592653589793;
		// Convert the rotation from rad/s to deg/s and scale by the gyroscope coefficient in HID
		constexpr float scale = 180.f / pi * HIDService::gyroscopeCoeff;
		// The axes are also inverted, so invert scale before the multiplication.
		return rotation * (-scale);
	}

	static simd_float3 convertAcceleration(float* data) {
		// Set our cap to ~9 m/s^2. The 3DS sensors cap at -930 and +930, so values above this value will get clamped to 930
		// At rest (3DS laid flat on table), hardware reads around ~0 for x and z axis, and around ~480 for y axis due to gravity.
		// This code tries to mimic this approximately, with offsets based on measurements from my DualShock 4.
		static constexpr float accelMax = 9.f;
		// Define standard gravity ourselves for consistent behavior across platforms.
		static constexpr float standardGravity = 9.80665f;

		s16 x = std::clamp<s16>(s16(data[0] / accelMax * 930.f), -930, +930);
		s16 y = std::clamp<s16>(s16(data[1] / (standardGravity * accelMax) * 930.f - 350.f), -930, +930);
		s16 z = std::clamp<s16>(s16((data[2] - 2.1f) / accelMax * 930.f), -930, +930);

		return simd_make_float3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
	}
}  // namespace Sensors
