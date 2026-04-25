cmake_minimum_required(VERSION 3.25)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  project(AuroraIOS LANGUAGES C CXX OBJC OBJCXX)
else()
  project(AuroraIOS LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME ON)

option(NBA_ENABLE_AURORA3DS_CORE_INTEGRATION
  "Build and wire the legacy aurora3ds src/core CPU+HLE implementation into the unified core set"
  OFF)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
  "src/core/*.cpp"
  "src/core/api/*.cpp"
  "src/core/mikage/*.cpp"
)

set(BEARSSL_SOURCES
  src/core/third_party/bearssl/src/settings.c
  src/core/third_party/bearssl/src/codec/ccopy.c
  src/core/third_party/bearssl/src/codec/dec32be.c
  src/core/third_party/bearssl/src/codec/enc32be.c
  src/core/third_party/bearssl/src/hash/sha2small.c
  src/core/third_party/bearssl/src/rsa/rsa_i31_pub.c
  src/core/third_party/bearssl/src/int/i31_add.c
  src/core/third_party/bearssl/src/int/i31_bitlen.c
  src/core/third_party/bearssl/src/int/i31_decmod.c
  src/core/third_party/bearssl/src/int/i31_decode.c
  src/core/third_party/bearssl/src/int/i31_decred.c
  src/core/third_party/bearssl/src/int/i31_encode.c
  src/core/third_party/bearssl/src/int/i31_fmont.c
  src/core/third_party/bearssl/src/int/i31_iszero.c
  src/core/third_party/bearssl/src/int/i31_moddiv.c
  src/core/third_party/bearssl/src/int/i31_modpow.c
  src/core/third_party/bearssl/src/int/i31_modpow2.c
  src/core/third_party/bearssl/src/int/i31_montmul.c
  src/core/third_party/bearssl/src/int/i31_mulacc.c
  src/core/third_party/bearssl/src/int/i31_muladd.c
  src/core/third_party/bearssl/src/int/i31_ninv31.c
  src/core/third_party/bearssl/src/int/i31_reduce.c
  src/core/third_party/bearssl/src/int/i31_rshift.c
  src/core/third_party/bearssl/src/int/i31_sub.c
  src/core/third_party/bearssl/src/int/i31_tmont.c
  src/core/third_party/bearssl/src/int/i32_div32.c
)

# Exclude reference interpreter files which have external dependencies not present in build env
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/core/mikage/src/core/CPU/reference/.*")
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/core/mikage/src/core/CPU/dyncom/arm_dyncom\.cpp")
if(NOT NBA_ENABLE_AURORA3DS_CORE_INTEGRATION)
  # The legacy aurora3ds src/core tree is currently incomplete in this repository snapshot and
  # pulls transitive includes that are not present (e.g. common/settings.h, applet headers).
  # Keep the adapter layer, but exclude that legacy implementation tree by default.
  list(FILTER CORE_SOURCES EXCLUDE REGEX "src/core/aurora3ds/src/core/.*")
endif()

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
    ${BEARSSL_SOURCES}
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
    ${CMAKE_SOURCE_DIR}/src/core/third_party/bearssl/src
    ${CMAKE_SOURCE_DIR}/src/core/mikage/include
    ${CMAKE_SOURCE_DIR}/src/core/third_party/bearssl/inc
  )

  target_compile_options(AuroraIOS PRIVATE
    -fobjc-arc
  )
  target_compile_definitions(AuroraIOS PRIVATE
    NBA_BUILD_AURORA3DS_ADAPTER=$<BOOL:${NBA_ENABLE_AURORA3DS_CORE_INTEGRATION}>
  )

  find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
  find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
  find_library(COMPRESSION_FRAMEWORK Compression)
  find_library(SECURITY_FRAMEWORK Security REQUIRED)

  target_link_libraries(AuroraIOS PRIVATE
    ${AVFOUNDATION_FRAMEWORK}
    ${AUDIOTOOLBOX_FRAMEWORK}
    ${SECURITY_FRAMEWORK}
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
    ${BEARSSL_SOURCES}
  )

  target_include_directories(AuroraSDL PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/api
    ${CMAKE_SOURCE_DIR}/src/core/aurora3ds/include
    ${CMAKE_SOURCE_DIR}/src/core/aurora3ds/src
    ${CMAKE_SOURCE_DIR}/src/core/third_party/bearssl/src
    ${CMAKE_SOURCE_DIR}/src/core/mikage/include
    ${CMAKE_SOURCE_DIR}/src/core/third_party/bearssl/inc
  )

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)

  target_compile_options(AuroraSDL PRIVATE ${SDL2_CFLAGS_OTHER})
  target_compile_definitions(AuroraSDL PRIVATE
    NBA_BUILD_AURORA3DS_ADAPTER=$<BOOL:${NBA_ENABLE_AURORA3DS_CORE_INTEGRATION}>
  )
  target_include_directories(AuroraSDL PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_directories(AuroraSDL PRIVATE ${SDL2_LIBRARY_DIRS})
  target_link_libraries(AuroraSDL PRIVATE ${SDL2_LIBRARIES} cryptopp pthread dl)
else()
  message(FATAL_ERROR "Unsupported CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}. Supported: iOS, Linux")
endif()
