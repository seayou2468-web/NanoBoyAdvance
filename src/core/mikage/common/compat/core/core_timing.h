// iOS Compatibility Layer for Core Timing
// Provides simple cycle counting and timing
#pragma once

#include <cstdint>
#include <chrono>
#include <memory>

namespace Core::Timing {

// Simple timer for cycle timing
class Timer {
public:
    Timer() : downcount(0), start_time(std::chrono::high_resolution_clock::now()) {}
    
    int64_t GetDowncount() const {
        return downcount;
    }
    
    void SetDowncount(int64_t value) {
        downcount = value;
    }
    
    void AddTicks(int64_t ticks) {
        downcount -= ticks;
    }
    
    uint64_t GetElapsedCycles() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - start_time).count();
        // Approximate CPU cycles at 1GHz (1 cycle per nanosecond)
        return elapsed;
    }
    
private:
    int64_t downcount;
    std::chrono::high_resolution_clock::time_point start_time;
};

} // namespace Core::Timing
