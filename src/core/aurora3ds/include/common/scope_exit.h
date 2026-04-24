#pragma once

#include <utility>

namespace Common {

template <class F>
class ScopeExit {
public:
    explicit ScopeExit(F&& fn) : fn_(std::forward<F>(fn)), active_(true) {}
    ScopeExit(ScopeExit&& other) noexcept : fn_(std::move(other.fn_)), active_(other.active_) {
        other.active_ = false;
    }
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ~ScopeExit() {
        if (active_) {
            fn_();
        }
    }

private:
    F fn_;
    bool active_;
};

template <class F>
ScopeExit<F> MakeScopeExit(F&& fn) {
    return ScopeExit<F>(std::forward<F>(fn));
}

} // namespace Common

#define CONCATENATE_INNER(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_INNER(x, y)
#define SCOPE_EXIT(code) auto CONCATENATE(scope_exit_, __LINE__) = ::Common::MakeScopeExit([&]() code)
