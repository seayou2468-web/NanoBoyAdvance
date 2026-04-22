
u32 CPU::getAddr(u32 inst, u32 old_pc) {
    const bool immediate = (inst & (1u << 25)) == 0;
    const bool pre = (inst & (1u << 24)) != 0;
    const bool up = (inst & (1u << 23)) != 0;
    const bool write_back = (inst & (1u << 21)) != 0;
    const u32 rn_idx = (inst >> 16) & 0xF;
    const u32 base = (rn_idx == 15) ? ((old_pc + 8) & ~3u) : gprs[rn_idx];

    u32 offset = 0;
    if (immediate) {
        offset = inst & 0xFFFu;
    } else {
        const u32 rm_idx = inst & 0xF;
        const u32 rm = (rm_idx == 15) ? (old_pc + 8) : gprs[rm_idx];
        const u32 shift_type = (inst >> 5) & 0x3;
        const u32 shift_imm = (inst >> 7) & 0x1F;
        switch (shift_type) {
            case 0: offset = rm << shift_imm; break;
            case 1: offset = (shift_imm == 0) ? 0 : (rm >> shift_imm); break;
            case 2: offset = (shift_imm == 0) ? (((rm >> 31) & 1) ? 0xFFFFFFFF : 0) : static_cast<u32>(static_cast<s32>(rm) >> shift_imm); break;
            case 3: offset = (shift_imm == 0) ? (((cpsr & CPSR::Carry) ? 1u : 0u) << 31) | (rm >> 1) : ror32(rm, shift_imm); break;
        }
    }

    u32 effective = pre ? (up ? (base + offset) : (base - offset)) : base;
    if (write_back || !pre) {
        gprs[rn_idx] = up ? (base + offset) : (base - offset);
    }
    return effective;
}
