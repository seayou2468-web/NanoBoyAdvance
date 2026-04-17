#include "armstate.h"
#include "core/system.h"
#include "core/memory.h"
#include "core/arm/arm_interface.h"

ARMul_State::ARMul_State(Core::System& system, Memory::MemorySystem& memory,
                         PrivilegeMode initial_mode)
    : system(system), memory(memory), Mode(static_cast<u32>(initial_mode)) {
    Reset();
}

void ARMul_State::Reset() {
    Reg.fill(0);
    Reg_usr.fill(0);
    Reg_svc.fill(0);
    Reg_abort.fill(0);
    Reg_undef.fill(0);
    Reg_irq.fill(0);
    Reg_firq.fill(0);
    Spsr.fill(0);
    CP15.fill(0);
    VFP.fill(0);
    ExtReg.fill(0);

    Cpsr = 0;
    Spsr_copy = 0;
    phys_pc = 0;
    Bank = 0;
    NFlag = ZFlag = CFlag = VFlag = IFFlags = 0;
    shifter_carry_out = 0;
    TFlag = 0;
    NumInstrs = 0;
    NumInstrsToExecute = 0;
}

void ARMul_State::ChangePrivilegeMode(u32 new_mode) {
    Mode = new_mode;
}

u8 ARMul_State::ReadMemory8(u32 address) const { return memory.Read8(address); }
u16 ARMul_State::ReadMemory16(u32 address) const { return memory.Read16(address); }
u32 ARMul_State::ReadMemory32(u32 address) const { return memory.Read32(address); }
u64 ARMul_State::ReadMemory64(u32 address) const { return memory.Read64(address); }

void ARMul_State::WriteMemory8(u32 address, u8 data) { memory.Write8(address, data); }
void ARMul_State::WriteMemory16(u32 address, u16 data) { memory.Write16(address, data); }
void ARMul_State::WriteMemory32(u32 address, u32 data) { memory.Write32(address, data); }
void ARMul_State::WriteMemory64(u32 address, u64 data) { memory.Write64(address, data); }

u32 ARMul_State::ReadCP15Register(u32 crn, u32 opcode_1, u32 crm, u32 opcode_2) const {
    u32 index = (crn << 4) | (opcode_1 << 2) | opcode_2;
    return index < CP15.size() ? CP15[index] : 0;
}

void ARMul_State::WriteCP15Register(u32 value, u32 crn, u32 opcode_1, u32 crm, u32 opcode_2) {
    u32 index = (crn << 4) | (opcode_1 << 2) | opcode_2;
    if (index < CP15.size()) CP15[index] = value;
}

void ARMul_State::ServeBreak() {
    // Stub
}
