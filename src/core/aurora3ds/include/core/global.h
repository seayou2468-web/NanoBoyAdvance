// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Core {

template <class T>
T& Global();

// Explicit specializations are provided by integration units when wired.
class System;
template <>
System& Global();

class Timing;
template <>
Timing& Global();

}  // namespace Core
