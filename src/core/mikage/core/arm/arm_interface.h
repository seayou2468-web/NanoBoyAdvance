#pragma once
#include "../../common/common.h"
#include "../../common/common_types.h"
#include "../hle/svc.h"

namespace Core {

class ARM_Interface : NonCopyable {
public:
    ARM_Interface() : num_instructions(0) {}
    virtual ~ARM_Interface() {}

    virtual void Run(int num_instructions) {
        ExecuteInstructions(num_instructions);
        this->num_instructions += num_instructions;
    }

    virtual void Step() {
        ExecuteInstructions(1);
        this->num_instructions += 1;
    }

    virtual void SetPC(u32 addr) = 0;
    virtual u32 GetPC() const = 0;
    virtual u32 GetReg(int index) const = 0;
    virtual void SetReg(int index, u32 value) = 0;
    virtual u32 GetCPSR() const = 0;
    virtual void SetCPSR(u32 cpsr) = 0;
    virtual u64 GetTicks() const = 0;

    virtual u32 GetCP15Register(u32 crn, u32 crm, u32 opcode_1, u32 opcode_2) const = 0;
    virtual void SetCP15Register(u32 crn, u32 crm, u32 opcode_1, u32 opcode_2, u32 value) = 0;
    virtual u32 GetVFPRegister(int index) const = 0;
    virtual void SetVFPRegister(int index, u32 value) = 0;
    virtual u32 GetVFPSystemReg() const = 0;
    virtual void SetVFPSystemReg(u32 value) = 0;
    virtual u32 GetPrivilegeMode() const = 0;
    virtual void SetPrivilegeMode(u32 mode) = 0;
    virtual u32 GetThumbFlag() const = 0;
    virtual void SetThumbFlag(u32 value) = 0;

    virtual void SaveContext(ThreadContext& ctx) = 0;
    virtual void LoadContext(const ThreadContext& ctx) = 0;

    u64 GetNumInstructions() { return num_instructions; }

protected:
    virtual void ExecuteInstructions(int num_instructions) = 0;

private:
    u64 num_instructions;
};

} // namespace Core
