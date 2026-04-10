option(USE_SYSTEM_TOML11 "Use system-provided toml11 library." OFF)

include(FetchContent)

if(USE_SYSTEM_TOML11)
  find_package(toml11 4.0 REQUIRED)
else()
  FetchContent_Declare(toml11
    GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
    GIT_TAG        be08ba2be2a964edcdb3d3e3ea8d100abc26f286 # v4.4.0
  )
  set(toml11_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(toml11)
endif()

set(SOURCES
  src/loader/bios.cpp
  src/loader/rom.cpp
  src/loader/save_state.cpp
  src/writer/save_state.cpp
  src/config.cpp
  src/emulator_thread.cpp
  src/frame_limiter.cpp
  src/game_db.cpp
)

set(HEADERS_PUBLIC
  include/platform/loader/bios.hpp
  include/platform/loader/rom.hpp
  include/platform/loader/save_state.hpp
  include/platform/writer/save_state.hpp
  include/platform/config.hpp
  include/platform/emulator_thread.hpp
  include/platform/frame_limiter.hpp
  include/platform/game_db.hpp
)

add_library(platform-core STATIC)
target_sources(platform-core PRIVATE ${SOURCES} ${HEADERS_PUBLIC})
target_include_directories(platform-core PRIVATE src PUBLIC include)

target_link_libraries(platform-core
  PUBLIC nba toml11::toml11
)
