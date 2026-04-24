#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <tuple>

namespace Common {

template <typename T, std::size_t NumPriorities>
class ThreadQueueList {
public:
    using Priority = std::uint32_t;

    void prepare(Priority priority) {
        if (priority >= NumPriorities) {
            return;
        }
        highest_priority = (highest_priority > priority) ? priority : highest_priority;
    }

    void push_back(Priority priority, const T& value) {
        if (priority >= NumPriorities) {
            return;
        }
        queues[priority].push_back(value);
        highest_priority = (highest_priority > priority) ? priority : highest_priority;
    }

    void push_front(Priority priority, const T& value) {
        if (priority >= NumPriorities) {
            return;
        }
        queues[priority].push_front(value);
        highest_priority = (highest_priority > priority) ? priority : highest_priority;
    }

    bool remove(Priority priority, const T& value) {
        if (priority >= NumPriorities) {
            return false;
        }

        auto& queue = queues[priority];
        for (auto it = queue.begin(); it != queue.end(); ++it) {
            if (*it == value) {
                queue.erase(it);
                RecomputeHighestPriority();
                return true;
            }
        }

        return false;
    }

    void move(const T& value, Priority from, Priority to) {
        remove(from, value);
        push_back(to, value);
    }

    Priority contains(const T& value) const {
        for (Priority priority = 0; priority < NumPriorities; ++priority) {
            const auto& queue = queues[priority];
            for (const auto& queued : queue) {
                if (queued == value) {
                    return priority;
                }
            }
        }
        return std::numeric_limits<Priority>::max();
    }

    std::tuple<Priority, T> pop_first() {
        for (Priority priority = highest_priority; priority < NumPriorities; ++priority) {
            auto& queue = queues[priority];
            if (!queue.empty()) {
                T value = queue.front();
                queue.pop_front();
                RecomputeHighestPriority();
                return {priority, value};
            }
        }

        return {0, T{}};
    }

    std::tuple<Priority, T> pop_first_better(Priority priority_limit) {
        Priority end = (priority_limit > NumPriorities) ? static_cast<Priority>(NumPriorities)
                                                        : priority_limit;
        for (Priority priority = highest_priority; priority < end; ++priority) {
            auto& queue = queues[priority];
            if (!queue.empty()) {
                T value = queue.front();
                queue.pop_front();
                RecomputeHighestPriority();
                return {priority, value};
            }
        }

        return {0, T{}};
    }

    T get_first() const {
        for (Priority priority = highest_priority; priority < NumPriorities; ++priority) {
            const auto& queue = queues[priority];
            if (!queue.empty()) {
                return queue.front();
            }
        }
        return T{};
    }

private:
    void RecomputeHighestPriority() {
        highest_priority = static_cast<Priority>(NumPriorities);
        for (Priority priority = 0; priority < NumPriorities; ++priority) {
            if (!queues[priority].empty()) {
                highest_priority = priority;
                return;
            }
        }
    }

    std::array<std::deque<T>, NumPriorities> queues{};
    Priority highest_priority = static_cast<Priority>(NumPriorities);
};

} // namespace Common
