#pragma once

#include "common/common_types.h"

namespace Core { class System; }

namespace GDBStub {
inline void SetHioRequest(Core::System&, VAddr) {}
} // namespace GDBStub
