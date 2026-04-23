#pragma once

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE
#include "./aac_decoder_ios.hpp"
#else
#include "./aac_decoder_stub.hpp"
#endif
