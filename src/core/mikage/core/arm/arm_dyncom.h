// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// iOS compatibility port

#pragma once

#include <memory>
#include <cstdint>
#include "../../../common/compat/core/core_timing.h"
#include "arm_interface.h"

// Forward declarations from dynamically compiled ARM interpreter
class ARMul_State;

namespace Core {

/// ARM DynCom (Dynamically Compiled) interpreter - JIT-free implementation
class ARM_DynCom : public ARM_Interface {
public:
    explicit ARM_DynCom(Core::System& system, Memory::MemorySystem& memory,
                        PrivilegeMode initial_mode, u32 id,
                        std::shared_ptr<Core::Timing::Timer> timer);
    ~ARM_DynCom();

    void Run() override;
    void Step() override;

    void SetPC(u32 pc) override;
    u32 GetPC() const override;

    u32 GetReg(int index) const override;
    void SetReg(int index, u32 value) override;

    u32 GetVFPReg(int index) const override;
    void SetVFPReg(int index, u32 value) override;

    u32 GetVFPSystemReg(VFPSystemRegister reg) const override;
    void SetVFPSystemReg(VFPSystemRegister reg, u32 value) override;

    u32 GetCPSR() const override;
    void SetCPSR(u32 cpsr) override;

    u32 GetCP15Register(CP15Register reg) const override;
    void SetCP15Register(CP15Register reg, u32 value) override;

    void ExecuteInstructions(u64 num_instructions) override;

    void ClearInstructionCache() override;
    void InvalidateCacheRange(u32 addr, std::size_t size) override;
    void SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) override;
    std::shared_ptr<Memory::PageTable> GetPageTable() const override;

private:
    std::unique_ptr<ARMul_State> state;
    Core::System& system;
    std::shared_ptr<Core::Timing::Timer> timer;

    // Instruction cache
    static constexpr std::size_t TRANS_CACHE_BUF_SIZE = 2 * 1024 * 1024; // 2MB
    u8 trans_cache_buf[TRANS_CACHE_BUF_SIZE];
    std::size_t trans_cache_buf_top = 0;
};

} // namespace Core
