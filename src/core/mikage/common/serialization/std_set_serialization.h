// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version

#pragma once

// Standard set serialization support (replaces boost/serialization/set.hpp)
// Uses PointerWrap framework from chunk_file.h for container serialization

#include <set>

// Note: Container serialization is handled by PointerWrap::Do() template
// specializations in chunk_file.h. This file is retained for API compatibility
// and can be extended with additional container support macros if needed.

