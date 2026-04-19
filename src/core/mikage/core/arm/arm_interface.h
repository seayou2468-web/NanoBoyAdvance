// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include <memory>
#include <cstddef>
#include "../../common/common.h"
#include "../../common/common_types.h"
#include "skyeye_common/arm_regformat.h"

#include "../hle/svc.h"

namespace Memory {
class PageTable;
}

/// Generic ARM11 CPU interface
class ARM_Interface : NonCopyable {
public:
    ARM_Interface() {
        num_instructions = 0;
    }

    virtual ~ARM_Interface() {
    }

    /**
     * Runs the CPU for the given number of instructions
     * @param num_instructions Number of instructions to run
     */
    void Run(int num_instructions) {
        ExecuteInstructions(num_instructions);
        this->num_instructions += num_instructions;
    }

    /// Step CPU by one instruction
    void Step() {
        Run(1);
    }

    /**
     * Set the Program Counter to an address
     * @param addr Address to set PC to
     */
    virtual void SetPC(u32 addr) = 0;

    /*
     * Get the current Program Counter
     * @return Returns current PC
     */
    virtual u32 GetPC() const = 0;

    /**
     * Get an ARM register
     * @param index Register index (0-15)
     * @return Returns the value in the register
     */
    virtual u32 GetReg(int index) const = 0;

    /**
     * Set an ARM register
     * @param index Register index (0-15)
     * @param value Value to set register to
     */
    virtual void SetReg(int index, u32 value) = 0;

    /**
     * Get the current CPSR register
     * @return Returns the value of the CPSR register
     */
    virtual u32 GetCPSR() const = 0;  

    /**
     * Set the current CPSR register
     * @param cpsr Value to set CPSR to
     */
    virtual void SetCPSR(u32 cpsr) = 0;

    /**
     * Returns the number of clock ticks since the last rese
     * @return Returns number of clock ticks
     */
    virtual u64 GetTicks() const = 0;

    /**
     * Saves the current CPU context
     * @param ctx Thread context to save
     */
    virtual void SaveContext(ThreadContext& ctx) = 0;

    /**
     * Loads a CPU context
     * @param ctx Thread context to load
     */
    virtual void LoadContext(const ThreadContext& ctx) = 0;

    /**
     * Get a CP15 coprocessor register
     * @param reg CP15 register identifier
     * @return The value of the CP15 register
     */
    virtual u32 GetCP15Register(CP15Register reg) const {
        return 0; // Default stub implementation
    }

    /**
     * Set a CP15 coprocessor register
     * @param reg CP15 register identifier
     * @param value Value to set the CP15 register to
     */
    virtual void SetCP15Register(CP15Register reg, u32 value) {
        // Default stub implementation
    }

    /**
     * Get a VFP register
     * @param index Register index (0-31 for single precision, 0-15 for double precision)
     * @return The value of the VFP register
     */
    virtual u32 GetVFPReg(int index) const {
        return 0; // Default stub implementation
    }

    /**
     * Set a VFP register
     * @param index Register index
     * @param value Value to set the VFP register to
     */
    virtual void SetVFPReg(int index, u32 value) {
        // Default stub implementation
    }

    /**
     * Get VFP status and control register
     * @param reg VFP system register identifier
     * @return The value of the VFP system register
     */
    virtual u32 GetVFPSystemReg(VFPSystemRegister reg) const {
        return 0; // Default stub implementation
    }

    /**
     * Set VFP status and control register
     * @param reg VFP system register identifier
     * @param value Value to set the VFP register to
     */
    virtual void SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
        // Default stub implementation
    }

    /**
     * Get the privilege mode
     * @return The current privilege mode
     */
    virtual u32 GetPrivilegeMode() const {
        return 0; // Default stub implementation
    }

    /**
     * Set the privilege mode
     * @param mode The privilege mode to set
     */
    virtual void SetPrivilegeMode(u32 mode) {
        // Default stub implementation
    }

    /**
     * Get Thumb flag state
     * @return Non-zero if in Thumb mode, zero if in ARM mode
     */
    virtual u32 GetThumbFlag() const {
        return 0; // Default stub implementation
    }

    /**
     * Set Thumb flag state
     * @param value Non-zero to enable Thumb mode, zero for ARM mode
     */
    virtual void SetThumbFlag(u32 value) {
        // Default stub implementation
    }

    /**
     * Clear the instruction cache
     */
    virtual void ClearInstructionCache() {
        // Default stub implementation
    }

    /**
     * Invalidate a range in the instruction cache
     * @param addr Address to invalidate
     * @param size Size of the range to invalidate
     */
    virtual void InvalidateCacheRange(u32 addr, std::size_t size) {
        // Default stub implementation
    }

    /**
     * Set the page table for memory access
     * @param page_table Shared pointer to the page table
     */
    virtual void SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
        // Default stub implementation
    }

    /**
     * Get the current page table
     * @return Shared pointer to the current page table
     */
    virtual std::shared_ptr<Memory::PageTable> GetPageTable() const {
        return nullptr;
    }

    /// Getter for num_instructions
    u64 GetNumInstructions() {
        return num_instructions;
    }

protected:
    
    /**
     * Executes the given number of instructions
     * @param num_instructions Number of instructions to execute
     */
    virtual void ExecuteInstructions(u64 num_instructions) = 0;

private:

    u64 num_instructions; ///< Number of instructions executed

};
