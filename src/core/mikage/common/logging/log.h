#pragma once

// Cytrus CPU uses the newer Citra-style LOG_* API with fmt-style format strings.
// Mikage's current tree still uses printf-style logging macros in common/log.h.
//
// For the migration phase we provide no-op compatibility macros so Cytrus CPU
// sources can be compiled incrementally before full logging backend unification.

#define LOG_TRACE(category, ...) do {} while (0)
#define LOG_DEBUG(category, ...) do {} while (0)
#define LOG_INFO(category, ...) do {} while (0)
#define LOG_WARNING(category, ...) do {} while (0)
#define LOG_ERROR(category, ...) do {} while (0)
#define LOG_CRITICAL(category, ...) do {} while (0)

#ifndef OS_LOG_TYPE_DEFAULT
#define OS_LOG_TYPE_DEFAULT 0
#endif
#ifndef OS_LOG_TYPE_WARNING
#define OS_LOG_TYPE_WARNING OS_LOG_TYPE_DEFAULT
#endif
