#include "core/arm/interpreter/exclusive_monitor.h"
#include "core/memory.h"
#include <mutex>
#include <unordered_map>

namespace Core {

ExclusiveMonitor::~ExclusiveMonitor() = default;

class InterpreterExclusiveMonitor final : public ExclusiveMonitor {
public:
    explicit InterpreterExclusiveMonitor(Memory::MemorySystem& memory, std::size_t num_cores)
        : memory(memory) {}

    void ClearExclusive(std::size_t core_index) override {
        std::lock_guard lock(mutex);
        exclusive_addresses.erase(core_index);
    }

    u8 ExclusiveRead8(std::size_t core_index, VAddr addr) override { return memory.Read8(addr); }
    u16 ExclusiveRead16(std::size_t core_index, VAddr addr) override { return memory.Read16(addr); }
    u32 ExclusiveRead32(std::size_t core_index, VAddr addr) override { return memory.Read32(addr); }
    u64 ExclusiveRead64(std::size_t core_index, VAddr addr) override { return memory.Read64(addr); }

    bool ExclusiveWrite8(std::size_t core_index, VAddr vaddr, u8 value) override {
        memory.Write8(vaddr, value);
        return true;
    }
    bool ExclusiveWrite16(std::size_t core_index, VAddr vaddr, u16 value) override {
        memory.Write16(vaddr, value);
        return true;
    }
    bool ExclusiveWrite32(std::size_t core_index, VAddr vaddr, u32 value) override {
        memory.Write32(vaddr, value);
        return true;
    }
    bool ExclusiveWrite64(std::size_t core_index, VAddr vaddr, u64 value) override {
        memory.Write64(vaddr, value);
        return true;
    }

private:
    Memory::MemorySystem& memory;
    std::mutex mutex;
    std::unordered_map<std::size_t, VAddr> exclusive_addresses;
};

std::unique_ptr<Core::ExclusiveMonitor> MakeExclusiveMonitor(Memory::MemorySystem& memory,
                                                             std::size_t num_cores) {
    return std::make_unique<InterpreterExclusiveMonitor>(memory, num_cores);
}

} // namespace Core
