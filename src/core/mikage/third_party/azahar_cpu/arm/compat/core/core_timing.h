#pragma once

#include <cstdint>
#include <memory>

namespace Core::Timing {
class Timer {
  public:
	virtual ~Timer() = default;
	virtual int64_t GetDowncount() const { return 0; }
	virtual void AddTicks(uint64_t) {}
};
}
