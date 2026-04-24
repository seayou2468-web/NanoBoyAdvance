// Copyright Aurora3DS Project
// Licensed under GPLv2 or any later version

#pragma once

#include <algorithm>
#include <vector>
#include "common/serialization/serialization_alias.hpp"

namespace Common {

enum class IntervalBounds {
    RightOpen,
};

template <typename T>
struct Interval {
    Interval() = default;
    Interval(T lower_, T upper_) : lower_(lower_), upper_(upper_) {}

    static Interval right_open(T lower, T upper) {
        return Interval(lower, upper);
    }

    [[nodiscard]] T lower() const {
        return lower_;
    }

    [[nodiscard]] T upper() const {
        return upper_;
    }

    [[nodiscard]] IntervalBounds bounds() const {
        return IntervalBounds::RightOpen;
    }

private:
    T lower_{};
    T upper_{};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & lower_;
        ar & upper_;
    }

    friend class Serialization::access;
};

template <typename T>
class IntervalSet {
public:
    using interval_type = Interval<T>;
    using Storage = std::vector<interval_type>;
    using const_iterator = typename Storage::const_iterator;
    using const_reverse_iterator = typename Storage::const_reverse_iterator;

    void clear() {
        intervals.clear();
    }

    void insert(const interval_type& interval) {
        *this += interval;
    }

    IntervalSet& operator+=(const interval_type& interval) {
        if (interval.lower() >= interval.upper()) {
            return *this;
        }

        intervals.push_back(interval);
        Normalize();
        return *this;
    }

    IntervalSet& operator+=(const IntervalSet& other) {
        for (const auto& interval : other.intervals) {
            *this += interval;
        }
        return *this;
    }

    IntervalSet& operator-=(const interval_type& interval) {
        if (interval.lower() >= interval.upper()) {
            return *this;
        }

        Storage next;
        next.reserve(intervals.size());
        for (const auto& current : intervals) {
            if (interval.upper() <= current.lower() || interval.lower() >= current.upper()) {
                next.push_back(current);
                continue;
            }

            if (interval.lower() > current.lower()) {
                next.emplace_back(current.lower(), interval.lower());
            }
            if (interval.upper() < current.upper()) {
                next.emplace_back(interval.upper(), current.upper());
            }
        }
        intervals = std::move(next);
        return *this;
    }

    IntervalSet& operator-=(const IntervalSet& other) {
        for (const auto& interval : other.intervals) {
            *this -= interval;
        }
        return *this;
    }

    [[nodiscard]] bool empty() const {
        return intervals.empty();
    }

    const_iterator begin() const {
        return intervals.begin();
    }
    const_iterator end() const {
        return intervals.end();
    }
    const_reverse_iterator rbegin() const {
        return intervals.rbegin();
    }
    const_reverse_iterator rend() const {
        return intervals.rend();
    }

    [[nodiscard]] const Storage& data() const {
        return intervals;
    }

private:
    Storage intervals;

    void Normalize() {
        if (intervals.empty()) {
            return;
        }

        std::sort(intervals.begin(), intervals.end(),
                  [](const interval_type& lhs, const interval_type& rhs) {
                      return lhs.lower() < rhs.lower() ||
                             (lhs.lower() == rhs.lower() && lhs.upper() < rhs.upper());
                  });

        Storage merged;
        merged.reserve(intervals.size());
        merged.push_back(intervals.front());
        for (std::size_t i = 1; i < intervals.size(); ++i) {
            auto& back = merged.back();
            const auto& current = intervals[i];
            if (current.lower() <= back.upper()) {
                back = interval_type(back.lower(), std::max(back.upper(), current.upper()));
            } else {
                merged.push_back(current);
            }
        }
        intervals = std::move(merged);
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & intervals;
    }

    friend class Serialization::access;
};

template <typename T>
bool contains(const IntervalSet<T>& set, const Interval<T>& interval) {
    for (const auto& current : set.data()) {
        if (current.lower() <= interval.lower() && current.upper() >= interval.upper()) {
            return true;
        }
    }
    return false;
}

template <typename T>
bool intersects(const IntervalSet<T>& set, const Interval<T>& interval) {
    for (const auto& current : set.data()) {
        if (interval.upper() > current.lower() && interval.lower() < current.upper()) {
            return true;
        }
    }
    return false;
}

} // namespace Common
