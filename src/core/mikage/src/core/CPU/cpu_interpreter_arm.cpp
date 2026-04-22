#include "../../../include/cpu_interpreter.hpp"
#include "../../../include/emulator.hpp"
#include "../../../include/kernel/kernel.hpp"
#include "../../../include/logger.hpp"
#include "./cpu_interpreter_internal.hpp"
#include <cfenv>
#include <cmath>
#include <bit>
#include <optional>

u32 CPU::executeArm(u32 inst) {
	const u32 old_pc = gprs[PC_INDEX];
	const u32 cond = inst >> 28;
	if (!conditionPassed(cond)) {
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	const InstructionDecoding* decoding = DecodeArmInstruction(inst);
	if (!decoding) {
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	const auto get_reg_operand = [&](u32 index) { return getRegOperand(index, old_pc); };
	const auto set_carry = [&](bool value) { setCarry(value); };
	const auto write_reg = [&](u32 index, u32 value) { writeReg(index, value); };
	const auto clear_exclusive = [&]() { clearExclusive(); };

    // Jump Table using Computed Goto
                static const void* InstLabel[] = {
        &&ADC_INST,        &&ADD_INST,        &&AND_INST,        &&B_INST,        &&BFC_INST,
        &&BFI_INST,        &&BIC_INST,        &&BKPT_INST,        &&BL_INST,        &&BLX_INST,
        &&BLX_IMM_INST,        &&BX_INST,        &&CLZ_INST,        &&CMN_INST,        &&CMP_INST,
        &&CPS_INST,        &&DMB_INST,        &&DSB_INST,        &&EOR_INST,        &&ISB_INST,
        &&LDC_INST,        &&LDM_INST,        &&LDR_INST,        &&LDRB_INST,        &&LDREX_INST,
        &&LDREXB_INST,        &&LDREXD_INST,        &&LDREXH_INST,        &&LDRH_INST,        &&LDRSB_INST,
        &&LDRSH_INST,        &&MLA_INST,        &&MOV_INST,        &&MOVT_INST,        &&MOVW_INST,
        &&MRS_INST,        &&MSR_INST,        &&MSR_IMM_INST,        &&MUL_INST,        &&MVN_INST,
        &&ORR_INST,        &&PKHBT_INST,        &&PKHTB_INST,        &&PLD_INST,        &&PLDW_INST,
        &&QADD_INST,        &&QDADD_INST,        &&QDSUB_INST,        &&QSUB_INST,        &&REV_INST,
        &&REV16_INST,        &&REVSH_INST,        &&RSB_INST,        &&RSC_INST,        &&SADD16_INST,
        &&SADD8_INST,        &&SADDSUBX_INST,        &&SBC_INST,        &&SBFX_INST,        &&SEL_INST,
        &&SMLAXY_INST,        &&SMLAL_INST,        &&SMULL_INST,        &&SSAT_INST,        &&SSAT16_INST,
        &&SSUB16_INST,        &&SSUB8_INST,        &&SSUBADDX_INST,        &&STC_INST,        &&STM_INST,
        &&STR_INST,        &&STRB_INST,        &&STREX_INST,        &&STREXB_INST,        &&STREXD_INST,
        &&STREXH_INST,        &&STRH_INST,        &&SUB_INST,        &&SWP_INST,        &&SWPB_INST,
        &&SXTAB_INST,        &&SXTAB16_INST,        &&SXTAH_INST,        &&SXTB_INST,        &&SXTB16_INST,
        &&SXTH_INST,        &&TEQ_INST,        &&TST_INST,        &&UBFX_INST,        &&UMAAL_INST,
        &&UMLAL_INST,        &&UMULL_INST,        &&USAT_INST,        &&USAT16_INST,        &&UXTAB_INST,
        &&UXTAB16_INST,        &&UXTAH_INST,        &&UXTB_INST,        &&UXTB16_INST,        &&UXTH_INST,
        &&VABS_F32_INST,        &&VABS_F64_INST,        &&VADD_F32_INST,        &&VADD_F64_INST,        &&VCMP_F32_INST,
        &&VCMP_F64_INST,        &&VCVT_F32_F64_INST,        &&VCVT_F64_F32_INST,        &&VCVT_S32_F32_INST,        &&VCVT_U32_F32_INST,
        &&VDIV_F32_INST,        &&VDIV_F64_INST,        &&VLDM_INST,        &&VLDR_INST,        &&VMLA_F32_INST,
        &&VMLA_F64_INST,        &&VMLS_F32_INST,        &&VMLS_F64_INST,        &&VMOV_I_INST,        &&VMOV_R_INST,
        &&VNEG_F32_INST,        &&VNEG_F64_INST,        &&VNMLA_F32_INST,        &&VNMLA_F64_INST,        &&VNMLS_F32_INST,
        &&VNMLS_F64_INST,        &&VSQRT_F32_INST,        &&VSQRT_F64_INST,        &&VSTM_INST,        &&VSTR_INST,
        &&WFE_INST,        &&WFI_INST,        &&YIELD_INST,        &&SEV_INST,        &&CLREX_INST,
        &&SVC_INST,        &&RFE_INST,        &&SRS_INST,        &&NOP_INST,        &&UNDEFINED_INST

    };

    goto *InstLabel[static_cast<u32>(decoding->id)];

ADC_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 rn_val = RN;
        if (((inst >> 16) & 0xFu) == 15)
            rn_val += 2 * 4;

        bool carry;
        bool overflow;
        RD = AddWithCarry(rn_val, SHIFTER_OPERAND, CFlag, &carry, &overflow);

        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = carry;
            VFlag = overflow;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
ADD_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 rn_val = (((((inst >> 16) == 15) ? (old_pc + 8) : gprs[((inst >> 16]) & 0xFu));

        bool carry;
        bool overflow;
        RD = AddWithCarry(rn_val, SHIFTER_OPERAND, 0, &carry, &overflow);

        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = carry;
            VFlag = overflow;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
AND_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 lop = RN;
        u32 rop = SHIFTER_OPERAND;

        if (((inst >> 16) & 0xFu) == 15)
            lop += 2 * 4;

        RD = lop & rop;

        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = shifter_carry;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
B_INST:
{
    if ((cond == 0xE) || conditionPassed(cond)) {
        bbl_inst* inst_cream = (bbl_inst*)component;
        if (((inst >> 20) & 1u)) {
            LINK_RTN_ADDR;
        }
        SET_PC;

        return 3;
    }
    gprs[15] += 4;

    return 3;
}
    goto END;
BFC_INST:
    goto END;
BFI_INST:
    goto END;
BIC_INST:
{
    bic_inst* inst_cream = (bic_inst*)component;
    if ((cond == 0xE) || conditionPassed(cond)) {
        u32 lop = RN;
        if (((inst >> 16) & 0xFu) == 15) {
            lop += 2 * 4;
        }
        u32 rop = SHIFTER_OPERAND;
        RD = lop & (~rop);
        if ((((inst >> 20) & 1u)) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = shifter_carry;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
BKPT_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        LOG_DEBUG(Core_ARM11, "Breakpoint instruction hit. Immediate: {:#010X}", (inst & 0xFFFu));
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
BL_INST:
{
    if ((cond == 0xE) || conditionPassed(cond)) {
        bbl_inst* inst_cream = (bbl_inst*)component;
        if (((inst >> 20) & 1u)) {
            LINK_RTN_ADDR;
        }
        SET_PC;

        return 3;
    }
    gprs[15] += 4;

    return 3;
}
    goto END;
BLX_INST:
{
    blx_inst* inst_cream = (blx_inst*)component;
    if ((cond == 0xE) || conditionPassed(cond)) {
        unsigned int inst = inst;
        if (BITS(inst, 20, 27) == 0x12 && BITS(inst, 4, 7) == 0x3) {
            const u32 jump_address = gprs[(inst & 0xFu)];
            gprs[14] = (gprs[15] + 4);
            if (TFlag)
                gprs[14] |= 0x1;
            gprs[15] = jump_address & 0xfffffffe;
            TFlag = jump_address & 0x1;
        } else {
            gprs[14] = (gprs[15] + 4);
            TFlag = 0x1;
            int signed_int = static_cast<s32>(signExtend(inst & 0xFFFFFF, 24));
            signed_int = (signed_int & 0x800000) ? (0x3F000000 | signed_int) : signed_int;
            signed_int = signed_int << 2;
            gprs[15] = gprs[15] + 8 + signed_int + (BIT(inst, 24) << 1);
        }

        return 3;
    }
    gprs[15] += 4;

    return 3;
}
    goto END;
BLX_IMM_INST:
{
    blx_inst* inst_cream = (blx_inst*)component;
    if ((cond == 0xE) || conditionPassed(cond)) {
        unsigned int inst = inst;
        if (BITS(inst, 20, 27) == 0x12 && BITS(inst, 4, 7) == 0x3) {
            const u32 jump_address = gprs[(inst & 0xFu)];
            gprs[14] = (gprs[15] + 4);
            if (TFlag)
                gprs[14] |= 0x1;
            gprs[15] = jump_address & 0xfffffffe;
            TFlag = jump_address & 0x1;
        } else {
            gprs[14] = (gprs[15] + 4);
            TFlag = 0x1;
            int signed_int = static_cast<s32>(signExtend(inst & 0xFFFFFF, 24));
            signed_int = (signed_int & 0x800000) ? (0x3F000000 | signed_int) : signed_int;
            signed_int = signed_int << 2;
            gprs[15] = gprs[15] + 8 + signed_int + (BIT(inst, 24) << 1);
        }

        return 3;
    }
    gprs[15] += 4;

    return 3;
}
    goto END;
BX_INST:
{
    // Note that only the 'fail' case of BXJ is emulated. This is because
    // the facilities for Jazelle emulation are not implemented.
    //
    // According to the ARM documentation on BXJ, if setting the J bit in the APSR
    // fails, then BXJ functions identically like a regular BX instruction.
    //
    // This is sufficient for citra, as the CPU for the 3DS does not implement Jazelle.

    if (cond == 0xE || conditionPassed(cond)) {


        u32 address = RM;

        if ((inst & 0xFu) == 15)
            address += 2 * 4;

        TFlag = address & 1;
        gprs[15] = address & 0xfffffffe;

        return 3;
    }

    gprs[15] += 4;

    return 3;
}
    goto END;
CLZ_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        clz_inst* inst_cream = (clz_inst*)component;
        RD = clz(RM);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
CMN_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 rn_val = RN;
        if (((inst >> 16) & 0xFu) == 15)
            rn_val += 2 * 4;

        bool carry;
        bool overflow;
        u32 result = AddWithCarry(rn_val, SHIFTER_OPERAND, 0, &carry, &overflow);

        UPDATE_NFLAG(result);
        UPDATE_ZFLAG(result);
        CFlag = carry;
        VFlag = overflow;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
CMP_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 rn_val = RN;
        if (((inst >> 16) & 0xFu) == 15)
            rn_val += 2 * 4;

        bool carry;
        bool overflow;
        u32 result = AddWithCarry(rn_val, ~SHIFTER_OPERAND, 1, &carry, &overflow);

        UPDATE_NFLAG(result);
        UPDATE_ZFLAG(result);
        CFlag = carry;
        VFlag = overflow;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
CPS_INST:
{
    cps_inst* inst_cream = (cps_inst*)component;
    u32 aif_val = 0;
    u32 aif_mask = 0;
    if (InAPrivilegedMode()) {
        if (imod1) {
            if (A) {
                aif_val |= (imod0 << 8);
                aif_mask |= 1 << 8;
            }
            if (((inst >> 25) & 1u)) {
                aif_val |= (imod0 << 7);
                aif_mask |= 1 << 7;
            }
            if (F) {
                aif_val |= (imod0 << 6);
                aif_mask |= 1 << 6;
            }
            aif_mask = ~aif_mask;
            cpsr = (cpsr & aif_mask) | aif_val;
        }
        if (mmod) {
            cpsr = (cpsr & 0xffffffe0) | mode;
            ChangePrivilegeMode(mode);
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
DMB_INST:
    goto END;
DSB_INST:
    goto END;
EOR_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        eor_inst* inst_cream = (eor_inst*)component;

        u32 lop = RN;
        if (((inst >> 16) & 0xFu) == 15) {
            lop += 2 * 4;
        }
        u32 rop = SHIFTER_OPERAND;
        RD = lop ^ rop;
        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = shifter_carry;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
ISB_INST:
    goto END;
LDC_INST:
    return executeCoproc(inst);
LDM_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        ldst_inst* inst_cream = (ldst_inst*)component;
        get_addr(cpu, inst, addr);

        unsigned int inst = inst;
        if (BIT(inst, 22) && !BIT(inst, 15)) {
            for (int i = 0; i < 13; i++) {
                if (BIT(inst, i)) {
                    gprs[i] = mem.read32(addr);
                    addr += 4;
                }
            }
            if (BIT(inst, 13)) {
                if (Mode == USER32MODE)
                    gprs[13] = mem.read32(addr);
                else
                    gprs_usr[0] = mem.read32(addr);

                addr += 4;
            }
            if (BIT(inst, 14)) {
                if (Mode == USER32MODE)
                    gprs[14] = mem.read32(addr);
                else
                    gprs_usr[1] = mem.read32(addr);

                addr += 4;
            }
        } else if (!BIT(inst, 22)) {
            for (int i = 0; i < 16; i++) {
                if (BIT(inst, i)) {
                    unsigned int ret = mem.read32(addr);

                    // For armv5t, should enter thumb when bits[0] is non-zero.
                    if (i == 15) {
                        TFlag = ret & 0x1;
                        ret &= 0xFFFFFFFE;
                    }

                    gprs[i] = ret;
                    addr += 4;
                }
            }
        } else if (BIT(inst, 22) && BIT(inst, 15)) {
            for (int i = 0; i < 15; i++) {
                if (BIT(inst, i)) {
                    gprs[i] = mem.read32(addr);
                    addr += 4;
                }
            }

            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }

            gprs[15] = mem.read32(addr);
        }

        if (BIT(inst, 15)) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
LDR_INST:
{
    ldst_inst* inst_cream = (ldst_inst*)component;
    get_addr(cpu, inst, addr);

    unsigned int value = mem.read32(addr);
    gprs[BITS(inst, 12, 15)] = value;

    if (BITS(inst, 12, 15) == 15) {
        // For armv5t, should enter thumb when bits[0] is non-zero.
        TFlag = value & 0x1;
        gprs[15] &= 0xFFFFFFFE;

        return 3;
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
LDRB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        ldst_inst* inst_cream = (ldst_inst*)component;
        get_addr(cpu, inst, addr);

        gprs[BITS(inst, 12, 15)] = mem.read8(addr);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
LDREX_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        generic_arm_inst* inst_cream = (generic_arm_inst*)component;
        unsigned int read_addr = RN;

        exclusiveMonitor.Set(read_addr);

        RD = mem.read32(read_addr);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
LDREXB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        generic_arm_inst* inst_cream = (generic_arm_inst*)component;
        unsigned int read_addr = RN;

        exclusiveMonitor.Set(read_addr);

        RD = mem.read8(read_addr);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
LDREXD_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        generic_arm_inst* inst_cream = (generic_arm_inst*)component;
        unsigned int read_addr = RN;

        exclusiveMonitor.Set(read_addr);

        RD = mem.read32(read_addr);
        RD2 = mem.read32(read_addr + 4);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
LDREXH_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        generic_arm_inst* inst_cream = (generic_arm_inst*)component;
        unsigned int read_addr = RN;

        exclusiveMonitor.Set(read_addr);

        RD = mem.read16(read_addr);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
LDRH_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        ldst_inst* inst_cream = (ldst_inst*)component;
        get_addr(cpu, inst, addr);

        gprs[BITS(inst, 12, 15)] = mem.read16(addr);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
LDRSB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        ldst_inst* inst_cream = (ldst_inst*)component;
        get_addr(cpu, inst, addr);
        unsigned int value = mem.read8(addr);
        if (BIT(value, 7)) {
            value |= 0xffffff00;
        }
        gprs[BITS(inst, 12, 15)] = value;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
LDRSH_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        ldst_inst* inst_cream = (ldst_inst*)component;
        get_addr(cpu, inst, addr);

        unsigned int value = mem.read16(addr);
        if (BIT(value, 15)) {
            value |= 0xffff0000;
        }
        gprs[BITS(inst, 12, 15)] = value;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
MLA_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        mla_inst* inst_cream = (mla_inst*)component;

        u64 rm = RM;
        u64 rs = RS;
        u64 rn = RN;

        RD = static_cast<u32>((rm * rs + rn) & 0xffffffff);
        if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
MOV_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        mov_inst* inst_cream = (mov_inst*)component;

        RD = SHIFTER_OPERAND;
        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = shifter_carry;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
MOVT_INST:
    goto END;
MOVW_INST:
    goto END;
MRS_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        mrs_inst* inst_cream = (mrs_inst*)component;

        if (R) {
            RD = cpsr;
        } else {
            SAVE_NZCVT;
            RD = cpsr;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
MSR_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        msr_inst* inst_cream = (msr_inst*)component;
        const u32 UserMask = 0xf80f0200, PrivMask = 0x000001df, StateMask = 0x01000020;
        unsigned int inst = inst;
        unsigned int operand;

        if (BIT(inst, 25)) {
            int rot_imm = BITS(inst, 8, 11) * 2;
            operand = ROTATE_RIGHT_32(BITS(inst, 0, 7), rot_imm);
        } else {
            operand = gprs[BITS(inst, 0, 3)];
        }
        u32 byte_mask = (BIT(inst, 16) ? 0xff : 0) | (BIT(inst, 17) ? 0xff00 : 0) |
                        (BIT(inst, 18) ? 0xff0000 : 0) | (BIT(inst, 19) ? 0xff000000 : 0);
        u32 mask = 0;
        if (!R) {
            if (InAPrivilegedMode()) {
                if ((operand & StateMask) != 0) {
                    /// UNPREDICTABLE
                    DEBUG_MSG;
                } else
                    mask = byte_mask & (UserMask | PrivMask);
            } else {
                mask = byte_mask & UserMask;
            }
            SAVE_NZCVT;

            cpsr = (cpsr & ~mask) | (operand & mask);
            ChangePrivilegeMode(cpsr & 0x1F);
            LOAD_NZCVT;
        } else {
            if (CurrentModeHasSPSR()) {
                mask = byte_mask & (UserMask | PrivMask | StateMask);
                cpsr = (cpsr & ~mask) | (operand & mask);
            }
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
MSR_IMM_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        msr_inst* inst_cream = (msr_inst*)component;
        const u32 UserMask = 0xf80f0200, PrivMask = 0x000001df, StateMask = 0x01000020;
        unsigned int inst = inst;
        unsigned int operand;

        if (BIT(inst, 25)) {
            int rot_imm = BITS(inst, 8, 11) * 2;
            operand = ROTATE_RIGHT_32(BITS(inst, 0, 7), rot_imm);
        } else {
            operand = gprs[BITS(inst, 0, 3)];
        }
        u32 byte_mask = (BIT(inst, 16) ? 0xff : 0) | (BIT(inst, 17) ? 0xff00 : 0) |
                        (BIT(inst, 18) ? 0xff0000 : 0) | (BIT(inst, 19) ? 0xff000000 : 0);
        u32 mask = 0;
        if (!R) {
            if (InAPrivilegedMode()) {
                if ((operand & StateMask) != 0) {
                    /// UNPREDICTABLE
                    DEBUG_MSG;
                } else
                    mask = byte_mask & (UserMask | PrivMask);
            } else {
                mask = byte_mask & UserMask;
            }
            SAVE_NZCVT;

            cpsr = (cpsr & ~mask) | (operand & mask);
            ChangePrivilegeMode(cpsr & 0x1F);
            LOAD_NZCVT;
        } else {
            if (CurrentModeHasSPSR()) {
                mask = byte_mask & (UserMask | PrivMask | StateMask);
                cpsr = (cpsr & ~mask) | (operand & mask);
            }
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
MUL_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        mul_inst* inst_cream = (mul_inst*)component;

        u64 rm = RM;
        u64 rs = RS;
        RD = static_cast<u32>((rm * rs) & 0xffffffff);
        if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
MVN_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        RD = ~SHIFTER_OPERAND;

        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = shifter_carry;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
ORR_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 lop = RN;
        u32 rop = SHIFTER_OPERAND;

        if (((inst >> 16) & 0xFu) == 15)
            lop += 2 * 4;

        RD = lop | rop;

        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = shifter_carry;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
PKHBT_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        pkh_inst* inst_cream = (pkh_inst*)component;
        RD = (RN & 0xFFFF) | ((RM << (inst & 0xFFFu)) & 0xFFFF0000);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
PKHTB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        pkh_inst* inst_cream = (pkh_inst*)component;
        int shift_imm = (inst & 0xFFFu) ? (inst & 0xFFFu) : 31;
        RD = ((static_cast<s32>(RM) >> shift_imm) & 0xFFFF) | (RN & 0xFFFF0000);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
PLD_INST:
{
    // Not implemented. PLD is a hint instruction, so it's optional.

    gprs[15] += 4;


    return 1;
}
    goto END;
PLDW_INST:
{
    // Not implemented. PLD is a hint instruction, so it's optional.

    gprs[15] += 4;


    return 1;
}
    goto END;
QADD_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 op1 = ((inst >> 20) & 0xFFu);
        const u32 rm_val = RM;
        const u32 rn_val = RN;

        u32 result = 0;

        // QADD
        if (op1 == 0x00) {
            result = rm_val + rn_val;

            if (AddOverflow(rm_val, rn_val, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QSUB
        else if (op1 == 0x01) {
            result = rm_val - rn_val;

            if (SubOverflow(rm_val, rn_val, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QDADD
        else if (op1 == 0x02) {
            u32 mul = (rn_val * 2);

            if (AddOverflow(rn_val, rn_val, rn_val * 2)) {
                mul = POS(mul) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }

            result = mul + rm_val;

            if (AddOverflow(rm_val, mul, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QDSUB
        else if (op1 == 0x03) {
            u32 mul = (rn_val * 2);

            if (AddOverflow(rn_val, rn_val, mul)) {
                mul = POS(mul) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }

            result = rm_val - mul;

            if (SubOverflow(rm_val, mul, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }

        RD = result;
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
QDADD_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 op1 = ((inst >> 20) & 0xFFu);
        const u32 rm_val = RM;
        const u32 rn_val = RN;

        u32 result = 0;

        // QADD
        if (op1 == 0x00) {
            result = rm_val + rn_val;

            if (AddOverflow(rm_val, rn_val, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QSUB
        else if (op1 == 0x01) {
            result = rm_val - rn_val;

            if (SubOverflow(rm_val, rn_val, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QDADD
        else if (op1 == 0x02) {
            u32 mul = (rn_val * 2);

            if (AddOverflow(rn_val, rn_val, rn_val * 2)) {
                mul = POS(mul) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }

            result = mul + rm_val;

            if (AddOverflow(rm_val, mul, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QDSUB
        else if (op1 == 0x03) {
            u32 mul = (rn_val * 2);

            if (AddOverflow(rn_val, rn_val, mul)) {
                mul = POS(mul) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }

            result = rm_val - mul;

            if (SubOverflow(rm_val, mul, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }

        RD = result;
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
QDSUB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 op1 = ((inst >> 20) & 0xFFu);
        const u32 rm_val = RM;
        const u32 rn_val = RN;

        u32 result = 0;

        // QADD
        if (op1 == 0x00) {
            result = rm_val + rn_val;

            if (AddOverflow(rm_val, rn_val, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QSUB
        else if (op1 == 0x01) {
            result = rm_val - rn_val;

            if (SubOverflow(rm_val, rn_val, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QDADD
        else if (op1 == 0x02) {
            u32 mul = (rn_val * 2);

            if (AddOverflow(rn_val, rn_val, rn_val * 2)) {
                mul = POS(mul) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }

            result = mul + rm_val;

            if (AddOverflow(rm_val, mul, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QDSUB
        else if (op1 == 0x03) {
            u32 mul = (rn_val * 2);

            if (AddOverflow(rn_val, rn_val, mul)) {
                mul = POS(mul) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }

            result = rm_val - mul;

            if (SubOverflow(rm_val, mul, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }

        RD = result;
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
QSUB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 op1 = ((inst >> 20) & 0xFFu);
        const u32 rm_val = RM;
        const u32 rn_val = RN;

        u32 result = 0;

        // QADD
        if (op1 == 0x00) {
            result = rm_val + rn_val;

            if (AddOverflow(rm_val, rn_val, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QSUB
        else if (op1 == 0x01) {
            result = rm_val - rn_val;

            if (SubOverflow(rm_val, rn_val, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QDADD
        else if (op1 == 0x02) {
            u32 mul = (rn_val * 2);

            if (AddOverflow(rn_val, rn_val, rn_val * 2)) {
                mul = POS(mul) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }

            result = mul + rm_val;

            if (AddOverflow(rm_val, mul, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }
        // QDSUB
        else if (op1 == 0x03) {
            u32 mul = (rn_val * 2);

            if (AddOverflow(rn_val, rn_val, mul)) {
                mul = POS(mul) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }

            result = rm_val - mul;

            if (SubOverflow(rm_val, mul, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpsr |= (1 << 27);
            }
        }

        RD = result;
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
REV_INST:
{

    if (cond == 0xE || conditionPassed(cond)) {


        const u8 op1 = ((inst >> 20) & 0xFFu);
        const u8 op2 = ((inst >> 4) & 0xFFu);

        // REV
        if (op1 == 0x03 && op2 == 0x01) {
            RD = ((RM & 0xFF) << 24) | (((RM >> 8) & 0xFF) << 16) | (((RM >> 16) & 0xFF) << 8) |
                 ((RM >> 24) & 0xFF);
        }
        // REV16
        else if (op1 == 0x03 && op2 == 0x05) {
            RD = ((RM & 0xFF) << 8) | ((RM & 0xFF00) >> 8) | ((RM & 0xFF0000) << 8) |
                 ((RM & 0xFF000000) >> 8);
        }
        // REVSH
        else if (op1 == 0x07 && op2 == 0x05) {
            RD = ((RM & 0xFF) << 8) | ((RM & 0xFF00) >> 8);
            if (RD & 0x8000)
                RD |= 0xffff0000;
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
REV16_INST:
{

    if (cond == 0xE || conditionPassed(cond)) {


        const u8 op1 = ((inst >> 20) & 0xFFu);
        const u8 op2 = ((inst >> 4) & 0xFFu);

        // REV
        if (op1 == 0x03 && op2 == 0x01) {
            RD = ((RM & 0xFF) << 24) | (((RM >> 8) & 0xFF) << 16) | (((RM >> 16) & 0xFF) << 8) |
                 ((RM >> 24) & 0xFF);
        }
        // REV16
        else if (op1 == 0x03 && op2 == 0x05) {
            RD = ((RM & 0xFF) << 8) | ((RM & 0xFF00) >> 8) | ((RM & 0xFF0000) << 8) |
                 ((RM & 0xFF000000) >> 8);
        }
        // REVSH
        else if (op1 == 0x07 && op2 == 0x05) {
            RD = ((RM & 0xFF) << 8) | ((RM & 0xFF00) >> 8);
            if (RD & 0x8000)
                RD |= 0xffff0000;
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
REVSH_INST:
{

    if (cond == 0xE || conditionPassed(cond)) {


        const u8 op1 = ((inst >> 20) & 0xFFu);
        const u8 op2 = ((inst >> 4) & 0xFFu);

        // REV
        if (op1 == 0x03 && op2 == 0x01) {
            RD = ((RM & 0xFF) << 24) | (((RM >> 8) & 0xFF) << 16) | (((RM >> 16) & 0xFF) << 8) |
                 ((RM >> 24) & 0xFF);
        }
        // REV16
        else if (op1 == 0x03 && op2 == 0x05) {
            RD = ((RM & 0xFF) << 8) | ((RM & 0xFF00) >> 8) | ((RM & 0xFF0000) << 8) |
                 ((RM & 0xFF000000) >> 8);
        }
        // REVSH
        else if (op1 == 0x07 && op2 == 0x05) {
            RD = ((RM & 0xFF) << 8) | ((RM & 0xFF00) >> 8);
            if (RD & 0x8000)
                RD |= 0xffff0000;
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
RSB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 rn_val = RN;
        if (((inst >> 16) & 0xFu) == 15)
            rn_val += 2 * 4;

        bool carry;
        bool overflow;
        RD = AddWithCarry(~rn_val, SHIFTER_OPERAND, 1, &carry, &overflow);

        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = carry;
            VFlag = overflow;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
RSC_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 rn_val = RN;
        if (((inst >> 16) & 0xFu) == 15)
            rn_val += 2 * 4;

        bool carry;
        bool overflow;
        RD = AddWithCarry(~rn_val, SHIFTER_OPERAND, CFlag, &carry, &overflow);

        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = carry;
            VFlag = overflow;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SADD16_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 op2 = ((inst >> 4) & 0xFFu);

        if (op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03) {
            const s16 rn_lo = (RN & 0xFFFF);
            const s16 rn_hi = ((RN >> 16) & 0xFFFF);
            const s16 rm_lo = (RM & 0xFFFF);
            const s16 rm_hi = ((RM >> 16) & 0xFFFF);

            s32 lo_result = 0;
            s32 hi_result = 0;

            // SADD16
            if (((inst >> 4) & 0xFFu) == 0x00) {
                lo_result = (rn_lo + rm_lo);
                hi_result = (rn_hi + rm_hi);
            }
            // SASX
            else if (op2 == 0x01) {
                lo_result = (rn_lo - rm_hi);
                hi_result = (rn_hi + rm_lo);
            }
            // SSAX
            else if (op2 == 0x02) {
                lo_result = (rn_lo + rm_hi);
                hi_result = (rn_hi - rm_lo);
            }
            // SSUB16
            else if (op2 == 0x03) {
                lo_result = (rn_lo - rm_lo);
                hi_result = (rn_hi - rm_hi);
            }

            RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);

            if (lo_result >= 0) {
                cpsr |= (1 << 16);
                cpsr |= (1 << 17);
            } else {
                cpsr &= ~(1 << 16);
                cpsr &= ~(1 << 17);
            }

            if (hi_result >= 0) {
                cpsr |= (1 << 18);
                cpsr |= (1 << 19);
            } else {
                cpsr &= ~(1 << 18);
                cpsr &= ~(1 << 19);
            }
        } else if (op2 == 0x04 || op2 == 0x07) {
            s32 lo_val1, lo_val2;
            s32 hi_val1, hi_val2;

            // SADD8
            if (op2 == 0x04) {
                lo_val1 = (s32)(s8)(RN & 0xFF) + (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) + (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) + (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) + (s32)(s8)((RM >> 24) & 0xFF);
            }
            // SSUB8
            else {
                lo_val1 = (s32)(s8)(RN & 0xFF) - (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) - (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) - (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) - (s32)(s8)((RM >> 24) & 0xFF);
            }

            RD = ((lo_val1 & 0xFF) | ((lo_val2 & 0xFF) << 8) | ((hi_val1 & 0xFF) << 16) |
                  ((hi_val2 & 0xFF) << 24));

            if (lo_val1 >= 0)
                cpsr |= (1 << 16);
            else
                cpsr &= ~(1 << 16);

            if (lo_val2 >= 0)
                cpsr |= (1 << 17);
            else
                cpsr &= ~(1 << 17);

            if (hi_val1 >= 0)
                cpsr |= (1 << 18);
            else
                cpsr &= ~(1 << 18);

            if (hi_val2 >= 0)
                cpsr |= (1 << 19);
            else
                cpsr &= ~(1 << 19);
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
SADD8_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 op2 = ((inst >> 4) & 0xFFu);

        if (op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03) {
            const s16 rn_lo = (RN & 0xFFFF);
            const s16 rn_hi = ((RN >> 16) & 0xFFFF);
            const s16 rm_lo = (RM & 0xFFFF);
            const s16 rm_hi = ((RM >> 16) & 0xFFFF);

            s32 lo_result = 0;
            s32 hi_result = 0;

            // SADD16
            if (((inst >> 4) & 0xFFu) == 0x00) {
                lo_result = (rn_lo + rm_lo);
                hi_result = (rn_hi + rm_hi);
            }
            // SASX
            else if (op2 == 0x01) {
                lo_result = (rn_lo - rm_hi);
                hi_result = (rn_hi + rm_lo);
            }
            // SSAX
            else if (op2 == 0x02) {
                lo_result = (rn_lo + rm_hi);
                hi_result = (rn_hi - rm_lo);
            }
            // SSUB16
            else if (op2 == 0x03) {
                lo_result = (rn_lo - rm_lo);
                hi_result = (rn_hi - rm_hi);
            }

            RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);

            if (lo_result >= 0) {
                cpsr |= (1 << 16);
                cpsr |= (1 << 17);
            } else {
                cpsr &= ~(1 << 16);
                cpsr &= ~(1 << 17);
            }

            if (hi_result >= 0) {
                cpsr |= (1 << 18);
                cpsr |= (1 << 19);
            } else {
                cpsr &= ~(1 << 18);
                cpsr &= ~(1 << 19);
            }
        } else if (op2 == 0x04 || op2 == 0x07) {
            s32 lo_val1, lo_val2;
            s32 hi_val1, hi_val2;

            // SADD8
            if (op2 == 0x04) {
                lo_val1 = (s32)(s8)(RN & 0xFF) + (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) + (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) + (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) + (s32)(s8)((RM >> 24) & 0xFF);
            }
            // SSUB8
            else {
                lo_val1 = (s32)(s8)(RN & 0xFF) - (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) - (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) - (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) - (s32)(s8)((RM >> 24) & 0xFF);
            }

            RD = ((lo_val1 & 0xFF) | ((lo_val2 & 0xFF) << 8) | ((hi_val1 & 0xFF) << 16) |
                  ((hi_val2 & 0xFF) << 24));

            if (lo_val1 >= 0)
                cpsr |= (1 << 16);
            else
                cpsr &= ~(1 << 16);

            if (lo_val2 >= 0)
                cpsr |= (1 << 17);
            else
                cpsr &= ~(1 << 17);

            if (hi_val1 >= 0)
                cpsr |= (1 << 18);
            else
                cpsr &= ~(1 << 18);

            if (hi_val2 >= 0)
                cpsr |= (1 << 19);
            else
                cpsr &= ~(1 << 19);
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
SADDSUBX_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 op2 = ((inst >> 4) & 0xFFu);

        if (op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03) {
            const s16 rn_lo = (RN & 0xFFFF);
            const s16 rn_hi = ((RN >> 16) & 0xFFFF);
            const s16 rm_lo = (RM & 0xFFFF);
            const s16 rm_hi = ((RM >> 16) & 0xFFFF);

            s32 lo_result = 0;
            s32 hi_result = 0;

            // SADD16
            if (((inst >> 4) & 0xFFu) == 0x00) {
                lo_result = (rn_lo + rm_lo);
                hi_result = (rn_hi + rm_hi);
            }
            // SASX
            else if (op2 == 0x01) {
                lo_result = (rn_lo - rm_hi);
                hi_result = (rn_hi + rm_lo);
            }
            // SSAX
            else if (op2 == 0x02) {
                lo_result = (rn_lo + rm_hi);
                hi_result = (rn_hi - rm_lo);
            }
            // SSUB16
            else if (op2 == 0x03) {
                lo_result = (rn_lo - rm_lo);
                hi_result = (rn_hi - rm_hi);
            }

            RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);

            if (lo_result >= 0) {
                cpsr |= (1 << 16);
                cpsr |= (1 << 17);
            } else {
                cpsr &= ~(1 << 16);
                cpsr &= ~(1 << 17);
            }

            if (hi_result >= 0) {
                cpsr |= (1 << 18);
                cpsr |= (1 << 19);
            } else {
                cpsr &= ~(1 << 18);
                cpsr &= ~(1 << 19);
            }
        } else if (op2 == 0x04 || op2 == 0x07) {
            s32 lo_val1, lo_val2;
            s32 hi_val1, hi_val2;

            // SADD8
            if (op2 == 0x04) {
                lo_val1 = (s32)(s8)(RN & 0xFF) + (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) + (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) + (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) + (s32)(s8)((RM >> 24) & 0xFF);
            }
            // SSUB8
            else {
                lo_val1 = (s32)(s8)(RN & 0xFF) - (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) - (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) - (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) - (s32)(s8)((RM >> 24) & 0xFF);
            }

            RD = ((lo_val1 & 0xFF) | ((lo_val2 & 0xFF) << 8) | ((hi_val1 & 0xFF) << 16) |
                  ((hi_val2 & 0xFF) << 24));

            if (lo_val1 >= 0)
                cpsr |= (1 << 16);
            else
                cpsr &= ~(1 << 16);

            if (lo_val2 >= 0)
                cpsr |= (1 << 17);
            else
                cpsr &= ~(1 << 17);

            if (hi_val1 >= 0)
                cpsr |= (1 << 18);
            else
                cpsr &= ~(1 << 18);

            if (hi_val2 >= 0)
                cpsr |= (1 << 19);
            else
                cpsr &= ~(1 << 19);
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
SBC_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 rn_val = RN;
        if (((inst >> 16) & 0xFu) == 15)
            rn_val += 2 * 4;

        bool carry;
        bool overflow;
        RD = AddWithCarry(rn_val, ~SHIFTER_OPERAND, CFlag, &carry, &overflow);

        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = carry;
            VFlag = overflow;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SBFX_INST:
    goto END;
SEL_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        const u32 to = RM;
        const u32 from = RN;
        const u32 cpsr = cpsr;

        u32 result;
        if (cpsr & (1 << 16))
            result = from & 0xff;
        else
            result = to & 0xff;

        if (cpsr & (1 << 17))
            result |= from & 0x0000ff00;
        else
            result |= to & 0x0000ff00;

        if (cpsr & (1 << 18))
            result |= from & 0x00ff0000;
        else
            result |= to & 0x00ff0000;

        if (cpsr & (1 << 19))
            result |= from & 0xff000000;
        else
            result |= to & 0xff000000;

        RD = result;
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
SMLAXY_INST:
    goto END;
SMLAL_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        umlal_inst* inst_cream = (umlal_inst*)component;
        long long int rm = RM;
        long long int rs = RS;
        if (BIT(rm, 31)) {
            rm |= 0xffffffff00000000LL;
        }
        if (BIT(rs, 31)) {
            rs |= 0xffffffff00000000LL;
        }
        long long int rst = rm * rs;
        long long int rdhi32 = RDHI;
        long long int hilo = (rdhi32 << 32) + RDLO;
        rst += hilo;
        RDLO = BITS(rst, 0, 31);
        RDHI = BITS(rst, 32, 63);
        if (((inst >> 20) & 1u)) {
            NFlag = BIT(RDHI, 31);
            ZFlag = (RDHI == 0 && RDLO == 0);
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SMULL_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        umull_inst* inst_cream = (umull_inst*)component;
        s64 rm = RM;
        s64 rs = RS;
        if (BIT(rm, 31)) {
            rm |= 0xffffffff00000000LL;
        }
        if (BIT(rs, 31)) {
            rs |= 0xffffffff00000000LL;
        }
        s64 rst = rm * rs;
        RDHI = BITS(rst, 32, 63);
        RDLO = BITS(rst, 0, 31);

        if (((inst >> 20) & 1u)) {
            NFlag = BIT(RDHI, 31);
            ZFlag = (RDHI == 0 && RDLO == 0);
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SSAT_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u8 shift_type = shift_type;
        u8 shift_amount = (inst & 0xFFFu)5;
        u32 rn_val = RN;

        // 32-bit ASR is encoded as an amount of 0.
        if (shift_type == 1 && shift_amount == 0)
            shift_amount = 31;

        if (shift_type == 0)
            rn_val <<= shift_amount;
        else if (shift_type == 1)
            rn_val = ((s32)rn_val >> shift_amount);

        bool saturated = false;
        rn_val = ARMul_SignedSatQ(rn_val, sat_imm, &saturated);

        if (saturated)
            cpsr |= (1 << 27);

        RD = rn_val;
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
SSAT16_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 saturate_to = sat_imm;

        bool sat1 = false;
        bool sat2 = false;

        RD = (ARMul_SignedSatQ((s16)RN, saturate_to, &sat1) & 0xFFFF) |
             ARMul_SignedSatQ((s32)RN >> 16, saturate_to, &sat2) << 16;

        if (sat1 || sat2)
            cpsr |= (1 << 27);
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
SSUB16_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 op2 = ((inst >> 4) & 0xFFu);

        if (op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03) {
            const s16 rn_lo = (RN & 0xFFFF);
            const s16 rn_hi = ((RN >> 16) & 0xFFFF);
            const s16 rm_lo = (RM & 0xFFFF);
            const s16 rm_hi = ((RM >> 16) & 0xFFFF);

            s32 lo_result = 0;
            s32 hi_result = 0;

            // SADD16
            if (((inst >> 4) & 0xFFu) == 0x00) {
                lo_result = (rn_lo + rm_lo);
                hi_result = (rn_hi + rm_hi);
            }
            // SASX
            else if (op2 == 0x01) {
                lo_result = (rn_lo - rm_hi);
                hi_result = (rn_hi + rm_lo);
            }
            // SSAX
            else if (op2 == 0x02) {
                lo_result = (rn_lo + rm_hi);
                hi_result = (rn_hi - rm_lo);
            }
            // SSUB16
            else if (op2 == 0x03) {
                lo_result = (rn_lo - rm_lo);
                hi_result = (rn_hi - rm_hi);
            }

            RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);

            if (lo_result >= 0) {
                cpsr |= (1 << 16);
                cpsr |= (1 << 17);
            } else {
                cpsr &= ~(1 << 16);
                cpsr &= ~(1 << 17);
            }

            if (hi_result >= 0) {
                cpsr |= (1 << 18);
                cpsr |= (1 << 19);
            } else {
                cpsr &= ~(1 << 18);
                cpsr &= ~(1 << 19);
            }
        } else if (op2 == 0x04 || op2 == 0x07) {
            s32 lo_val1, lo_val2;
            s32 hi_val1, hi_val2;

            // SADD8
            if (op2 == 0x04) {
                lo_val1 = (s32)(s8)(RN & 0xFF) + (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) + (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) + (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) + (s32)(s8)((RM >> 24) & 0xFF);
            }
            // SSUB8
            else {
                lo_val1 = (s32)(s8)(RN & 0xFF) - (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) - (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) - (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) - (s32)(s8)((RM >> 24) & 0xFF);
            }

            RD = ((lo_val1 & 0xFF) | ((lo_val2 & 0xFF) << 8) | ((hi_val1 & 0xFF) << 16) |
                  ((hi_val2 & 0xFF) << 24));

            if (lo_val1 >= 0)
                cpsr |= (1 << 16);
            else
                cpsr &= ~(1 << 16);

            if (lo_val2 >= 0)
                cpsr |= (1 << 17);
            else
                cpsr &= ~(1 << 17);

            if (hi_val1 >= 0)
                cpsr |= (1 << 18);
            else
                cpsr &= ~(1 << 18);

            if (hi_val2 >= 0)
                cpsr |= (1 << 19);
            else
                cpsr &= ~(1 << 19);
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
SSUB8_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 op2 = ((inst >> 4) & 0xFFu);

        if (op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03) {
            const s16 rn_lo = (RN & 0xFFFF);
            const s16 rn_hi = ((RN >> 16) & 0xFFFF);
            const s16 rm_lo = (RM & 0xFFFF);
            const s16 rm_hi = ((RM >> 16) & 0xFFFF);

            s32 lo_result = 0;
            s32 hi_result = 0;

            // SADD16
            if (((inst >> 4) & 0xFFu) == 0x00) {
                lo_result = (rn_lo + rm_lo);
                hi_result = (rn_hi + rm_hi);
            }
            // SASX
            else if (op2 == 0x01) {
                lo_result = (rn_lo - rm_hi);
                hi_result = (rn_hi + rm_lo);
            }
            // SSAX
            else if (op2 == 0x02) {
                lo_result = (rn_lo + rm_hi);
                hi_result = (rn_hi - rm_lo);
            }
            // SSUB16
            else if (op2 == 0x03) {
                lo_result = (rn_lo - rm_lo);
                hi_result = (rn_hi - rm_hi);
            }

            RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);

            if (lo_result >= 0) {
                cpsr |= (1 << 16);
                cpsr |= (1 << 17);
            } else {
                cpsr &= ~(1 << 16);
                cpsr &= ~(1 << 17);
            }

            if (hi_result >= 0) {
                cpsr |= (1 << 18);
                cpsr |= (1 << 19);
            } else {
                cpsr &= ~(1 << 18);
                cpsr &= ~(1 << 19);
            }
        } else if (op2 == 0x04 || op2 == 0x07) {
            s32 lo_val1, lo_val2;
            s32 hi_val1, hi_val2;

            // SADD8
            if (op2 == 0x04) {
                lo_val1 = (s32)(s8)(RN & 0xFF) + (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) + (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) + (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) + (s32)(s8)((RM >> 24) & 0xFF);
            }
            // SSUB8
            else {
                lo_val1 = (s32)(s8)(RN & 0xFF) - (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) - (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) - (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) - (s32)(s8)((RM >> 24) & 0xFF);
            }

            RD = ((lo_val1 & 0xFF) | ((lo_val2 & 0xFF) << 8) | ((hi_val1 & 0xFF) << 16) |
                  ((hi_val2 & 0xFF) << 24));

            if (lo_val1 >= 0)
                cpsr |= (1 << 16);
            else
                cpsr &= ~(1 << 16);

            if (lo_val2 >= 0)
                cpsr |= (1 << 17);
            else
                cpsr &= ~(1 << 17);

            if (hi_val1 >= 0)
                cpsr |= (1 << 18);
            else
                cpsr &= ~(1 << 18);

            if (hi_val2 >= 0)
                cpsr |= (1 << 19);
            else
                cpsr &= ~(1 << 19);
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
SSUBADDX_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 op2 = ((inst >> 4) & 0xFFu);

        if (op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03) {
            const s16 rn_lo = (RN & 0xFFFF);
            const s16 rn_hi = ((RN >> 16) & 0xFFFF);
            const s16 rm_lo = (RM & 0xFFFF);
            const s16 rm_hi = ((RM >> 16) & 0xFFFF);

            s32 lo_result = 0;
            s32 hi_result = 0;

            // SADD16
            if (((inst >> 4) & 0xFFu) == 0x00) {
                lo_result = (rn_lo + rm_lo);
                hi_result = (rn_hi + rm_hi);
            }
            // SASX
            else if (op2 == 0x01) {
                lo_result = (rn_lo - rm_hi);
                hi_result = (rn_hi + rm_lo);
            }
            // SSAX
            else if (op2 == 0x02) {
                lo_result = (rn_lo + rm_hi);
                hi_result = (rn_hi - rm_lo);
            }
            // SSUB16
            else if (op2 == 0x03) {
                lo_result = (rn_lo - rm_lo);
                hi_result = (rn_hi - rm_hi);
            }

            RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);

            if (lo_result >= 0) {
                cpsr |= (1 << 16);
                cpsr |= (1 << 17);
            } else {
                cpsr &= ~(1 << 16);
                cpsr &= ~(1 << 17);
            }

            if (hi_result >= 0) {
                cpsr |= (1 << 18);
                cpsr |= (1 << 19);
            } else {
                cpsr &= ~(1 << 18);
                cpsr &= ~(1 << 19);
            }
        } else if (op2 == 0x04 || op2 == 0x07) {
            s32 lo_val1, lo_val2;
            s32 hi_val1, hi_val2;

            // SADD8
            if (op2 == 0x04) {
                lo_val1 = (s32)(s8)(RN & 0xFF) + (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) + (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) + (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) + (s32)(s8)((RM >> 24) & 0xFF);
            }
            // SSUB8
            else {
                lo_val1 = (s32)(s8)(RN & 0xFF) - (s32)(s8)(RM & 0xFF);
                lo_val2 = (s32)(s8)((RN >> 8) & 0xFF) - (s32)(s8)((RM >> 8) & 0xFF);
                hi_val1 = (s32)(s8)((RN >> 16) & 0xFF) - (s32)(s8)((RM >> 16) & 0xFF);
                hi_val2 = (s32)(s8)((RN >> 24) & 0xFF) - (s32)(s8)((RM >> 24) & 0xFF);
            }

            RD = ((lo_val1 & 0xFF) | ((lo_val2 & 0xFF) << 8) | ((hi_val1 & 0xFF) << 16) |
                  ((hi_val2 & 0xFF) << 24));

            if (lo_val1 >= 0)
                cpsr |= (1 << 16);
            else
                cpsr &= ~(1 << 16);

            if (lo_val2 >= 0)
                cpsr |= (1 << 17);
            else
                cpsr &= ~(1 << 17);

            if (hi_val1 >= 0)
                cpsr |= (1 << 18);
            else
                cpsr &= ~(1 << 18);

            if (hi_val2 >= 0)
                cpsr |= (1 << 19);
            else
                cpsr &= ~(1 << 19);
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
STC_INST:
    return executeCoproc(inst);
STM_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        ldst_inst* inst_cream = (ldst_inst*)component;
        unsigned int inst = inst;

        unsigned int Rn = BITS(inst, 16, 19);
        unsigned int old_RN = gprs[Rn];

        get_addr(cpu, inst, addr);
        if (BIT(inst, 22) == 1) {
            for (int i = 0; i < 13; i++) {
                if (BIT(inst, i)) {
                    mem.write32(addr, gprs[i]);
                    addr += 4;
                }
            }
            if (BIT(inst, 13)) {
                if (Mode == USER32MODE)
                    mem.write32(addr, gprs[13]);
                else
                    mem.write32(addr, gprs_usr[0]);

                addr += 4;
            }
            if (BIT(inst, 14)) {
                if (Mode == USER32MODE)
                    mem.write32(addr, gprs[14]);
                else
                    mem.write32(addr, gprs_usr[1]);

                addr += 4;
            }
            if (BIT(inst, 15)) {
                mem.write32(addr, gprs[15] + 8);
            }
        } else {
            for (unsigned int i = 0; i < 15; i++) {
                if (BIT(inst, i)) {
                    if (i == Rn)
                        mem.write32(addr, old_RN);
                    else
                        mem.write32(addr, gprs[i]);

                    addr += 4;
                }
            }

            // Check PC reg
            if (BIT(inst, 15)) {
                mem.write32(addr, gprs[15] + 8);
            }
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
STR_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        ldst_inst* inst_cream = (ldst_inst*)component;
        get_addr(cpu, inst, addr);

        unsigned int reg = BITS(inst, 12, 15);
        unsigned int value = gprs[reg];

        if (reg == 15)
            value += 2 * 4;

        mem.write32(addr, value);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
STRB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        ldst_inst* inst_cream = (ldst_inst*)component;
        get_addr(cpu, inst, addr);
        unsigned int value = gprs[BITS(inst, 12, 15)] & 0xff;
        mem.write8(addr, value);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
STREX_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        generic_arm_inst* inst_cream = (generic_arm_inst*)component;
        unsigned int write_addr = gprs[((inst >> 16) & 0xFu)];

        if (IsExclusiveMemoryAccess(write_addr)) {
            UnsetExclusiveMemoryAddress();
            mem.write32(write_addr, RM);
            RD = 0;
        } else {
            // Failed to write due to mutex access
            RD = 1;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
STREXB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        generic_arm_inst* inst_cream = (generic_arm_inst*)component;
        unsigned int write_addr = gprs[((inst >> 16) & 0xFu)];

        if (IsExclusiveMemoryAccess(write_addr)) {
            UnsetExclusiveMemoryAddress();
            mem.write8(write_addr, gprs[(inst & 0xFu)]);
            RD = 0;
        } else {
            // Failed to write due to mutex access
            RD = 1;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
STREXD_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        generic_arm_inst* inst_cream = (generic_arm_inst*)component;
        unsigned int write_addr = gprs[((inst >> 16) & 0xFu)];

        if (IsExclusiveMemoryAccess(write_addr)) {
            UnsetExclusiveMemoryAddress();

            const u32 rt = gprs[(inst & 0xFu) + 0];
            const u32 rt2 = gprs[(inst & 0xFu) + 1];
            u64 value;

            if (InBigEndianMode())
                value = (((u64)rt << 32) | rt2);
            else
                value = (((u64)rt2 << 32) | rt);

            WriteMemory64(write_addr, value);
            RD = 0;
        } else {
            // Failed to write due to mutex access
            RD = 1;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
STREXH_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        generic_arm_inst* inst_cream = (generic_arm_inst*)component;
        unsigned int write_addr = gprs[((inst >> 16) & 0xFu)];

        if (IsExclusiveMemoryAccess(write_addr)) {
            UnsetExclusiveMemoryAddress();
            mem.write16(write_addr, RM);
            RD = 0;
        } else {
            // Failed to write due to mutex access
            RD = 1;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
STRH_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        ldst_inst* inst_cream = (ldst_inst*)component;
        get_addr(cpu, inst, addr);

        unsigned int value = gprs[BITS(inst, 12, 15)] & 0xffff;
        mem.write16(addr, value);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SUB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 rn_val = (((((inst >> 16) == 15) ? (old_pc + 8) : gprs[((inst >> 16]) & 0xFu));

        bool carry;
        bool overflow;
        RD = AddWithCarry(rn_val, ~SHIFTER_OPERAND, 1, &carry, &overflow);

        if (((inst >> 20) & 1u) && (((inst >> 12) & 0xFu) == 15)) {
            if (CurrentModeHasSPSR()) {
                cpsr = cpsr;
                ChangePrivilegeMode(cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (((inst >> 20) & 1u)) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            CFlag = carry;
            VFlag = overflow;
        }
        if (((inst >> 12) & 0xFu) == 15) {

            return 3;
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SWP_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        swp_inst* inst_cream = (swp_inst*)component;

        addr = RN;
        unsigned int value = mem.read32(addr);
        mem.write32(addr, RM);

        RD = value;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SWPB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        swp_inst* inst_cream = (swp_inst*)component;

        addr = RN;
        unsigned int value = mem.read32(addr);
        mem.write32(addr, RM);

        RD = value;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SXTAB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        sxtab_inst* inst_cream = (sxtab_inst*)component;

        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * ((inst >> 8) & 0xFu)) & 0xff;

        // Sign extend for byte
        operand2 = (0x80 & operand2) ? (0xFFFFFF00 | operand2) : operand2;
        RD = RN + operand2;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SXTAB16_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        const u8 rotation = ((inst >> 8) & 0xFu) * 8;
        u32 rm_val = RM;
        u32 rn_val = RN;

        if (rotation)
            rm_val = ((rm_val << (32 - rotation)) | (rm_val >> rotation));

        // SXTB16
        if (((inst >> 16) & 0xFu) == 15) {
            u32 lo = (u32)(s8)rm_val;
            u32 hi = (u32)(s8)(rm_val >> 16);
            RD = (lo & 0xFFFF) | (hi << 16);
        }
        // SXTAB16
        else {
            u32 lo = rn_val + (u32)(s8)(rm_val & 0xFF);
            u32 hi = (rn_val >> 16) + (u32)(s8)((rm_val >> 16) & 0xFF);
            RD = (lo & 0xFFFF) | (hi << 16);
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
SXTAH_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        sxtah_inst* inst_cream = (sxtah_inst*)component;

        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * ((inst >> 8) & 0xFu)) & 0xffff;
        // Sign extend for half
        operand2 = (0x8000 & operand2) ? (0xFFFF0000 | operand2) : operand2;
        RD = RN + operand2;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SXTB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        sxtb_inst* inst_cream = (sxtb_inst*)component;

        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * ((inst >> 8) & 0xFu));
        if (BIT(operand2, 7)) {
            operand2 |= 0xffffff00;
        } else {
            operand2 &= 0xff;
        }
        RD = operand2;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
SXTB16_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        const u8 rotation = ((inst >> 8) & 0xFu) * 8;
        u32 rm_val = RM;
        u32 rn_val = RN;

        if (rotation)
            rm_val = ((rm_val << (32 - rotation)) | (rm_val >> rotation));

        // SXTB16
        if (((inst >> 16) & 0xFu) == 15) {
            u32 lo = (u32)(s8)rm_val;
            u32 hi = (u32)(s8)(rm_val >> 16);
            RD = (lo & 0xFFFF) | (hi << 16);
        }
        // SXTAB16
        else {
            u32 lo = rn_val + (u32)(s8)(rm_val & 0xFF);
            u32 hi = (rn_val >> 16) + (u32)(s8)((rm_val >> 16) & 0xFF);
            RD = (lo & 0xFFFF) | (hi << 16);
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
SXTH_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        sxth_inst* inst_cream = (sxth_inst*)component;

        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * ((inst >> 8) & 0xFu));
        if (BIT(operand2, 15)) {
            operand2 |= 0xffff0000;
        } else {
            operand2 &= 0xffff;
        }
        RD = operand2;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
TEQ_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 lop = RN;
        u32 rop = SHIFTER_OPERAND;

        if (((inst >> 16) & 0xFu) == 15)
            lop += 4 * 2;

        u32 result = lop ^ rop;

        UPDATE_NFLAG(result);
        UPDATE_ZFLAG(result);
        CFlag = shifter_carry;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
TST_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u32 lop = RN;
        u32 rop = SHIFTER_OPERAND;

        if (((inst >> 16) & 0xFu) == 15)
            lop += 4 * 2;

        u32 result = lop & rop;

        UPDATE_NFLAG(result);
        UPDATE_ZFLAG(result);
        CFlag = shifter_carry;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
UBFX_INST:
    goto END;
UMAAL_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u64 rm = RM;
        const u64 rn = RN;
        const u64 rd_lo = RDLO;
        const u64 rd_hi = RDHI;
        const u64 result = (rm * rn) + rd_lo + rd_hi;

        RDLO = (result & 0xFFFFFFFF);
        RDHI = ((result >> 32) & 0xFFFFFFFF);
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
UMLAL_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        umlal_inst* inst_cream = (umlal_inst*)component;
        unsigned long long int rm = RM;
        unsigned long long int rs = RS;
        unsigned long long int rst = rm * rs;
        unsigned long long int add = ((unsigned long long)RDHI) << 32;
        add += RDLO;
        rst += add;
        RDLO = BITS(rst, 0, 31);
        RDHI = BITS(rst, 32, 63);

        if (((inst >> 20) & 1u)) {
            NFlag = BIT(RDHI, 31);
            ZFlag = (RDHI == 0 && RDLO == 0);
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
UMULL_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        umull_inst* inst_cream = (umull_inst*)component;
        unsigned long long int rm = RM;
        unsigned long long int rs = RS;
        unsigned long long int rst = rm * rs;
        RDHI = BITS(rst, 32, 63);
        RDLO = BITS(rst, 0, 31);

        if (((inst >> 20) & 1u)) {
            NFlag = BIT(RDHI, 31);
            ZFlag = (RDHI == 0 && RDLO == 0);
        }
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
USAT_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        u8 shift_type = shift_type;
        u8 shift_amount = (inst & 0xFFFu)5;
        u32 rn_val = RN;

        // 32-bit ASR is encoded as an amount of 0.
        if (shift_type == 1 && shift_amount == 0)
            shift_amount = 31;

        if (shift_type == 0)
            rn_val <<= shift_amount;
        else if (shift_type == 1)
            rn_val = ((s32)rn_val >> shift_amount);

        bool saturated = false;
        rn_val = ARMul_UnsignedSatQ(rn_val, sat_imm, &saturated);

        if (saturated)
            cpsr |= (1 << 27);

        RD = rn_val;
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
USAT16_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {

        const u8 saturate_to = sat_imm;

        bool sat1 = false;
        bool sat2 = false;

        RD = (ARMul_UnsignedSatQ((s16)RN, saturate_to, &sat1) & 0xFFFF) |
             ARMul_UnsignedSatQ((s32)RN >> 16, saturate_to, &sat2) << 16;

        if (sat1 || sat2)
            cpsr |= (1 << 27);
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
UXTAB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        uxtab_inst* inst_cream = (uxtab_inst*)component;

        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * ((inst >> 8) & 0xFu)) & 0xff;
        RD = RN + operand2;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
UXTAB16_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        const u8 rn_idx = ((inst >> 16) & 0xFu);
        const u32 rm_val = RM;
        const u32 rotation = ((inst >> 8) & 0xFu) * 8;
        const u32 rotated_rm = ((rm_val << (32 - rotation)) | (rm_val >> rotation));

        // UXTB16, otherwise UXTAB16
        if (rn_idx == 15) {
            RD = rotated_rm & 0x00FF00FF;
        } else {
            const u32 rn_val = RN;
            const u8 lo_rotated = (rotated_rm & 0xFF);
            const u16 lo_result = (rn_val & 0xFFFF) + (u16)lo_rotated;
            const u8 hi_rotated = (rotated_rm >> 16) & 0xFF;
            const u16 hi_result = (rn_val >> 16) + (u16)hi_rotated;

            RD = ((hi_result << 16) | (lo_result & 0xFFFF));
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
UXTAH_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        uxtah_inst* inst_cream = (uxtah_inst*)component;
        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * ((inst >> 8) & 0xFu)) & 0xffff;

        RD = RN + operand2;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
UXTB_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        uxtb_inst* inst_cream = (uxtb_inst*)component;
        RD = ROTATE_RIGHT_32(RM, 8 * ((inst >> 8) & 0xFu)) & 0xff;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
UXTB16_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {


        const u8 rn_idx = ((inst >> 16) & 0xFu);
        const u32 rm_val = RM;
        const u32 rotation = ((inst >> 8) & 0xFu) * 8;
        const u32 rotated_rm = ((rm_val << (32 - rotation)) | (rm_val >> rotation));

        // UXTB16, otherwise UXTAB16
        if (rn_idx == 15) {
            RD = rotated_rm & 0x00FF00FF;
        } else {
            const u32 rn_val = RN;
            const u8 lo_rotated = (rotated_rm & 0xFF);
            const u16 lo_result = (rn_val & 0xFFFF) + (u16)lo_rotated;
            const u8 hi_rotated = (rotated_rm >> 16) & 0xFF;
            const u16 hi_result = (rn_val >> 16) + (u16)hi_rotated;

            RD = ((hi_result << 16) | (lo_result & 0xFFFF));
        }
    }

    gprs[15] += 4;


    return 1;
}
    goto END;
UXTH_INST:
{
    if (cond == 0xE || conditionPassed(cond)) {
        uxth_inst* inst_cream = (uxth_inst*)component;
        RD = ROTATE_RIGHT_32(RM, 8 * ((inst >> 8) & 0xFu)) & 0xffff;
    }
    gprs[15] += 4;


    return 1;
}
    goto END;
VABS_F32_INST:
    return executeVfp(inst);
VABS_F64_INST:
    return executeVfp(inst);
VADD_F32_INST:
    return executeVfp(inst);
VADD_F64_INST:
    return executeVfp(inst);
VCMP_F32_INST:
    return executeVfp(inst);
VCMP_F64_INST:
    return executeVfp(inst);
VCVT_F32_F64_INST:
    return executeVfp(inst);
VCVT_F64_F32_INST:
    return executeVfp(inst);
VCVT_S32_F32_INST:
    return executeVfp(inst);
VCVT_U32_F32_INST:
    return executeVfp(inst);
VDIV_F32_INST:
    return executeVfp(inst);
VDIV_F64_INST:
    return executeVfp(inst);
VLDM_INST:
    return executeVfp(inst);
VLDR_INST:
    return executeVfp(inst);
VMLA_F32_INST:
    return executeVfp(inst);
VMLA_F64_INST:
    return executeVfp(inst);
VMLS_F32_INST:
    return executeVfp(inst);
VMLS_F64_INST:
    return executeVfp(inst);
VMOV_I_INST:
    return executeVfp(inst);
VMOV_R_INST:
    return executeVfp(inst);
VNEG_F32_INST:
    return executeVfp(inst);
VNEG_F64_INST:
    return executeVfp(inst);
VNMLA_F32_INST:
    return executeVfp(inst);
VNMLA_F64_INST:
    return executeVfp(inst);
VNMLS_F32_INST:
    return executeVfp(inst);
VNMLS_F64_INST:
    return executeVfp(inst);
VSQRT_F32_INST:
    return executeVfp(inst);
VSQRT_F64_INST:
    return executeVfp(inst);
VSTM_INST:
    return executeVfp(inst);
VSTR_INST:
    return executeVfp(inst);
WFE_INST:
{
    // Stubbed, as WFE is a hint instruction.
    if (cond == 0xE || conditionPassed(cond)) {
        LOG_TRACE(Core_ARM11, "WFE executed.");
    }

    gprs[15] += 4;
    INC_PC_STUB;

    return 1;
}
    goto END;
WFI_INST:
{
    // Stubbed, as WFI is a hint instruction.
    if (cond == 0xE || conditionPassed(cond)) {
        LOG_TRACE(Core_ARM11, "WFI executed.");
    }

    gprs[15] += 4;
    INC_PC_STUB;

    return 1;
}
    goto END;
YIELD_INST:
{
    // Stubbed, as YIELD is a hint instruction.
    if (cond == 0xE || conditionPassed(cond)) {
        LOG_TRACE(Core_ARM11, "YIELD executed.");
    }

    gprs[15] += 4;
    INC_PC_STUB;

    return 1;
}
    goto END;
SEV_INST:
{
    // Stubbed, as SEV is a hint instruction.
    if (cond == 0xE || conditionPassed(cond)) {
        LOG_TRACE(Core_ARM11, "SEV executed.");
    }

    gprs[15] += 4;
    INC_PC_STUB;

    return 1;
}
    goto END;
CLREX_INST:
{
    UnsetExclusiveMemoryAddress();
    gprs[15] += 4;


    return 1;
}
    goto END;
SVC_INST:
    { const u32 svc = inst & 0x00FFFFFFu; gprs[PC_INDEX] = old_pc + 4; kernel.serviceSVC(svc); return 4; }
RFE_INST:
{
    // RFE is unconditional


    u32 address = 0;
    get_addr(cpu, inst, address);

    cpsr = mem.read32(address);
    gprs[15] = mem.read32(address + 4);


    return 3;
}
    goto END;
SRS_INST:
{
    // SRS is unconditional


    u32 address = 0;
    get_addr(cpu, inst, address);

    mem.write32(address + 0, gprs[14]);
    mem.write32(address + 4, cpsr);

    gprs[15] += 4;


    return 1;
}
    goto END;
NOP_INST:
{
    gprs[15] += 4;
    INC_PC_STUB;

    return 1;
}
    goto END;

END:
    gprs[PC_INDEX] = old_pc + 4;
    return 1;
}