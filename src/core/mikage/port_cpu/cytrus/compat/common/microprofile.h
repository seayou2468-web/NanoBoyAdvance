// iOS Compatibility Layer for Performance Profiling
// Microprofile macros are replaced with no-ops for production builds
#pragma once

// All microprofile macros are replaced with empty implementations
// These are only used for performance profiling during development

#define MICROPROFILE_DECLARE(name)
#define MICROPROFILE_DEFINE(category, name, color)
#define MICROPROFILE_SCOPE(token)
#define MICROPROFILE_SCOPEI(category, name, color)

// Optional: Simple cycle counter for profiling if needed (no-op by default)
inline uint64_t MicroprofileGetCycles() {
    return 0;
}
