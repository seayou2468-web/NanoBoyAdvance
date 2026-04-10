# Backward-compatible entry point.
# The platform layer was moved under the GBA core tree:
#   src/core/gba/platform
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../core/gba/platform ${CMAKE_CURRENT_BINARY_DIR}/gba-platform)
