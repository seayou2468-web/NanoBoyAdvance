#pragma once

#include <cstdint>
#include <memory>

namespace Core::Timing {
class Timer {
  public:
	int64_t GetDowncount() const { return 0; }
	void AddTicks(uint64_t) {}
};
}
