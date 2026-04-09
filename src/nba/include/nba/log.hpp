/*
 * Copyright (C) 2025 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#pragma once

#include <array>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string_view>
#include <utility>

namespace nba {

enum Level {
  Trace =  1,
  Debug =  2,
  Info  =  4,
  Warn  =  8,
  Error = 16,
  Fatal = 32,

  All = Trace | Debug | Info | Warn | Error | Fatal
};

namespace detail {

#if defined(NDEBUG)
  static constexpr int kLogMask = Info | Warn | Error | Fatal;
#else
  static constexpr int kLogMask = All;
#endif

} // namespace nba::detail

template<Level level, typename... Args>
inline void Log(std::string_view format, Args&&... args) {
  if constexpr((detail::kLogMask & level) != 0) {
    const char* prefix = "[?]";
    const char* color_begin = "";
    const char* color_end = "";

    if constexpr(level == Trace) {
      prefix = "[T]";
      color_begin = "\033[36m";
      color_end = "\033[0m";
    }
    if constexpr(level == Debug) {
      prefix = "[D]";
      color_begin = "\033[34m";
      color_end = "\033[0m";
    }
    if constexpr(level == Info) {
      prefix = "[I]";
    }
    if constexpr(level == Warn) {
      prefix = "[W]";
      color_begin = "\033[33m";
      color_end = "\033[0m";
    }
    if constexpr(level == Error) {
      prefix = "[E]";
      color_begin = "\033[35m";
      color_end = "\033[0m";
    }
    if constexpr(level == Fatal) {
      prefix = "[F]";
      color_begin = "\033[31m";
      color_end = "\033[0m";
    }

    std::cout << color_begin << prefix << ' '
              << std::vformat(format, std::make_format_args(args...))
              << color_end << '\n';
  }
}

template<typename... Args>
inline void Assert(bool condition, Args... args) {
  if(!condition) {
    Log<Fatal>(std::forward<Args>(args)...);
    std::exit(-1);
  }
}

} // namespace nba
