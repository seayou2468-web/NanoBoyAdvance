#pragma once

#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

namespace fmt {
	namespace detail {
		template <typename T>
		std::string valueToString(const T& value) {
			std::ostringstream out;
			out << value;
			return out.str();
		}

		inline std::string valueToString(const char* value) { return value ? std::string(value) : std::string(); }
		inline std::string valueToString(char* value) { return value ? std::string(value) : std::string(); }
		inline std::string valueToString(const std::string& value) { return value; }

		template <typename T>
		std::string valueToString(const T& value, const std::string&) {
			// Minimal embedded formatter: currently ignores format specifiers.
			return valueToString(value);
		}

		template <typename Tuple, std::size_t... Indices>
		std::string getArgString(const Tuple& tuple, std::size_t index, const std::string& spec, std::index_sequence<Indices...>) {
			std::string result;
			(void)std::initializer_list<int>{
				(index == Indices ? (result = valueToString(std::get<Indices>(tuple), spec), 0) : 0)...};
			return result;
		}
	}  // namespace detail

	template <typename... Args>
	std::string format(const std::string& pattern, Args&&... args) {
		const auto tuple = std::forward_as_tuple(args...);
		std::string out;
		out.reserve(pattern.size() + sizeof...(Args) * 8);

		std::size_t argIndex = 0;
		for (std::size_t i = 0; i < pattern.size(); i++) {
			const char c = pattern[i];
			if (c == '{') {
				if (i + 1 < pattern.size() && pattern[i + 1] == '{') {
					out.push_back('{');
					i++;
					continue;
				}

				const std::size_t close = pattern.find('}', i + 1);
				if (close == std::string::npos) {
					throw std::runtime_error("fmt::format: unmatched '{'");
				}

				std::string spec;
				if (close > i + 1 && pattern[i + 1] == ':') {
					spec = pattern.substr(i + 2, close - (i + 2));
				}

				if (argIndex >= sizeof...(Args)) {
					throw std::runtime_error("fmt::format: not enough arguments");
				}

				out += detail::getArgString(tuple, argIndex, spec, std::index_sequence_for<Args...>{});
				argIndex++;
				i = close;
				continue;
			}

			if (c == '}') {
				if (i + 1 < pattern.size() && pattern[i + 1] == '}') {
					out.push_back('}');
					i++;
					continue;
				}
				throw std::runtime_error("fmt::format: unmatched '}'");
			}

			out.push_back(c);
		}

		return out;
	}

	template <typename... Args>
	void print(const std::string& pattern, Args&&... args) {
		const auto text = format(pattern, std::forward<Args>(args)...);
		std::fwrite(text.data(), 1, text.size(), stdout);
	}

	template <typename Range>
	std::string join(const Range& range, const std::string& separator) {
		std::ostringstream out;
		bool first = true;
		for (const auto& value : range) {
			if (!first) {
				out << separator;
			}
			first = false;
			out << value;
		}
		return out.str();
	}
}  // namespace fmt
