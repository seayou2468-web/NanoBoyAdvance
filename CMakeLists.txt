cmake_minimum_required(VERSION 3.25)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  project(AuroraIOS LANGUAGES C CXX OBJC OBJCXX)
else()
  project(AuroraIOS LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME ON)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
  "src/core/*.cpp"
  "src/core/api/*.cpp"
  "src/core/mikage/*.cpp"
  "src/core/mikage/*.c"
)

# Exclude reference interpreter files which have external dependencies not present in build env
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/core/mikage/src/core/CPU/reference/.*")
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/core/mikage/src/core/CPU/dyncom/arm_dyncom\.cpp")
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/core/aurora3ds/src/core/CPU/dyncom/arm_dyncom\.cpp$")

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  file(GLOB_RECURSE AURORAIOS_SOURCES CONFIGURE_DEPENDS
    "ios/AuroraIOS/*.m"
    "ios/AuroraIOS/*.mm"
    "ios/AuroraIOS/*.c"
    "ios/AuroraIOS/*.cpp"
  )

  add_executable(AuroraIOS MACOSX_BUNDLE
    ${AURORAIOS_SOURCES}
    ${CORE_SOURCES}
    Resources/fonts/SharedFontReplacement.bin
  )

  set_source_files_properties(Resources/fonts/SharedFontReplacement.bin PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources/fonts"
  )

  set_target_properties(AuroraIOS PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER com.frappev.rn
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.frappev.rn
    XCODE_ATTRIBUTE_INFOPLIST_FILE ${CMAKE_SOURCE_DIR}/Config/Info.plist
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
  )

  target_include_directories(AuroraIOS PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/api
    ${CMAKE_SOURCE_DIR}/src/core/aurora3ds/include
    ${CMAKE_SOURCE_DIR}/src/core/aurora3ds/src
    ${CMAKE_SOURCE_DIR}/src/core/aurora3ds/src/core/services
    ${CMAKE_SOURCE_DIR}/src/core/mikage/include
  )

  target_compile_options(AuroraIOS PRIVATE
    -fobjc-arc
  )

  find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
  find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
  find_library(COMPRESSION_FRAMEWORK Compression)

  target_link_libraries(AuroraIOS PRIVATE
    ${AVFOUNDATION_FRAMEWORK}
    ${AUDIOTOOLBOX_FRAMEWORK}
    "-framework Foundation"
    "-framework UIKit"
    "-framework CoreGraphics"
    "-framework QuartzCore"
    "-framework Network"
    "-framework AVKit"
    "-framework CoreImage"
    "-framework Metal"
    "-framework MetalKit"
    "-framework PencilKit"
    "-framework UniformTypeIdentifiers"
    "-framework CoreServices"
    "-framework MobileCoreServices"
    "-framework GameController"
    "-framework Security"
    "-lz"
    "-liconv"
  )

  if(COMPRESSION_FRAMEWORK)
    target_link_libraries(AuroraIOS PRIVATE ${COMPRESSION_FRAMEWORK})
  else()
    target_link_libraries(AuroraIOS PRIVATE "-lcompression")
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(FILTER CORE_SOURCES EXCLUDE REGEX "src/core/aurora3ds/src/core/audio/ios_audio_device\.cpp$")

  add_executable(AuroraSDL
    src/linux_sdl_main.cpp
    ${CORE_SOURCES}
  )

  target_include_directories(AuroraSDL PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/api
    ${CMAKE_SOURCE_DIR}/src/core/aurora3ds/include
    ${CMAKE_SOURCE_DIR}/src/core/aurora3ds/src
    ${CMAKE_SOURCE_DIR}/src/core/aurora3ds/src/core/services
    ${CMAKE_SOURCE_DIR}/src/core/mikage/include
  )

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)

  target_compile_options(AuroraSDL PRIVATE ${SDL2_CFLAGS_OTHER})
  target_include_directories(AuroraSDL PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_directories(AuroraSDL PRIVATE ${SDL2_LIBRARY_DIRS})
  target_link_libraries(AuroraSDL PRIVATE ${SDL2_LIBRARIES} cryptopp pthread dl)
else()
  message(FATAL_ERROR "Unsupported CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}. Supported: iOS, Linux")
endif()
