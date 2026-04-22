cmake_minimum_required(VERSION 3.25)

project(AuroraIOS LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME ON)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  message(FATAL_ERROR "This CMake project is for iOS builds only.")
endif()

file(GLOB_RECURSE AURORAIOS_SOURCES CONFIGURE_DEPENDS
  "ios/AuroraIOS/*.m"
  "ios/AuroraIOS/*.mm"
  "ios/AuroraIOS/*.c"
  "ios/AuroraIOS/*.cpp"
)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
  "src/core/*.cpp"
  "src/core/api/*.cpp"
  "src/core/mikage/*.cpp"
  "src/core/mikage/*.c"
)

# Exclude reference interpreter files which have external dependencies not present in build env
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/core/mikage/src/core/CPU/reference/.*")
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/core/mikage/src/core/CPU/dyncom/arm_dyncom\.cpp")


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
  ${CMAKE_SOURCE_DIR}/src/core/mikage/include
)

target_compile_options(AuroraIOS PRIVATE
  -fobjc-arc
)

find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)

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
  "-framework LocalAuthentication"
  "-framework UniformTypeIdentifiers"
  "-framework CoreServices"
  "-framework CoreLocation"
  "-framework MapKit"
  "-framework MobileCoreServices"
  "-framework GameController"
  "-lz"
  "-liconv"
)
