// Copyright Aurora Project
// Licensed under GPLv2 or any later version

#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace aurora::icl {

template <typename T>
class right_open_interval {
public:
    using value_type = T;

    constexpr right_open_interval() = default;
    constexpr right_open_interval(T lower, T upper) : lower_(lower), upper_(upper) {}

    constexpr T lower() const {
        return lower_;
    }

    constexpr T upper() const {
        return upper_;
    }

private:
    T lower_{};
    T upper_{};
};

template <typename T>
constexpr T first(const right_open_interval<T>& interval) {
    return interval.lower();
}

template <typename T>
constexpr T lower(const right_open_interval<T>& interval) {
    return interval.lower();
}

template <typename T>
constexpr T last_next(const right_open_interval<T>& interval) {
    return interval.upper();
}

template <typename T>
constexpr T length(const right_open_interval<T>& interval) {
    return interval.upper() > interval.lower() ? interval.upper() - interval.lower() : 0;
}

template <typename T>
constexpr bool is_empty(const right_open_interval<T>& interval) {
    return length(interval) == 0;
}

template <typename T>
constexpr right_open_interval<T> operator&(const right_open_interval<T>& lhs,
                                           const right_open_interval<T>& rhs) {
    const T begin = std::max(lhs.lower(), rhs.lower());
    const T end = std::min(lhs.upper(), rhs.upper());
    return {begin, end < begin ? begin : end};
}

template <typename T, typename Compare = std::less<T>,
          typename Interval = right_open_interval<T>>
class interval_set {
public:
    using value_type = Interval;
    using container_type = std::vector<Interval>;
    using iterator = typename container_type::const_iterator;

    interval_set() = default;
    explicit interval_set(const Interval& interval) {
        Add(interval);
    }

    bool empty() const {
        return intervals_.empty();
    }

    iterator begin() const {
        return intervals_.begin();
    }

    iterator end() const {
        return intervals_.end();
    }

    void clear() {
        intervals_.clear();
    }

    interval_set& operator+=(const Interval& interval) {
        Add(interval);
        return *this;
    }

    interval_set& erase(const Interval& interval) {
        Subtract(interval);
        return *this;
    }

    friend interval_set operator&(const interval_set& set, const Interval& interval) {
        interval_set result;
        for (const auto& entry : set.intervals_) {
            const auto intersect = entry & interval;
            if (!is_empty(intersect)) {
                result.Add(intersect);
            }
        }
        return result;
    }

    friend interval_set operator-(const interval_set& lhs, const interval_set& rhs) {
        interval_set result = lhs;
        for (const auto& interval : rhs.intervals_) {
            result.Subtract(interval);
        }
        return result;
    }

    friend bool is_empty(const interval_set& set) {
        return set.empty();
    }

private:
    void Add(const Interval& interval) {
        if (is_empty(interval)) {
            return;
        }
        intervals_.push_back(interval);
        Normalize();
    }

    void Subtract(const Interval& interval) {
        if (is_empty(interval)) {
            return;
        }
        container_type out;
        for (const auto& current : intervals_) {
            const auto intersect = current & interval;
            if (is_empty(intersect)) {
                out.push_back(current);
                continue;
            }
            if (current.lower() < intersect.lower()) {
                out.emplace_back(current.lower(), intersect.lower());
            }
            if (intersect.upper() < current.upper()) {
                out.emplace_back(intersect.upper(), current.upper());
            }
        }
        intervals_ = std::move(out);
    }

    void Normalize() {
        std::sort(intervals_.begin(), intervals_.end(), [](const Interval& a, const Interval& b) {
            return a.lower() < b.lower() || (a.lower() == b.lower() && a.upper() < b.upper());
        });
        container_type merged;
        for (const auto& interval : intervals_) {
            if (merged.empty() || merged.back().upper() < interval.lower()) {
                merged.push_back(interval);
            } else {
                merged.back() =
                    Interval{merged.back().lower(), std::max(merged.back().upper(), interval.upper())};
            }
        }
        intervals_ = std::move(merged);
    }

    container_type intervals_;
};

struct partial_absorber {};
struct inplace_plus {};
struct inter_section {};

template <typename K, typename V, typename Absorber = partial_absorber, typename Compare = std::less<K>,
          typename Combine = inplace_plus, typename Section = inter_section,
          typename Interval = right_open_interval<K>>
class interval_map {
public:
    using value_type = std::pair<Interval, V>;
    using container_type = std::vector<value_type>;
    using iterator = typename container_type::const_iterator;

    void clear() {
        entries_.clear();
    }

    void set(const value_type& value) {
        erase(value.first);
        if (!is_empty(value.first)) {
            entries_.push_back(value);
        }
    }

    void add(const value_type& value) {
        if (is_empty(value.first)) {
            return;
        }
        entries_.push_back(value);
    }

    void erase(const Interval& interval) {
        if (is_empty(interval)) {
            return;
        }
        container_type out;
        for (const auto& [current, value] : entries_) {
            const auto intersect = current & interval;
            if (is_empty(intersect)) {
                out.emplace_back(current, value);
                continue;
            }
            if (current.lower() < intersect.lower()) {
                out.emplace_back(Interval{current.lower(), intersect.lower()}, value);
            }
            if (intersect.upper() < current.upper()) {
                out.emplace_back(Interval{intersect.upper(), current.upper()}, value);
            }
        }
        entries_ = std::move(out);
    }

    interval_map& operator-=(const Interval& interval) {
        erase(interval);
        return *this;
    }

    template <typename T, typename Cmp, typename Intv>
    interval_map& operator-=(const interval_set<T, Cmp, Intv>& set) {
        for (const auto& interval : set) {
            erase(Interval{interval.lower(), interval.upper()});
        }
        return *this;
    }

    iterator find(const Interval& interval) const {
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (!is_empty(it->first & interval)) {
                return it;
            }
        }
        return entries_.end();
    }

    std::pair<iterator, iterator> equal_range(const Interval& interval) const {
        BuildScratch(interval);
        return {scratch_.begin(), scratch_.end()};
    }

    iterator end() const {
        return entries_.end();
    }

private:
    void BuildScratch(const Interval& interval) const {
        scratch_.clear();
        for (const auto& [current, value] : entries_) {
            const auto intersect = current & interval;
            if (!is_empty(intersect)) {
                scratch_.emplace_back(intersect, value);
            }
        }
    }

    container_type entries_;
    mutable container_type scratch_;
};

} // namespace aurora::icl
