#pragma once

#include <cstdint>

namespace Core {
class System {
  public:
	virtual ~System() = default;
	virtual void AddTicks(std::uint64_t ticks) = 0;
	virtual void CallSVC(std::uint32_t svc) = 0;
};
}
