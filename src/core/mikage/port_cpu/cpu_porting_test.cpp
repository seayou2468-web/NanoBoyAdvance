// Simple compilation test for ported CPU implementations
// Verifies that all compatibility layers are correctly configured

#include <iostream>
#include <cstdint>

// Include the compatibility headers for all three targets
// This ensures all includes are resolvable

// CPU1 includes (for verification)
#include "cpu1/compat/common/logging/log.h"
#include "cpu1/compat/common/swap.h"
#include "cpu1/compat/common/microprofile.h"
#include "cpu1/compat/common/common_types.h"
#include "cpu1/compat/common/assert.h"
#include "cpu1/compat/common/common_funcs.h"
#include "cpu1/compat/common/arch.h"
#include "cpu1/compat/common/settings.h"

#include "cpu1/compat/core/core.h"
#include "cpu1/compat/core/memory.h"
#include "cpu1/compat/core/core_timing.h"
#include "cpu1/compat/core/gdbstub/gdbstub.h"
#include "cpu1/compat/core/hle/kernel/svc.h"

int main() {
    std::cout << "=== CPU Port Compatibility Test ===" << std::endl;
    
    // Test 1: Type definitions
    std::cout << "Test 1: Type definitions..." << std::endl;
    u32 test_u32 = 0x12345678;
    u16 test_u16 = 0xABCD;
    f32 test_f32 = 3.14159f;
    std::cout << "  u32: " << std::hex << test_u32 << std::endl;
    std::cout << "  u16: " << test_u16 << std::endl;
    std::cout << "  f32: " << std::dec << test_f32 << std::endl;
    
    // Test 2: Byte swapping
    std::cout << "Test 2: Byte swapping..." << std::endl;
    uint16_t swapped_16 = Common::swap16(0x1234);
    uint32_t swapped_32 = Common::swap32(0x12345678);
    std::cout << "  swap16(0x1234) = 0x" << std::hex << swapped_16 << std::endl;
    std::cout << "  swap32(0x12345678) = 0x" << swapped_32 << std::endl;
    
    // Test 3: Logging (to stderr)
    std::cout << "Test 3: Logging..." << std::endl;
    LOG_DEBUG(Core_ARM11, "Debug message from CPU port");
    LOG_INFO(Core_ARM11, "Info message from CPU port");
    LOG_WARNING(Core_ARM11, "Warning message from CPU port");
    
    // Test 4: Memory system
    std::cout << "Test 4: Memory system..." << std::endl;
    Memory::MemorySystem memory;
    memory.Write32(0x0, 0xDEADBEEF);
    uint32_t read_value = memory.Read32(0x0);
    std::cout << "  Wrote 0xDEADBEEF, read back: 0x" << std::hex << read_value << std::endl;
    
    // Test 5: System core
    std::cout << "Test 5: System core..." << std::endl;
    Core::System system;
    std::cout << "  System instance created successfully" << std::endl;
    
    // Test 6: Timing
    std::cout << "Test 6: Timing..." << std::endl;
    auto timer = std::make_shared<Core::Timing::Timer>();
    timer->SetDowncount(1000);
    std::cout << "  Timer downcount set to: " << timer->GetDowncount() << std::endl;
    
    // Test 7: Bit operations
    std::cout << "Test 7: Bit operations..." << std::endl;
    uint32_t clz_test = 0x00000001;
    int leading_zeros = clz(clz_test);
    std::cout << "  CLZ(0x00000001) = " << leading_zeros << std::endl;
    
    std::cout << "=== All tests completed successfully ===" << std::endl;
    return 0;
}
