// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "../../../common/common.h"

#include "../arm_interface.h"

class ARMul_State;

namespace Core {
class System;
}

namespace Memory {
class MemorySystem;
}

struct ThreadContext;

namespace Core {
class ARM_Interpreter : virtual public ARM_Interface {
public:

    ARM_Interpreter();
    ~ARM_Interpreter();

    /**
     * Initialize the interpreter with System and Memory references
     * This must be called before executing instructions
     */
    void InitializeWithSystem(Core::System& system, Memory::MemorySystem& memory);

    /**
     * Set the Program Counter to an address
     * @param addr Address to set PC to
     */
    void SetPC(u32 pc) override;

    /**
     * Get the current Program Counter
     * @return Returns current PC
     */
    u32 GetPC() const override;

    /**
     * Get an ARM register
     * @param index Register index (0-15)
     * @return Returns the value in the register
     */
    u32 GetReg(int index) const override;

    /**
     * Set an ARM register
     * @param index Register index (0-15)
     * @param value Value to set register to
     */
    void SetReg(int index, u32 value) override;

    /**
     * Get the current CPSR register
     * @return Returns the value of the CPSR register
     */
    u32 GetCPSR() const override;

    /**
     * Set the current CPSR register
     * @param cpsr Value to set CPSR to
     */
    void SetCPSR(u32 cpsr) override;

    /**
     * Returns the number of clock ticks since the last reset
     * @return Returns number of clock ticks
     */
    u64 GetTicks() const override;

    /**
     * Saves the current CPU context
     * @param ctx Thread context to save
     */
    void SaveContext(ThreadContext& ctx) override;

    /**
     * Loads a CPU context
     * @param ctx Thread context to load
     */
    void LoadContext(const ThreadContext& ctx) override;

    /**
     * Get a CP15 coprocessor register
     */
    u32 GetCP15Register(u32 crn, u32 crm, u32 opcode_1, u32 opcode_2) const override;

    /**
     * Set a CP15 coprocessor register
     */
    void SetCP15Register(u32 crn, u32 crm, u32 opcode_1, u32 opcode_2, u32 value) override;

    /**
     * Get a VFP register
     */
    u32 GetVFPRegister(int index) const override;

    /**
     * Set a VFP register
     */
    void SetVFPRegister(int index, u32 value) override;

    /**
     * Get VFP status and control register
     */
    u32 GetVFPSystemReg() const override;

    /**
     * Set VFP status and control register
     */
    void SetVFPSystemReg(u32 value) override;

    /**
     * Get the privilege mode
     */
    u32 GetPrivilegeMode() const override;

    /**
     * Set the privilege mode
     */
    void SetPrivilegeMode(u32 mode) override;

    /**
     * Get Thumb flag state
     */
    u32 GetThumbFlag() const override;

    /**
     * Set Thumb flag state
     */
    void SetThumbFlag(u32 value) override;

protected:

    /**
     * Executes the given number of instructions
     * @param num_instructions Number of instructions to execute
     */
    void ExecuteInstructions(int num_instructions) override;

private:

    ARMul_State* state;

};
}
