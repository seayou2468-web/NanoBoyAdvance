#pragma once
#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include "./helpers.hpp"

template <typename Key, typename Val, usize size>
class FixedFlatMultimap {
	using Entry = std::pair<Key, Val>;
	std::array<Entry, size> entries {};
	usize used = 0;

  public:
	using iterator = typename std::array<Entry, size>::iterator;
	using const_iterator = typename std::array<Entry, size>::const_iterator;

	iterator begin() { return entries.begin(); }
	iterator end() { return entries.begin() + used; }
	const_iterator begin() const { return entries.begin(); }
	const_iterator end() const { return entries.begin() + used; }
	const_iterator cbegin() const { return entries.cbegin(); }
	const_iterator cend() const { return entries.cbegin() + used; }

	void clear() { used = 0; }

	std::pair<iterator, iterator> equal_range(const Key& key) {
		iterator first = std::lower_bound(begin(), end(), key, [](const Entry& e, const Key& k) { return e.first < k; });
		iterator last = std::upper_bound(first, end(), key, [](const Key& k, const Entry& e) { return k < e.first; });
		return {first, last};
	}

	std::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
		const_iterator first = std::lower_bound(cbegin(), cend(), key, [](const Entry& e, const Key& k) { return e.first < k; });
		const_iterator last = std::upper_bound(first, cend(), key, [](const Key& k, const Entry& e) { return k < e.first; });
		return {first, last};
	}

	void emplace(Key key, Val val) {
		if (used >= size) [[unlikely]] {
			Helpers::panic("FixedFlatMultimap overflow");
		}

		iterator insertion = std::upper_bound(begin(), end(), key, [](const Key& k, const Entry& e) { return k < e.first; });
		std::move_backward(insertion, end(), end() + 1);
		*insertion = Entry{key, val};
		used++;
	}

	iterator erase(iterator it) {
		if (it == end()) {
			return it;
		}

		std::move(it + 1, end(), it);
		used--;
		return it;
	}
};

struct Scheduler {
	enum class EventType {
		VBlank = 0,          // End of frame event
		ThreadWakeup = 1,    // A thread might wake up from eg sleeping or timing out
		RunDSP = 2,          // Make the emulated DSP run for one audio frame
		UpdateTimers = 3,    // Update kernel timer objects
		SignalY2R = 4,       // Signal that a Y2R conversion has finished
		UpdateIR = 5,        // Update an IR device (For now, just the CirclePad Pro/N3DS controls)
		Panic = 6,           // Dummy event that is always pending and should never be triggered (Timestamp = UINT64_MAX)
		TotalNumberOfEvents  // How many event types do we have in total?
	};
	static constexpr usize totalNumberOfEvents = static_cast<usize>(EventType::TotalNumberOfEvents);
	static constexpr u64 arm11Clock = 268111856;

	template <typename Key, typename Val, usize size>
	using EventMap = FixedFlatMultimap<Key, Val, size>;

	EventMap<u64, EventType, totalNumberOfEvents> events;
	u64 currentTimestamp = 0;
	u64 nextTimestamp = 0;

	// Set nextTimestamp to the timestamp of the next event
	void updateNextTimestamp() { nextTimestamp = events.cbegin()->first; }

	// Add an event to the scheduler. Assumes this event doesn't already exist in the scheduler.
	// (If it might, then use rescheduleEvent instead, which will remove and reschedule the event)
	void addEvent(EventType type, u64 timestamp) {
		events.emplace(timestamp, type);
		updateNextTimestamp();
	}

	void removeEvent(EventType type) {
		for (auto it = events.begin(); it != events.end(); it++) {
			// Find first event of type "type" and remove it.
			// Our scheduler shouldn't have duplicate events, so it's safe to exit when an event is found
			if (it->second == type) {
				events.erase(it);
				updateNextTimestamp();
				break;
			}
		}
	};

	// Reschedule an event of "type" to "newTimestamp".
	// If the event is already in the scheduler, replace its timestamp with the new timestamp
	// If possible, it will perform an in-place replacement. Otherwise, it will fallback to a remove+insert operation.
	// If the event is not in the scheduler, we'll add it
	void rescheduleEvent(EventType type, u64 newTimestamp) {
		// Find the event if it exists
		for (auto it = events.begin(); it != events.end(); ++it) {
			if (it->second == type) {
				bool inplace = true;

				// Peek at the previous and next events to see if we can safely update in-place
				if (it != events.begin()) {
					auto previousIterator = std::prev(it);
					if (newTimestamp < previousIterator->first) {
						inplace = false;
					}
				}

				auto nextIterator = std::next(it);
				if (nextIterator != events.end() && newTimestamp > nextIterator->first) {
					inplace = false;
				}

				if (inplace) {
					it->first = newTimestamp;
				} else {
					EventType ev = it->second;
					events.erase(it);
					events.emplace(newTimestamp, ev);
				}

				updateNextTimestamp();
				return;
			}
		}

		// The event did not exist: Add it to the scheduler
		addEvent(type, newTimestamp);
	}

	void reset() {
		currentTimestamp = 0;

		// Clear any pending events
		events.clear();
		addEvent(Scheduler::EventType::VBlank, arm11Clock / 60);

		// Add a dummy event to always keep the scheduler non-empty
		addEvent(EventType::Panic, std::numeric_limits<u64>::max());
	}

  private:
	static constexpr u64 MAX_VALUE_TO_MULTIPLY = std::numeric_limits<s64>::max() / arm11Clock;

  public:
	// Function for converting time units to cycles for various kernel functions
	// Thank you Citra
	static constexpr s64 nsToCycles(float ns) { return s64(arm11Clock * (0.000000001f) * ns); }
	static constexpr s64 nsToCycles(int ns) { return arm11Clock * s64(ns) / 1000000000; }

	static constexpr s64 nsToCycles(s64 ns) {
		if (ns / 1000000000 > static_cast<s64>(MAX_VALUE_TO_MULTIPLY)) {
			return std::numeric_limits<s64>::max();
		}

		if (ns > static_cast<s64>(MAX_VALUE_TO_MULTIPLY)) {
			return arm11Clock * (ns / 1000000000);
		}

		return (arm11Clock * ns) / 1000000000;
	}

	static constexpr s64 nsToCycles(u64 ns) {
		if (ns / 1000000000 > MAX_VALUE_TO_MULTIPLY) {
			return std::numeric_limits<s64>::max();
		}

		if (ns > MAX_VALUE_TO_MULTIPLY) {
			return arm11Clock * (s64(ns) / 1000000000);
		}

		return (arm11Clock * s64(ns)) / 1000000000;
	}
};
