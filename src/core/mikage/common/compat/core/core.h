// iOS Compatibility Layer for System Core
// Provides minimal System interface stub for CPU interpreter
#pragma once

#include <memory>
#include <cstdint>

namespace Core {

// Forward declaration - minimal stub for CPU execution
// In iOS port, system is typically not used during CPU execution
class System {
public:
    System() = default;
    ~System() = default;
    
    // Placeholder methods that CPU code might call
    // These are typically no-ops in the ARM interpreter context
    
    void UpdateCores() {}
    void InvalidateCacheRange(uint32_t addr, std::size_t size) {}
    
    // Any other system methods needed can be added here
};

} // namespace Core
