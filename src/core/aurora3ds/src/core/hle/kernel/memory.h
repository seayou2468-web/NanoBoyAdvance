// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <optional>
#include <vector>
#include "../boost_compat.h"
#include "common/common_types.h"

namespace Kernel {

struct AddressMapping;
class VMManager;

struct MemoryRegionInfo {
    u32 base; // Not an address, but offset from start of FCRAM
    u32 size;
    u32 used;

    struct Interval {
        enum class Bounds : u8 { RightOpen };

        u32 lower_bound{};
        u32 upper_bound{};

        Interval() = default;
        Interval(u32 lower, u32 upper) : lower_bound(lower), upper_bound(upper) {}

        static Interval right_open(u32 lower, u32 upper) {
            return Interval{lower, upper};
        }

        u32 lower() const {
            return lower_bound;
        }
        u32 upper() const {
            return upper_bound;
        }
        Bounds bounds() const {
            return Bounds::RightOpen;
        }

    private:
        friend class aurora::serialization::access;
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & lower_bound;
            ar & upper_bound;
        }
    };

    class IntervalSet {
    public:
        using interval_type = Interval;
        using container_type = std::vector<interval_type>;
        using iterator = container_type::iterator;
        using const_iterator = container_type::const_iterator;
        using reverse_iterator = container_type::reverse_iterator;
        using const_reverse_iterator = container_type::const_reverse_iterator;

        void clear() {
            intervals.clear();
        }

        void insert(const interval_type& interval) {
            AddInterval(interval);
        }

        IntervalSet& operator+=(const interval_type& interval) {
            AddInterval(interval);
            return *this;
        }

        IntervalSet& operator+=(const IntervalSet& other) {
            for (const auto& interval : other.intervals) {
                AddInterval(interval);
            }
            return *this;
        }

        IntervalSet& operator-=(const interval_type& interval) {
            SubtractInterval(interval);
            return *this;
        }

        IntervalSet& operator-=(const IntervalSet& other) {
            for (const auto& interval : other.intervals) {
                SubtractInterval(interval);
            }
            return *this;
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

    private:
        void AddInterval(const interval_type& interval) {
            if (interval.lower() >= interval.upper()) {
                return;
            }

            interval_type merged = interval;
            container_type out;
            bool inserted = false;

            for (const auto& current : intervals) {
                if (current.upper() < merged.lower()) {
                    out.push_back(current);
                } else if (merged.upper() < current.lower()) {
                    if (!inserted) {
                        out.push_back(merged);
                        inserted = true;
                    }
                    out.push_back(current);
                } else {
                    merged.lower_bound = std::min(merged.lower(), current.lower());
                    merged.upper_bound = std::max(merged.upper(), current.upper());
                }
            }

            if (!inserted) {
                out.push_back(merged);
            }

            intervals = std::move(out);
        }

        void SubtractInterval(const interval_type& interval) {
            if (interval.lower() >= interval.upper()) {
                return;
            }

            container_type out;
            for (const auto& current : intervals) {
                if (current.upper() <= interval.lower() || interval.upper() <= current.lower()) {
                    out.push_back(current);
                    continue;
                }
                if (current.lower() < interval.lower()) {
                    out.emplace_back(current.lower(), interval.lower());
                }
                if (interval.upper() < current.upper()) {
                    out.emplace_back(interval.upper(), current.upper());
                }
            }
            intervals = std::move(out);
        }

        container_type intervals;

        friend bool Contains(const IntervalSet& set, const interval_type& interval);
        friend bool Intersects(const IntervalSet& set, const interval_type& interval);
        friend class aurora::serialization::access;
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & intervals;
        }
    };

    static bool Contains(const IntervalSet& set, const Interval& interval) {
        if (interval.lower() >= interval.upper()) {
            return true;
        }
        for (const auto& block : set.intervals) {
            if (block.lower() <= interval.lower() && interval.upper() <= block.upper()) {
                return true;
            }
        }
        return false;
    }

    static bool Intersects(const IntervalSet& set, const Interval& interval) {
        if (interval.lower() >= interval.upper()) {
            return false;
        }
        for (const auto& block : set.intervals) {
            if (!(block.upper() <= interval.lower() || interval.upper() <= block.lower())) {
                return true;
            }
        }
        return false;
    }

    IntervalSet free_blocks;

    // When locked, Free calls will be ignored, while Allocate calls will hit an assert. A memory
    // region locks itself after deserialization.
    bool is_locked{};

    /**
     * Reset the allocator state
     * @param base The base offset the beginning of FCRAM.
     * @param size The region size this allocator manages
     */
    void Reset(u32 base, u32 size);

    /**
     * Allocates memory from the heap.
     * @param size The size of memory to allocate.
     * @returns The set of blocks that make up the allocation request. Empty set if there is no
     *     enough space.
     */
    IntervalSet HeapAllocate(u32 size);

    /**
     * Allocates memory from the linear heap with specific address and size.
     * @param offset the address offset to the beginning of FCRAM.
     * @param size size of the memory to allocate.
     * @returns true if the allocation is successful. false if the requested region is not free.
     */
    bool LinearAllocate(u32 offset, u32 size);

    /**
     * Allocates memory from the linear heap with only size specified.
     * @param size size of the memory to allocate.
     * @returns the address offset to the beginning of FCRAM; null if there is no enough space
     */
    std::optional<u32> LinearAllocate(u32 size);

    /**
     * Allocates memory from the linear heap with only size specified.
     * @param size size of the memory to allocate.
     * @returns the address offset to the found block, searching from the end of FCRAM; null if
     * there is no enough space
     */
    std::optional<u32> RLinearAllocate(u32 size);

    /**
     * Frees one segment of memory. The memory must have been allocated as heap or linear heap.
     * @param offset the region address offset to the beginning of FCRAM.
     * @param size the size of the region to free.
     */
    void Free(u32 offset, u32 size);

    /**
     * Unlock the MemoryRegion. Used after loading is completed.
     */
    void Unlock();

private:
    friend class aurora::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

HLE_CLASS_EXPORT_KEY(Kernel::MemoryRegionInfo)
