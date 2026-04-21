# Reference 命令実装テンプレート集
## cpu_interpreter.cpp 統合用

---

## Part 1: データ処理命令テンプレート

### テンプレート 1.1: 基本的なデータ処理命令

```cpp
// テンプレート: 汎用データ処理命令 (AND, ORR, EOR, BIC, etc.)
class GenericDataProcessingHandler {
public:
    static void Execute(ARM_Interpreter* cpu, const Instruction* inst) {
        // 条件チェック
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        // オペランド取得
        u32 rn_val = cpu->GetRegister(inst->Rn);
        if (inst->Rn == 15)  // PC の場合は調整
            rn_val += 2 * cpu->GetInstructionSize();
        
        u32 shifter_operand = GetShifterOperand(cpu, inst);
        u32 shifter_carry = cpu->shifter_carry_out;
        
        // 命令実行
        u32 result = PerformOperation(rn_val, shifter_operand, inst->opcode);
        
        // 結果書き込み
        cpu->SetRegister(inst->Rd, result);
        
        // フラグ更新
        if (inst->S) {
            UpdateFlags(cpu, result, shifter_carry, inst->opcode);
        }
        
        // PC 更新
        if (inst->Rd == 15) {
            cpu->DispatchNext();
        } else {
            AdvancePC(cpu);
        }
    }
    
private:
    static u32 PerformOperation(u32 rn, u32 operand, int opcode) {
        switch (opcode) {
        case 0x0: return rn & operand;      // AND
        case 0x1: return rn ^ operand;      // EOR
        case 0x2: return rn - operand;      // SUB (実装略)
        case 0x3: return operand - rn;      // RSB (実装略)
        case 0x4: return rn + operand;      // ADD (実装略)
        case 0x5: return rn + operand + 0;  // ADC (実装略)
        case 0x6: return rn - operand - 0;  // SBC (実装略)
        case 0x7: return operand - rn - 0;  // RSC (実装略)
        case 0xC: return rn | operand;      // ORR
        case 0xD: return operand;           // MOV
        case 0xE: return rn & ~operand;     // BIC
        case 0xF: return ~operand;          // MVN
        }
        return 0;
    }
    
    static void UpdateFlags(ARM_Interpreter* cpu, u32 result, u32 carry, int opcode) {
        cpu->NFlag = (result >> 31) & 1;
        cpu->ZFlag = (result == 0) ? 1 : 0;
        cpu->CFlag = carry;
        // V フラグは命令依存（SUB, ADD, ADC, SBC, RSB, RSC のみ）
    }
};
```

### テンプレート 1.2: 加減算命令（キャリー付き）

```cpp
class ArithmeticHandler {
public:
    static u32 AddWithCarry(u32 x, u32 y, u32 c_in, bool& c_out, bool& v_out) {
        u64 result = (u64)x + (u64)y + c_in;
        c_out = (result >> 32) & 1;
        
        u32 result_32 = result & 0xffffffff;
        bool x_sign = (x >> 31) & 1;
        bool y_sign = (y >> 31) & 1;
        bool r_sign = (result_32 >> 31) & 1;
        
        v_out = (x_sign == y_sign) && (x_sign != r_sign);
        return result_32;
    }
    
    static void ExecuteADD(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rn = GetRN(cpu, inst);
        u32 operand = GetShifterOperand(cpu, inst);
        
        bool carry, overflow;
        u32 result = AddWithCarry(rn, operand, 0, carry, overflow);
        
        cpu->SetRegister(inst->Rd, result);
        
        if (inst->S) {
            cpu->NFlag = (result >> 31) & 1;
            cpu->ZFlag = (result == 0) ? 1 : 0;
            cpu->CFlag = carry;
            cpu->VFlag = overflow;
        }
        
        if (inst->Rd == 15) cpu->DispatchNext();
        else AdvancePC(cpu);
    }
    
    static void ExecuteSUB(ARM_Interpreter* cpu, const Instruction* inst) {
        // SUB Rd, Rn, operand = ADD Rd, Rn, -operand
        // 実装: AddWithCarry(Rn, ~operand, 1, carry, overflow)
        // これは: Rn + (~operand) + 1 = Rn - operand
        
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rn = GetRN(cpu, inst);
        u32 operand = GetShifterOperand(cpu, inst);
        
        bool carry, overflow;
        u32 result = AddWithCarry(rn, ~operand, 1, carry, overflow);
        
        cpu->SetRegister(inst->Rd, result);
        
        if (inst->S) {
            cpu->NFlag = (result >> 31) & 1;
            cpu->ZFlag = (result == 0) ? 1 : 0;
            cpu->CFlag = carry;
            cpu->VFlag = overflow;
        }
        
        if (inst->Rd == 15) cpu->DispatchNext();
        else AdvancePC(cpu);
    }
};
```

### テンプレート 1.3: テスト命令（フラグのみ更新）

```cpp
class ComparisonHandler {
public:
    static void ExecuteCMP(ARM_Interpreter* cpu, const Instruction* inst) {
        // CMP は Rn - operand のフラグのみ更新
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rn = GetRN(cpu, inst);
        u32 operand = GetShifterOperand(cpu, inst);
        
        bool carry, overflow;
        u32 result = AddWithCarry(rn, ~operand, 1, carry, overflow);
        
        cpu->NFlag = (result >> 31) & 1;
        cpu->ZFlag = (result == 0) ? 1 : 0;
        cpu->CFlag = carry;
        cpu->VFlag = overflow;
        
        AdvancePC(cpu);
    }
    
    static void ExecuteTST(ARM_Interpreter* cpu, const Instruction* inst) {
        // TST は Rn & operand のフラグのみ更新
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rn = GetRN(cpu, inst);
        u32 operand = GetShifterOperand(cpu, inst);
        u32 result = rn & operand;
        
        cpu->NFlag = (result >> 31) & 1;
        cpu->ZFlag = (result == 0) ? 1 : 0;
        cpu->CFlag = cpu->shifter_carry_out;
        // V フラグは更新なし
        
        AdvancePC(cpu);
    }
};
```

---

## Part 2: メモリ転送命令テンプレート

### テンプレート 2.1: 単一レジスタロード/ストア

```cpp
class SingleMemoryTransferHandler {
public:
    static void ExecuteLDR(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 address = CalculateAddress(cpu, inst);
        u32 value = cpu->ReadMemory32(address);
        
        if (inst->Rd == 15) {
            // Thumb ビット処理
            cpu->TFlag = value & 0x1;
            cpu->SetRegister(15, value & 0xFFFFFFFE);
            cpu->DispatchNext();
        } else {
            cpu->SetRegister(inst->Rd, value);
            AdvancePC(cpu);
        }
    }
    
    static void ExecuteSTR(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 address = CalculateAddress(cpu, inst);
        u32 value = cpu->GetRegister(inst->Rd);
        
        cpu->WriteMemory32(address, value);
        AdvancePC(cpu);
    }
    
    static void ExecuteLDRB(ARM_Interpreter* cpu, const Instruction* inst) {
        // LDRB: ロードして符号拡張なし
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 address = CalculateAddress(cpu, inst);
        u32 value = cpu->ReadMemory8(address);
        
        cpu->SetRegister(inst->Rd, value);  // 符号拡張なし
        AdvancePC(cpu);
    }
    
    static void ExecuteLDRSB(ARM_Interpreter* cpu, const Instruction* inst) {
        // LDRSB: ロードして符号拡張
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 address = CalculateAddress(cpu, inst);
        s32 value = (s8)cpu->ReadMemory8(address);  // 符号拡張
        
        cpu->SetRegister(inst->Rd, value);
        AdvancePC(cpu);
    }
    
    static void ExecuteLDRH(ARM_Interpreter* cpu, const Instruction* inst) {
        // LDRH: ハーフワード、符号拡張なし
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 address = CalculateAddress(cpu, inst);
        u32 value = cpu->ReadMemory16(address);
        
        cpu->SetRegister(inst->Rd, value);
        AdvancePC(cpu);
    }
    
    static void ExecuteLDRSH(ARM_Interpreter* cpu, const Instruction* inst) {
        // LDRSH: ハーフワード、符号拡張
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 address = CalculateAddress(cpu, inst);
        s32 value = (s16)cpu->ReadMemory16(address);  // 符号拡張
        
        cpu->SetRegister(inst->Rd, value);
        AdvancePC(cpu);
    }
    
private:
    static u32 CalculateAddress(ARM_Interpreter* cpu, const Instruction* inst) {
        // アドレッシング方式に応じた計算
        // イミディエートオフセット、レジスタオフセット、
        // プリインデックス、ポストインデックスなど
        // (詳細は命令形式に依存)
        return cpu->GetRegister(inst->Rn) + inst->offset;
    }
};
```

### テンプレート 2.2: 複数レジスタ転送

```cpp
class MultipleMemoryTransferHandler {
public:
    static void ExecuteLDM(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 address = cpu->GetRegister(inst->Rn);
        
        // アドレッシングモード別に事前調整
        // (IncAfter, IncBefore, DecAfter, DecBefore)
        
        bool branch = false;
        for (int i = 0; i < 16; i++) {
            if (inst->register_list & (1 << i)) {
                u32 value = cpu->ReadMemory32(address);
                
                // R15 の場合はThumbビット処理
                if (i == 15) {
                    cpu->TFlag = value & 0x1;
                    value &= 0xFFFFFFFE;
                    branch = true;
                }
                
                cpu->SetRegister(i, value);
                address += 4;
            }
        }
        
        // ベースレジスタ更新（Wビット）
        if (inst->W) {
            cpu->SetRegister(inst->Rn, address);
        }
        
        if (branch && (inst->register_list & (1 << 15))) {
            cpu->DispatchNext();
        } else {
            AdvancePC(cpu);
        }
    }
    
    static void ExecuteSTM(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 address = cpu->GetRegister(inst->Rn);
        
        for (int i = 0; i < 16; i++) {
            if (inst->register_list & (1 << i)) {
                u32 value = cpu->GetRegister(i);
                cpu->WriteMemory32(address, value);
                address += 4;
            }
        }
        
        if (inst->W) {
            cpu->SetRegister(inst->Rn, address);
        }
        
        AdvancePC(cpu);
    }
};
```

---

## Part 3: 乗算命令テンプレート

### テンプレート 3.1: 32ビット乗算

```cpp
class MultiplyHandler {
public:
    static void ExecuteMUL(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u64 rm = (u64)cpu->GetRegister(inst->Rm);
        u64 rs = (u64)cpu->GetRegister(inst->Rs);
        
        u32 result = (u32)((rm * rs) & 0xffffffff);
        cpu->SetRegister(inst->Rd, result);
        
        if (inst->S) {
            cpu->NFlag = (result >> 31) & 1;
            cpu->ZFlag = (result == 0) ? 1 : 0;
            // C フラグと V フラグは UNPREDICTABLE
        }
        
        AdvancePC(cpu);
    }
    
    static void ExecuteMLA(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u64 rm = (u64)cpu->GetRegister(inst->Rm);
        u64 rs = (u64)cpu->GetRegister(inst->Rs);
        u64 rn = (u64)cpu->GetRegister(inst->Rn);
        
        u32 result = (u32)((rm * rs + rn) & 0xffffffff);
        cpu->SetRegister(inst->Rd, result);
        
        if (inst->S) {
            cpu->NFlag = (result >> 31) & 1;
            cpu->ZFlag = (result == 0) ? 1 : 0;
        }
        
        AdvancePC(cpu);
    }
};
```

### テンプレート 3.2: 64ビット乗算

```cpp
class LongMultiplyHandler {
public:
    static void ExecuteUMULL(ARM_Interpreter* cpu, const Instruction* inst) {
        // 符号なし64ビット乗算
        // RD:RN = RM * RS
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u64 rm = (u64)cpu->GetRegister(inst->Rm);
        u64 rs = (u64)cpu->GetRegister(inst->Rs);
        u64 result = rm * rs;
        
        cpu->SetRegister(inst->Rd, (u32)(result & 0xffffffff));
        cpu->SetRegister(inst->Rn, (u32)((result >> 32) & 0xffffffff));
        
        if (inst->S) {
            cpu->NFlag = (result >> 63) & 1;
            cpu->ZFlag = (result == 0) ? 1 : 0;
            // C フラグと V フラグは UNPREDICTABLE
        }
        
        AdvancePC(cpu);
    }
    
    static void ExecuteSMULL(ARM_Interpreter* cpu, const Instruction* inst) {
        // 符号付き64ビット乗算
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        s64 rm = (s64)(s32)cpu->GetRegister(inst->Rm);
        s64 rs = (s64)(s32)cpu->GetRegister(inst->Rs);
        s64 result = rm * rs;
        
        cpu->SetRegister(inst->Rd, (u32)(result & 0xffffffff));
        cpu->SetRegister(inst->Rn, (u32)((result >> 32) & 0xffffffff));
        
        if (inst->S) {
            cpu->NFlag = (result >> 63) & 1;
            cpu->ZFlag = (result == 0) ? 1 : 0;
        }
        
        AdvancePC(cpu);
    }
    
    static void ExecuteUMLAL(ARM_Interpreter* cpu, const Instruction* inst) {
        // 符号なし64ビット乗算加算
        // RD:RN += RM * RS
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u64 rm = (u64)cpu->GetRegister(inst->Rm);
        u64 rs = (u64)cpu->GetRegister(inst->Rs);
        u64 rn = ((u64)cpu->GetRegister(inst->Rn) << 32) | 
                 (u64)cpu->GetRegister(inst->Rd);
        
        u64 result = (rm * rs) + rn;
        
        cpu->SetRegister(inst->Rd, (u32)(result & 0xffffffff));
        cpu->SetRegister(inst->Rn, (u32)((result >> 32) & 0xffffffff));
        
        if (inst->S) {
            cpu->NFlag = (result >> 63) & 1;
            cpu->ZFlag = (result == 0) ? 1 : 0;
        }
        
        AdvancePC(cpu);
    }
    
    static void ExecuteSMLAL(ARM_Interpreter* cpu, const Instruction* inst) {
        // 符号付き64ビット乗算加算
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        s64 rm = (s64)(s32)cpu->GetRegister(inst->Rm);
        s64 rs = (s64)(s32)cpu->GetRegister(inst->Rs);
        s64 rn = ((s64)(s32)cpu->GetRegister(inst->Rn) << 32) | 
                 (u64)cpu->GetRegister(inst->Rd);
        
        s64 result = (rm * rs) + rn;
        
        cpu->SetRegister(inst->Rd, (u32)(result & 0xffffffff));
        cpu->SetRegister(inst->Rn, (u32)((result >> 32) & 0xffffffff));
        
        if (inst->S) {
            cpu->NFlag = (result >> 63) & 1;
            cpu->ZFlag = (result == 0) ? 1 : 0;
        }
        
        AdvancePC(cpu);
    }
};
```

---

## Part 4: 分岐命令テンプレート

### テンプレート 4.1: 条件付き分岐

```cpp
class BranchHandler {
public:
    static void ExecuteB(ARM_Interpreter* cpu, const Instruction* inst) {
        // B or BL
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        if (inst->L) {  // Link ビット
            cpu->SetRegister(14, cpu->GetRegister(15) + cpu->GetInstructionSize());
        }
        
        // オフセット計算
        s32 offset = (s32)inst->signed_imm_24;
        offset = (offset << 2);  // 4倍に拡大
        
        u32 new_pc = cpu->GetRegister(15) + 8 + offset;
        cpu->SetRegister(15, new_pc);
        
        cpu->DispatchNext();
    }
    
    static void ExecuteBX(ARM_Interpreter* cpu, const Instruction* inst) {
        // BX or BXJ
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 address = cpu->GetRegister(inst->Rm);
        cpu->SetRegister(15, address & 0xfffffffe);
        cpu->TFlag = address & 0x1;  // ビット0 = Thumbモード
        
        cpu->DispatchNext();
    }
    
    static void ExecuteBLX(ARM_Interpreter* cpu, const Instruction* inst) {
        // BLX: 2種類の形式
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        if (inst->IsRegisterForm()) {
            // BLX Rm
            u32 address = cpu->GetRegister(inst->Rm);
            cpu->SetRegister(14, cpu->GetRegister(15) + cpu->GetInstructionSize());
            cpu->SetRegister(15, address & 0xfffffffe);
            cpu->TFlag = address & 0x1;
        } else {
            // BLX imm24
            cpu->SetRegister(14, cpu->GetRegister(15) + cpu->GetInstructionSize());
            cpu->TFlag = 0x1;  // Thumb に切り替え
            
            s32 offset = (s32)inst->signed_imm_24;
            offset = (offset << 2) | (inst->H << 1);  // H ビット
            
            u32 new_pc = cpu->GetRegister(15) + 8 + offset;
            cpu->SetRegister(15, new_pc & 0xfffffffe);
        }
        
        cpu->DispatchNext();
    }
};
```

---

## Part 5: 飽和演算命令テンプレート

### テンプレート 5.1: スカラー飽和演算

```cpp
class SaturatingArithmeticHandler {
public:
    static s32 SignedSaturatedAdd32(s32 a, s32 b) {
        s64 result = (s64)a + (s64)b;
        if (result > 0x7FFFFFFF)
            return 0x7FFFFFFF;
        if (result < -0x80000000)
            return -0x80000000;
        return (s32)result;
    }
    
    static s16 SignedSaturatedAdd16(s16 a, s16 b) {
        s32 result = (s32)a + (s32)b;
        if (result > 0x7FFF)
            return 0x7FFF;
        if (result < -0x8000)
            return -0x8000;
        return (s16)result;
    }
    
    static s8 SignedSaturatedAdd8(s8 a, s8 b) {
        s32 result = (s32)a + (s32)b;
        if (result > 0x7F)
            return 0x7F;
        if (result < -0x80)
            return -0x80;
        return (s8)result;
    }
    
    static void ExecuteQADD(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        s32 rn = (s32)cpu->GetRegister(inst->Rn);
        s32 rm = (s32)cpu->GetRegister(inst->Rm);
        
        s32 result = SignedSaturatedAdd32(rn, rm);
        cpu->SetRegister(inst->Rd, result);
        
        // Q フラグ（飽和発生）
        s64 actual = (s64)rn + (s64)rm;
        if (actual > 0x7FFFFFFF || actual < -0x80000000) {
            cpu->SetQFlag(1);
        }
        
        AdvancePC(cpu);
    }
    
    static void ExecuteQSUB(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        s32 rn = (s32)cpu->GetRegister(inst->Rn);
        s32 rm = (s32)cpu->GetRegister(inst->Rm);
        
        s32 result = SignedSaturatedAdd32(rn, -rm);
        cpu->SetRegister(inst->Rd, result);
        
        AdvancePC(cpu);
    }
};
```

### テンプレート 5.2: SIMD 飽和演算

```cpp
class SIMDSaturatingHandler {
public:
    static void ExecuteQADD16(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        s16 rn_lo = (s16)(cpu->GetRegister(inst->Rn) & 0xFFFF);
        s16 rn_hi = (s16)((cpu->GetRegister(inst->Rn) >> 16) & 0xFFFF);
        s16 rm_lo = (s16)(cpu->GetRegister(inst->Rm) & 0xFFFF);
        s16 rm_hi = (s16)((cpu->GetRegister(inst->Rm) >> 16) & 0xFFFF);
        
        s16 lo_result = SignedSaturatedAdd16(rn_lo, rm_lo);
        s16 hi_result = SignedSaturatedAdd16(rn_hi, rm_hi);
        
        u32 result = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);
        cpu->SetRegister(inst->Rd, result);
        
        AdvancePC(cpu);
    }
    
    static void ExecuteQADD8(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rn = cpu->GetRegister(inst->Rn);
        u32 rm = cpu->GetRegister(inst->Rm);
        u32 result = 0;
        
        for (int i = 0; i < 4; i++) {
            s8 rn_byte = (s8)((rn >> (8*i)) & 0xFF);
            s8 rm_byte = (s8)((rm >> (8*i)) & 0xFF);
            
            s8 res_byte = SignedSaturatedAdd8(rn_byte, rm_byte);
            result |= ((u32)res_byte & 0xFF) << (8*i);
        }
        
        cpu->SetRegister(inst->Rd, result);
        AdvancePC(cpu);
    }
};
```

---

## Part 6: DSP/SIMD 命令テンプレート

### テンプレート 6.1: SIMD 加減算

```cpp
class DSPAddSubHandler {
public:
    static void ExecuteSADD16(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        s16 rn_lo = (s16)(cpu->GetRegister(inst->Rn) & 0xFFFF);
        s16 rn_hi = (s16)((cpu->GetRegister(inst->Rn) >> 16) & 0xFFFF);
        s16 rm_lo = (s16)(cpu->GetRegister(inst->Rm) & 0xFFFF);
        s16 rm_hi = (s16)((cpu->GetRegister(inst->Rm) >> 16) & 0xFFFF);
        
        s32 lo_result = (s32)rn_lo + (s32)rm_lo;
        s32 hi_result = (s32)rn_hi + (s32)rm_hi;
        
        u32 result = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);
        cpu->SetRegister(inst->Rd, result);
        
        // GE フラグ更新（GreaterOrEqual, Bits 16-19）
        if (lo_result >= 0)
            cpu->Cpsr |= (0x3 << 16);  // Bits 16-17
        else
            cpu->Cpsr &= ~(0x3 << 16);
        
        if (hi_result >= 0)
            cpu->Cpsr |= (0x3 << 18);  // Bits 18-19
        else
            cpu->Cpsr &= ~(0x3 << 18);
        
        AdvancePC(cpu);
    }
    
    static void ExecuteSADD8(ARM_Interpreter* cpu, const Instruction* inst) {
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rn = cpu->GetRegister(inst->Rn);
        u32 rm = cpu->GetRegister(inst->Rm);
        u32 result = 0;
        
        for (int i = 0; i < 4; i++) {
            s8 rn_byte = (s8)((rn >> (8*i)) & 0xFF);
            s8 rm_byte = (s8)((rm >> (8*i)) & 0xFF);
            
            s32 res = (s32)rn_byte + (s32)rm_byte;
            result |= ((res & 0xFF) << (8*i));
            
            // GE フラグ更新
            if (res >= 0)
                cpu->Cpsr |= (1 << (16 + i));
            else
                cpu->Cpsr &= ~(1 << (16 + i));
        }
        
        cpu->SetRegister(inst->Rd, result);
        AdvancePC(cpu);
    }
};
```

### テンプレート 6.2: SIMD 乗算加算

```cpp
class DSPMultiplyHandler {
public:
    static void ExecuteSMUAD(ARM_Interpreter* cpu, const Instruction* inst) {
        // SMUAD: (RN[15:0] * RM[15:0]) + (RN[31:16] * RM[31:16])
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        s16 rn_lo = (s16)(cpu->GetRegister(inst->Rn) & 0xFFFF);
        s16 rn_hi = (s16)((cpu->GetRegister(inst->Rn) >> 16) & 0xFFFF);
        s16 rm_lo = (s16)(cpu->GetRegister(inst->Rm) & 0xFFFF);
        s16 rm_hi = (s16)((cpu->GetRegister(inst->Rm) >> 16) & 0xFFFF);
        
        s32 product1 = (s32)rn_lo * (s32)rm_lo;
        s32 product2 = (s32)rn_hi * (s32)rm_hi;
        
        s64 sum = (s64)product1 + (s64)product2;
        if (inst->Ra != 15) {
            sum += (s64)(s32)cpu->GetRegister(inst->Ra);
        }
        
        u32 result = (u32)sum;
        cpu->SetRegister(inst->Rd, result);
        
        AdvancePC(cpu);
    }
    
    static void ExecuteSMLAD(ARM_Interpreter* cpu, const Instruction* inst) {
        // SMLAD: Rd = (RN[15:0] * RM[15:0]) + (RN[31:16] * RM[31:16]) + Ra
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        s16 rn_lo = (s16)(cpu->GetRegister(inst->Rn) & 0xFFFF);
        s16 rn_hi = (s16)((cpu->GetRegister(inst->Rn) >> 16) & 0xFFFF);
        s16 rm_lo = (s16)(cpu->GetRegister(inst->Rm) & 0xFFFF);
        s16 rm_hi = (s16)((cpu->GetRegister(inst->Rm) >> 16) & 0xFFFF);
        s32 ra = (s32)cpu->GetRegister(inst->Ra);
        
        s32 product1 = (s32)rn_lo * (s32)rm_lo;
        s32 product2 = (s32)rn_hi * (s32)rm_hi;
        
        s64 result = (s64)product1 + (s64)product2 + (s64)ra;
        
        cpu->SetRegister(inst->Rd, (u32)result);
        AdvancePC(cpu);
    }
};
```

---

## Part 7: レジスタ操作命令テンプレート

### テンプレート 7.1: 符号拡張

```cpp
class SignExtensionHandler {
public:
    static void ExecuteSXTB(ARM_Interpreter* cpu, const Instruction* inst) {
        // SXTB Rd, Rm
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rm = cpu->GetRegister(inst->Rm);
        u32 byte = (rm >> (8 * inst->rotate)) & 0xFF;
        
        s32 result = (s8)byte;  // 符号拡張
        cpu->SetRegister(inst->Rd, result);
        
        AdvancePC(cpu);
    }
    
    static void ExecuteSXTH(ARM_Interpreter* cpu, const Instruction* inst) {
        // SXTH Rd, Rm
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rm = cpu->GetRegister(inst->Rm);
        u32 half = (rm >> (8 * inst->rotate)) & 0xFFFF;
        
        s32 result = (s16)half;  // 符号拡張
        cpu->SetRegister(inst->Rd, result);
        
        AdvancePC(cpu);
    }
};
```

### テンプレート 7.2: ビット操作

```cpp
class BitOperationHandler {
public:
    static void ExecuteCLZ(ARM_Interpreter* cpu, const Instruction* inst) {
        // CLZ: Leading Zero Count
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rm = cpu->GetRegister(inst->Rm);
        int count = 0;
        
        for (int i = 31; i >= 0; i--) {
            if ((rm >> i) & 1)
                break;
            count++;
        }
        
        cpu->SetRegister(inst->Rd, count);
        AdvancePC(cpu);
    }
    
    static void ExecuteREV(ARM_Interpreter* cpu, const Instruction* inst) {
        // REV: Byte Reverse
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rm = cpu->GetRegister(inst->Rm);
        u32 result = ((rm & 0xFF) << 24) |
                     (((rm >> 8) & 0xFF) << 16) |
                     (((rm >> 16) & 0xFF) << 8) |
                     ((rm >> 24) & 0xFF);
        
        cpu->SetRegister(inst->Rd, result);
        AdvancePC(cpu);
    }
    
    static void ExecuteREV16(ARM_Interpreter* cpu, const Instruction* inst) {
        // REV16: Byte Reverse in each 16-bit half
        if (!ConditionPassed(cpu, inst->cond)) {
            AdvancePC(cpu);
            return;
        }
        
        u32 rm = cpu->GetRegister(inst->Rm);
        u32 result = ((rm & 0xFF) << 8) | ((rm & 0xFF00) >> 8) |
                     ((rm & 0xFF0000) << 8) | ((rm & 0xFF000000) >> 8);
        
        cpu->SetRegister(inst->Rd, result);
        AdvancePC(cpu);
    }
};
```

---

## 実装チェックリスト

### ステップ1: 基本的な汎用レジスタ命令
- [ ] ADD, SUB, ADC, SBC, RSB, RSC
- [ ] AND, ORR, EOR, BIC
- [ ] MOV, MVN
- [ ] CMP, CMN, TST, TEQ

### ステップ2: メモリ転送
- [ ] LDR, STR (32-bit)
- [ ] LDRB, STRB (8-bit)
- [ ] LDRH, STRH (16-bit)
- [ ] LDRSB, LDRSH (符号拡張)
- [ ] LDM, STM (複数)

### ステップ3: 乗算
- [ ] MUL, MLA
- [ ] UMULL, UMLAL
- [ ] SMULL, SMLAL

### ステップ4: 分岐
- [ ] B, BL (条件付き分岐)
- [ ] BX, BXJ (レジスタ分岐)
- [ ] BLX (リンク付き分岐+切り替え)

### ステップ5: 拡張機能
- [ ] LDREX, STREX (排他的アクセス)
- [ ] SADD16, SADD8 (DSP加算)
- [ ] QADD, QADD16, QADD8 (飽和加算)
- [ ] CLZ, REV, REV16 (ビット操作)
- [ ] SXTB, SXTH (符号拡張)

---

**ノート**: これらのテンプレートは参考実装です。実装時には以下を確認してください：
1. 条件コードの正確な処理
2. フラグ更新の完全性
3. PC の正しい処理（+4 vs +8）
4. Thumb ビットの適切な処理
5. メモリアドレッシング方式
6. 特殊な場合（R15 使用時など）
