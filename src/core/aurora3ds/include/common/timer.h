#pragma once

#include <chrono>

namespace Common {

class Timer {
public:
    using clock = std::chrono::steady_clock;

    Timer() { Start(); }

    void Start() {
        start_ = clock::now();
        last_ = start_;
    }

    void Update() {
        last_ = clock::now();
    }

    std::chrono::milliseconds GetTimeElapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start_);
    }

    std::chrono::milliseconds GetTimeDifference() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - last_);
    }

private:
    clock::time_point start_{};
    clock::time_point last_{};
};

} // namespace Common
