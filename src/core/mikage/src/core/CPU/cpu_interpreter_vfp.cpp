#include "../../../include/cpu_interpreter.hpp"
#include "./cpu_interpreter_internal.hpp"
#include "../../../include/logger.hpp"
#include <cfenv>
#include <cmath>

u32 CPU::executeVfp(u32 inst) {
    const u32 old_pc = gprs[PC_INDEX];
    const u32 cond = inst >> 28;
    if (cond != 0xE && !conditionPassed(cond)) return 1;

    const InstructionDecoding* decoding = DecodeArmInstruction(inst);
    if (!decoding) return 0;
    std::string_view name(decoding->name);


    gprs[PC_INDEX] = old_pc + 4;
    return 1;
}

u32 CPU::executeNeon(u32 inst) {
    gprs[PC_INDEX] = gprs[PC_INDEX] + 4;
    return 1;
}
