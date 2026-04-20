#pragma once

template <typename T>
class RightOpenInterval {
	T lowerBound {};
	T upperBound {};

  public:
	RightOpenInterval() = default;
	RightOpenInterval(T lower, T upper) : lowerBound(lower), upperBound(upper) {}

	T lower() const { return lowerBound; }
	T upper() const { return upperBound; }
};

