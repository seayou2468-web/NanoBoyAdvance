# Reference フォルダ ARM 命令実装抽出レポート

## 概要
Reference フォルダ（arm_dyncom_interpreter.cpp 4090行、arm_dyncom_thumb.cpp 343行）から ARM 命令の実装パターンを抽出しました。

---

## 1. Reference で実装されている命令（205個）

### ディスパッチャーテーブル分析結果

#### VFP/浮動小数点命令（32個）
```
0-5:    VMLA, VMLS, VNMLA, VNMLS, VNMUL, VMUL
6-8:    VADD, VSUB, VDIV
9-13:   VMOVI, VMOVR, VABS, VNEG, VSQRT
14-18:  VCMP, VCMP2, VCVTBDS, VCVTBFF, VCVTBFI
19-25:  VMOVBRS, VMSR, VMOVBRC, VMRS, VMOVBCR, VMOVBRRSS, VMOVBRRD
26-31:  VSTR, VPUSH, VSTM, VPOP, VLDR, VLDM
```

#### 特殊/システム命令（8個）
```
32-33:  SRS, RFE
34:     BKPT
35:     BLX
36:     CPS
37:     PLD
38:     SETEND
39:     CLREX
```

#### レジスタ操作命令（12個）
```
40-41:  REV16, USAD8
42-47:  SXTB, UXTB, SXTH, SXTB16, UXTH, UXTB16
48-49:  CPY, UXTAB
```

#### DSP/SIMD命令（50-89）
```
50:     SSUB8
51:     SHSUB8
52:     SSUBADDX
53-56:  STREX, STREXB, SWP, SWPB
57:     SSUB16
58:     SHSUB16
59:     SHSUBADDX
60:     SADD8
61:     SHADD8
62:     SADDSUBX
63:     SHADD16
64:     SADDSUBX (variant)
65:     SADD16
66:     SADDSUBX (variant)
67:     USUBADDX
68:     UHSUB8
69:     UHSUB16
70:     UHSUBADDX
71:     UHADD8
72:     UHADD16
73:     UADDSUBX
74:     UADD8
75:     UADD16
76:     SXTAH
77:     SXTAB16
78:     QADD8
79:     BXJ
80:     CLZ
81:     UXTAH
82:     BX
83:     REV
84:     BLX
85:     REVSH
86-89:  QADD, QADD16, QADDSUBX, LDREX
```

#### 乗算・飽和命令（90-111）
```
90:     QDADD
91:     QDSUB
92:     QSUB
93:     LDREXB
94-99:  QSUB8, QSUB16, (6個)
100-111: SMUAD, SMMUL, SMUSD, SMLSD, SMLSLD, SMMLA, SMMLS, SMLALD, SMLAD, SMLAW, SMULW, PKHTB, PKHBT
```

#### 乗算・分岐・テスト命令（112-143）
```
112-115: SMUL, SMLALXY, SMLA, MCRR
116:     MRRC
117-120: CMP, TST, TEQ, CMN
121-127: SMULL, UMULL, UMLAL, SMLAL, MUL, MLA, SSAT
128-129: USAT, MRS
130-143: MSR, AND, BIC, LDM, EOR, ADD, RSB, RSC, SBC, ADC, SUB, ORR, MVN, MOV
```

#### メモリ転送命令（144-189）
```
144-148: STM, LDM, LDRSH, STM, LDM
149-151: LDRSB, STRD, LDRH
152-156: STRH, LDRD, STRT, STRBT, LDRBT
157-160: LDRT, MRC, MCR, MSR (複数)
161-164: LDRB, STRB, LDR, LDRCOND
165-170: STR, CDP, STC, LDC, LDREXD, STREXD
171-172: LDREXH, STREXH
```

#### 制御フロー命令（173-190）
```
173:     NOP
174:     YIELD
175:     WFE
176:     WFI
177:     SEV
178:     SWI
179:     BBL
180-182: B_2_THUMB, B_COND_THUMB, BL_1_THUMB
183-186: BL_2_THUMB, BLX_1_THUMB, DISPATCH, INIT_INST_LENGTH
187:     END
```

---

## 2. 実装パターン分析

### 2.1 データ処理命令のパターン

#### 条件実行と基本構造
```c
// 汎用パターン
XXX_INST: {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        xxx_inst* inst_cream = (xxx_inst*)inst_base->component;
        
        // Rd = <operation>
        RD = <result>;
        
        // フラグ更新（S ビット設定時）
        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            UPDATE_CFLAG_WITH_SC;  // シフター出力からキャリーを取得
        }
        
        // PC=R15 の場合は分岐
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(xxx_inst));
            goto DISPATCH;
        }
    }
    
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(xxx_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
```

#### 重要な命令実装例

**ADD（加算）：**
```c
ADD_INST: {
    if (CondPassed(cpu, inst_base->cond)) {
        u32 rn_val = CHECK_READ_REG15_WA(cpu, inst_cream->Rn);
        bool carry, overflow;
        RD = AddWithCarry(rn_val, SHIFTER_OPERAND, 0, &carry, &overflow);
        
        if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            cpu->CFlag = carry;
            cpu->VFlag = overflow;
        }
    }
    // ... PC update and next instruction fetch
}
```

**SUB（減算）：**
```c
// SUB Rd, Rn, <operand2> は内部的に ADD Rd, Rn, #-operand2
SUB = AddWithCarry(RN, ~SHIFTER_OPERAND, 1, &carry, &overflow);
```

**RSB（逆減算）：**
```c
// RSB Rd, Rn, <operand2> は ADD Rd, -Rn, operand2
RSB = AddWithCarry(~RN, SHIFTER_OPERAND, 1, &carry, &overflow);
```

**ADC（キャリー付き加算）：**
```c
ADC = AddWithCarry(RN, SHIFTER_OPERAND, CFlag, &carry, &overflow);
```

**SBC（キャリー付き減算）：**
```c
SBC = AddWithCarry(RN, ~SHIFTER_OPERAND, CFlag, &carry, &overflow);
```

**AND（論理積）:**
```c
AND_INST: {
    u32 lop = RN;
    u32 rop = SHIFTER_OPERAND;
    RD = lop & rop;
    
    if (inst_cream->S) {
        UPDATE_NFLAG(RD);
        UPDATE_ZFLAG(RD);
        UPDATE_CFLAG_WITH_SC;  // シフター出力のキャリー
    }
}
```

**ORR（論理和）:**
```c
ORR = RN | SHIFTER_OPERAND;
```

**EOR（排他的論理和）:**
```c
EOR = RN ^ SHIFTER_OPERAND;
```

**BIC（ビッククリア）:**
```c
BIC = RN & (~SHIFTER_OPERAND);
```

**MVN（ビット反転）:**
```c
MVN = ~SHIFTER_OPERAND;
```

**MOV（データ転送）:**
```c
MOV = SHIFTER_OPERAND;
```

### 2.2 テスト命令のパターン

**CMP（比較）:**
```c
CMP_INST: {
    u32 result = AddWithCarry(RN, ~SHIFTER_OPERAND, 1, &carry, &overflow);
    UPDATE_NFLAG(result);
    UPDATE_ZFLAG(result);
    cpu->CFlag = carry;
    cpu->VFlag = overflow;
    // Rd には結果を書き込まない
}
```

**TST（ビットテスト）:**
```c
TST_INST: {
    u32 result = RN & SHIFTER_OPERAND;
    UPDATE_NFLAG(result);
    UPDATE_ZFLAG(result);
    cpu->CFlag = cpu->shifter_carry_out;
}
```

**CMN（比較ネゲート）:**
```c
CMN_INST: {
    u32 result = AddWithCarry(RN, SHIFTER_OPERAND, 0, &carry, &overflow);
    UPDATE_NFLAG(result);
    UPDATE_ZFLAG(result);
    cpu->CFlag = carry;
    cpu->VFlag = overflow;
}
```

**TEQ（テスト等価）:**
```c
TEQ_INST: {
    u32 result = RN ^ SHIFTER_OPERAND;
    UPDATE_NFLAG(result);
    UPDATE_ZFLAG(result);
    cpu->CFlag = cpu->shifter_carry_out;
}
```

### 2.3 乗算命令のパターン

**MUL（乗算）:**
```c
MUL_INST: {
    u64 rm = RM;
    u64 rs = RS;
    RD = (rm * rs) & 0xffffffff;
    
    if (inst_cream->S) {
        UPDATE_NFLAG(RD);
        UPDATE_ZFLAG(RD);
        // C フラグと V フラグは UNPREDICTABLE
    }
}
```

**MLA（乗算加算）:**
```c
MLA_INST: {
    u64 rm = RM;
    u64 rs = RS;
    u64 rn = RN;
    RD = (rm * rs + rn) & 0xffffffff;
    
    if (inst_cream->S) {
        UPDATE_NFLAG(RD);
        UPDATE_ZFLAG(RD);
    }
}
```

**UMULL（符号なし64ビット乗算）:**
```c
UMULL_INST: {
    u64 rm = RM;
    u64 rs = RS;
    u64 result = rm * rs;
    RD = (result >> 0) & 0xffffffff;   // RDlow
    RN = (result >> 32) & 0xffffffff;  // RDhigh
}
```

**SMULL（符号付き64ビット乗算）:**
```c
SMULL_INST: {
    s64 rm = (s32)RM;
    s64 rs = (s32)RS;
    s64 result = rm * rs;
    RD = (result >> 0) & 0xffffffff;
    RN = (result >> 32) & 0xffffffff;
}
```

**UMLAL（符号なし64ビット乗算加算）:**
```c
UMLAL_INST: {
    u64 rm = RM;
    u64 rs = RS;
    u64 rn = (((u64)RN << 32) | RD);  // 64ビット読み込み
    u64 result = (rm * rs) + rn;
    RD = result & 0xffffffff;
    RN = (result >> 32) & 0xffffffff;
}
```

**SMLAL（符号付き64ビット乗算加算）:**
```c
SMLAL_INST: {
    s64 rm = (s32)RM;
    s64 rs = (s32)RS;
    s64 rn = (((s64)(s32)RN << 32) | RD);
    s64 result = (rm * rs) + rn;
    RD = result & 0xffffffff;
    RN = (result >> 32) & 0xffffffff;
}
```

### 2.4 メモリ転送命令のパターン

**LDR（レジスタロード）:**
```c
LDR_INST: {
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;
    inst_cream->get_addr(cpu, inst_cream->inst, addr);
    
    u32 value = cpu->ReadMemory32(addr);
    cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
    
    // R15 の場合はThumbビットをセット
    if (Rd == 15) {
        cpu->TFlag = value & 0x1;
        cpu->Reg[15] &= 0xFFFFFFFE;
        goto DISPATCH;
    }
}
```

**STR（レジスタストア）:**
```c
STR_INST: {
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;
    inst_cream->get_addr(cpu, inst_cream->inst, addr);
    
    cpu->WriteMemory32(addr, cpu->Reg[Rd]);
}
```

**LDRB（バイトロード符号なし）:**
```c
LDRB_INST: {
    u32 value = cpu->ReadMemory8(addr);
    cpu->Reg[Rd] = value;  // 符号拡張なし
}
```

**LDRSB（バイトロード符号付き）:**
```c
LDRSB_INST: {
    u32 value = cpu->ReadMemory8(addr);
    if (BIT(value, 7))
        value |= 0xffffff00;  // 符号拡張
    cpu->Reg[Rd] = value;
}
```

**LDRH（ハーフワードロード符号なし）:**
```c
LDRH_INST: {
    u32 value = cpu->ReadMemory16(addr);
    cpu->Reg[Rd] = value;
}
```

**LDRSH（ハーフワードロード符号付き）:**
```c
LDRSH_INST: {
    u32 value = cpu->ReadMemory16(addr);
    if (BIT(value, 15))
        value |= 0xffff0000;  // 符号拡張
    cpu->Reg[Rd] = value;
}
```

**LDM（複数レジスタロード）:**
```c
LDM_INST: {
    u32 addr = RN;  // get_addr() で計算
    
    for (int i = 0; i < 16; i++) {
        if (BIT(inst, i)) {
            u32 value = cpu->ReadMemory32(addr);
            
            // R15 の場合はThumbビット処理
            if (i == 15) {
                cpu->TFlag = value & 0x1;
                value &= 0xFFFFFFFE;
            }
            
            cpu->Reg[i] = value;
            addr += 4;
        }
    }
    
    // R15 がリストに含まれている場合は分岐
    if (BIT(inst, 15)) {
        goto DISPATCH;
    }
}
```

**STM（複数レジスタストア）:**
```c
STM_INST: {
    u32 addr = RN;
    
    for (int i = 0; i < 16; i++) {
        if (BIT(inst, i)) {
            cpu->WriteMemory32(addr, cpu->Reg[i]);
            addr += 4;
        }
    }
}
```

**LDREX（排他的ロード）:**
```c
LDREX_INST: {
    u32 addr = cpu->Reg[Rn];
    u32 value = cpu->ReadMemory32(addr);
    
    cpu->SetExclusiveMemoryAddress(addr);  // 排他的アクセスマーク
    cpu->Reg[Rd] = value;
}
```

**STREX（排他的ストア）:**
```c
STREX_INST: {
    u32 write_addr = cpu->Reg[Rn];
    
    if (cpu->IsExclusiveMemoryAccess(write_addr)) {
        cpu->UnsetExclusiveMemoryAddress();
        cpu->WriteMemory32(write_addr, RM);
        RD = 0;  // 成功（0）
    } else {
        RD = 1;  // 失敗（1）
    }
}
```

### 2.5 分岐命令のパターン

**B（条件付き分岐）:**
```c
BBL_INST: {  // Branch, Branch with Link unified
    if (CondPassed(cpu, inst_base->cond)) {
        bbl_inst* inst_cream = (bbl_inst*)inst_base->component;
        
        if (inst_cream->L) {  // Link ビット
            cpu->Reg[14] = cpu->Reg[15] + cpu->GetInstructionSize();
        }
        
        // PC = PC + 8 + SignExtend(signed_imm_24 << 2)
        s32 offset = inst_cream->signed_imm_24;
        offset = (offset << 2);
        cpu->Reg[15] = cpu->Reg[15] + 8 + offset;
        
        goto DISPATCH;
    }
}
```

**BX（レジスタ分岐）:**
```c
BX_INST: {
    if (CondPassed(cpu, inst_base->cond)) {
        u32 jump_addr = RM;
        cpu->Reg[15] = jump_addr & 0xfffffffe;
        cpu->TFlag = jump_addr & 0x1;  // Thumbモード判定
        goto DISPATCH;
    }
}
```

**BLX（分岐＋リンク＋Thumb切り替え）:**
```c
BLX_INST: {
    // 2種類の形式: imm24 または Rm
    if (imm24_form) {
        cpu->Reg[14] = cpu->Reg[15] + GetInstructionSize();
        cpu->TFlag = 0x1;  // Thumb に切り替え
        u32 offset = (signed_imm_24 << 2) | (BIT(inst, 24) << 1);
        cpu->Reg[15] = (cpu->Reg[15] + 8 + offset) & 0xfffffffe;
    } else {
        // BLX Rm
        cpu->Reg[14] = cpu->Reg[15] + GetInstructionSize();
        cpu->Reg[15] = RM & 0xfffffffe;
        cpu->TFlag = RM & 0x1;
    }
    goto DISPATCH;
}
```

### 2.6 飽和演算命令のパターン

**QADD（飽和加算）:**
```c
QADD_INST: {
    s32 rn = (s32)RN;
    s32 rm = (s32)RM;
    s64 result = (s64)rn + (s64)rm;
    
    if (result > 0x7FFFFFFF) {
        RD = 0x7FFFFFFF;
        cpu->Cpsr |= (1 << 27);  // Q フラグセット
    } else if (result < -0x80000000) {
        RD = 0x80000000;
        cpu->Cpsr |= (1 << 27);
    } else {
        RD = result & 0xffffffff;
    }
}
```

**QADD8/QADD16（8ビット/16ビット飽和加算）:**
```c
// 16ビット単位の飽和加算
QADD16_INST: {
    s16 rn_lo = (s16)(RN & 0xFFFF);
    s16 rn_hi = (s16)((RN >> 16) & 0xFFFF);
    s16 rm_lo = (s16)(RM & 0xFFFF);
    s16 rm_hi = (s16)((RM >> 16) & 0xFFFF);
    
    s16 lo_result = ARMul_SignedSaturatedAdd16(rn_lo, rm_lo);
    s16 hi_result = ARMul_SignedSaturatedAdd16(rn_hi, rm_hi);
    
    RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);
}
```

### 2.7 DSP/SIMD命令のパターン

**SADD16（符号付き16ビット加算）:**
```c
SADD16_INST: {
    s16 rn_lo = RN & 0xFFFF;
    s16 rn_hi = (RN >> 16) & 0xFFFF;
    s16 rm_lo = RM & 0xFFFF;
    s16 rm_hi = (RM >> 16) & 0xFFFF;
    
    s32 lo_result = rn_lo + rm_lo;
    s32 hi_result = rn_hi + rm_hi;
    
    RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);
    
    // GE フラグ更新（Bits 16-19）
    if (lo_result >= 0)
        cpu->Cpsr |= (0x3 << 16);  // Bits 16-17
    else
        cpu->Cpsr &= ~(0x3 << 16);
    
    if (hi_result >= 0)
        cpu->Cpsr |= (0x3 << 18);  // Bits 18-19
    else
        cpu->Cpsr &= ~(0x3 << 18);
}
```

**SADD8（符号付き8ビット加算）:**
```c
SADD8_INST: {
    // 8ビット単位で4つの加算を実行
    for (int i = 0; i < 4; i++) {
        s32 byte_rn = (s32)(s8)((RN >> (8*i)) & 0xFF);
        s32 byte_rm = (s32)(s8)((RM >> (8*i)) & 0xFF);
        s32 result = byte_rn + byte_rm;
        
        RD |= (result & 0xFF) << (8*i);
        
        // GE フラグ更新
        if (result >= 0)
            cpu->Cpsr |= (1 << (16 + i));
        else
            cpu->Cpsr &= ~(1 << (16 + i));
    }
}
```

**SMUAD（SIMD乗算加算）:**
```c
SMUAD_INST: {
    s16 rn_lo = (s16)(RN & 0xFFFF);
    s16 rn_hi = (s16)((RN >> 16) & 0xFFFF);
    s16 rm_lo = (s16)(RM & 0xFFFF);
    s16 rm_hi = (s16)((RM >> 16) & 0xFFFF);
    
    s32 product1 = (s32)rn_lo * (s32)rm_lo;
    s32 product2 = (s32)rn_hi * (s32)rm_hi;
    
    s64 sum = (s64)product1 + (s64)product2 + (s64)RN;  // RN は累積値
    
    RD = (s32)sum;  // 下位32ビット
}
```

### 2.8 レジスタ操作/拡張命令のパターン

**SXTB（バイト符号拡張）:**
```c
SXTB_INST: {
    u8 byte = (RM >> (8 * rotate)) & 0xFF;
    if (BIT(byte, 7))
        RD = byte | 0xffffff00;  // 符号拡張
    else
        RD = byte;
}
```

**SXTH（ハーフワード符号拡張）:**
```c
SXTH_INST: {
    u16 half = (RM >> (8 * rotate)) & 0xFFFF;
    if (BIT(half, 15))
        RD = half | 0xffff0000;
    else
        RD = half;
}
```

**UXTB/UXTH（符号なし拡張）:**
```c
UXTB_INST: {
    RD = (RM >> (8 * rotate)) & 0xFF;  // 符号拡張なし
}

UXTH_INST: {
    RD = (RM >> (8 * rotate)) & 0xFFFF;
}
```

**CLZ（Leading Zero Count）:**
```c
CLZ_INST: {
    u32 rm = RM;
    int count = 0;
    for (int i = 31; i >= 0; i--) {
        if (BIT(rm, i))
            break;
        count++;
    }
    RD = count;
}
```

**REV（バイト反転）:**
```c
REV_INST: {
    RD = ((RM & 0xFF) << 24) 
       | (((RM >> 8) & 0xFF) << 16)
       | (((RM >> 16) & 0xFF) << 8)
       | ((RM >> 24) & 0xFF);
}
```

**REV16（ハーフワード内バイト反転）:**
```c
REV16_INST: {
    RD = ((RM & 0xFF) << 8) | ((RM & 0xFF00) >> 8)
       | ((RM & 0xFF0000) << 8) | ((RM & 0xFF000000) >> 8);
}
```

---

## 3. Thumb 命令実装パターン

### 3.1 Thumb デコーダー概要

Thumb 命令は ARM 命令に変換してから実行されます（arm_dyncom_thumb.cpp で実装）。

**主要なThumb形式:**

```
Format 0-2:   シフト/レジスタ操作 (LSL, LSR, ASR)
Format 3:     ADD/SUB (3ビットイミディエート)
Format 4-7:   MOV/CMP/ADD/SUB (8ビットイミディエート)
Format 8:     ALU操作 (AND, EOR, LSL, LSR, ASR, ADC, SBC, ROR, TST, NEG, CMP, CMN, ORR, MUL, BIC, MVN)
Format 9:     LDR PC相対ロード
Format 10-11: LDR/STR レジスタオフセット
Format 12-15: LDR/STR イミディエートオフセット
Format 16-19: 分岐 (B条件、B無条件、BL上位、BL下位、BLX)
```

### 3.2 Thumb デコード例

**Thumb ADD (Format 8, case 0x0):**
```c
// Thumb: ADD Rd, Rd, Rs
// → ARM:  E0800000 | (Rd << 16) | (Rd << 12) | (Rs << 0)
*ainstr = 0xE0800000
        | ((tinstr & 0x0007) << 16)   // Rn = Rd
        | ((tinstr & 0x0007) << 12)   // Rd
        | ((tinstr & 0x0038) >> 3);   // Rm = Rs
```

**Thumb LDR (Format 10-11):**
```c
// Thumb: LDR Rd, [Rb, Ro]
// → ARM:  E7900000 | (Rd << 12) | (Rb << 16) | (Ro << 0)
*ainstr = subset[opcode]
        | ((tinstr & 0x0007) << 12)  // Rd
        | ((tinstr & 0x0038) << 13)  // Rb
        | ((tinstr & 0x01C0) >> 6);  // Ro
```

---

## 4. cpu_interpreter.cpp の欠落命令

### 分析: 205個の命令から実装済みを確認

**要確認項目:**
- [ ] VFP/NEON 浮動小数点命令（VMLA, VMLS, VADD, VSUB, VDIV など 32個）
- [ ] DSP/SIMD 命令（SADD16, SADD8, SMUAD, SMLAD など 50個）
- [ ] 飽和演算命令（QADD, QSUB, QADD16, QSUB8 など 15個）
- [ ] マルチレジスタ転送（LDRD, STRD, LDM, STM 複数形式）
- [ ] 排他的アクセス（LDREX, STREX, LDREXD, STREXD など）
- [ ] コプロセッサ命令（MRC, MCR, CDP, STC, LDC）
- [ ] 64ビット乗算（SMULL, UMULL, SMLAL, UMLAL）
- [ ] Thumb 形式での分岐（B_COND_THUMB, BL_THUMB など）

---

## 5. 実装パターンまとめ

### 実装時の重要ポイント

#### 5.1 条件実行
```c
// すべてのARM命令は条件フィールドをサポート
if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
    // 命令実行
} else {
    // スキップ（PC += GetInstructionSize()）
}
```

#### 5.2 フラグ更新規則
```c
// S ビット設定時のフラグ更新
if (S_bit) {
    UPDATE_NFLAG(result);        // N = result[31]
    UPDATE_ZFLAG(result);        // Z = (result == 0) ? 1 : 0
    UPDATE_CFLAG_WITH_SC;        // C = shifter_carry_out
    // V はケースバイケース（演算による）
}
```

#### 5.3 PC（R15）の特別処理
```c
// Rd == 15 の場合
if (Rd == 15) {
    INC_PC(sizeof(xxx_inst));
    goto DISPATCH;  // 次の命令フェッチなしで即座に分岐
} else {
    cpu->Reg[15] += GetInstructionSize();
    INC_PC(sizeof(xxx_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
```

#### 5.4 Thumb ビット処理（LDR/LDM で R15）
```c
if (Rd == 15) {
    u32 value = ReadMemory32(addr);
    cpu->TFlag = value & 0x1;      // ビット0 = Thumbモード
    cpu->Reg[15] = value & 0xFFFFFFFE;  // ビット0クリア
    goto DISPATCH;
}
```

#### 5.5 シフター操作（データ処理命令）
```c
// オペランド2の処理
u32 shifter_operand = SHIFTER_OPERAND;  // シフター関数の結果
u32 shifter_carry_out = cpu->shifter_carry_out;  // シフター出力のキャリー
```

#### 5.6 R15 のアドレッシング補正
```c
// PC読み込み時
u32 rn_val = RN;
if (Rn == 15)
    rn_val += 2 * GetInstructionSize();  // +8 (ARM) または +4 (Thumb)
```

---

## 6. Reference から移植する際の注意点

### 6.1 構造の互換性
- Reference は `ARMul_State` 構造を使用
- cpu_interpreter.cpp は独自の `ARM_Interpreter` クラスを使用
- フィールド名とアクセス方法を統一する必要がある

### 6.2 マクロの確認
```c
#define RN      cpu->Reg[inst_cream->Rn]
#define RM      cpu->Reg[inst_cream->Rm]
#define RD      cpu->Reg[inst_cream->Rd]
#define RS      cpu->Reg[inst_cream->Rs]

#define SHIFTER_OPERAND cpu->shifter_operand
#define UPDATE_NFLAG(dst)  (cpu->NFlag = BIT(dst, 31) ? 1 : 0)
#define UPDATE_ZFLAG(dst)  (cpu->ZFlag = dst ? 0 : 1)
```

### 6.3 メモリアクセス関数の互換性
```c
Reference:
    cpu->ReadMemory32(addr)
    cpu->WriteMemory32(addr, value)
    cpu->ReadMemory16(addr)
    cpu->WriteMemory16(addr, value)
    cpu->ReadMemory8(addr)
    cpu->WriteMemory8(addr, value)

cpu_interpreter.cpp: 対応関数を確認し、必要に応じてラッパーを作成
```

### 6.4 条件コードの処理
```c
// CondPassed() のロジック確認
bool CondPassed(ARMul_State* cpu, unsigned int cond) {
    bool n = cpu->NFlag;
    bool z = cpu->ZFlag;
    bool c = cpu->CFlag;
    bool v = cpu->VFlag;
    
    switch (cond) {
    case 0x0: return z;           // EQ
    case 0x1: return !z;          // NE
    case 0x2: return c;           // CS/HS
    case 0x3: return !c;          // CC/LO
    case 0x4: return n;           // MI
    case 0x5: return !n;          // PL
    case 0x6: return v;           // VS
    case 0x7: return !v;          // VC
    case 0x8: return c && !z;     // HI
    case 0x9: return !c || z;     // LS
    case 0xA: return n == v;      // GE
    case 0xB: return n != v;      // LT
    case 0xC: return !z && n == v; // GT
    case 0xD: return z || n != v;  // LE
    case 0xE: return true;         // AL (unconditional)
    case 0xF: return false;        // Reserved/NV
    }
}
```

### 6.5 AddWithCarry 関数の実装
```c
u32 AddWithCarry(u32 x, u32 y, u32 c_in, bool* c_out, bool* v_out) {
    u64 result = (u64)x + (u64)y + c_in;
    *c_out = (result >> 32) & 1;
    
    u32 result_32 = result & 0xffffffff;
    bool x_sign = BIT(x, 31);
    bool y_sign = BIT(y, 31);
    bool r_sign = BIT(result_32, 31);
    
    *v_out = (x_sign == y_sign) && (x_sign != r_sign);
    
    return result_32;
}
```

### 6.6 飽和演算関数
```c
s16 ARMul_SignedSaturatedAdd16(s16 a, s16 b) {
    s32 result = (s32)a + (s32)b;
    if (result > 0x7FFF)
        return 0x7FFF;
    if (result < -0x8000)
        return -0x8000;
    return (s16)result;
}

s8 ARMul_SignedSaturatedAdd8(s8 a, s8 b) {
    s32 result = (s32)a + (s32)b;
    if (result > 0x7F)
        return 0x7F;
    if (result < -0x80)
        return -0x80;
    return (s8)result;
}
```

### 6.7 指定アドレッシング関数（get_addr）
```c
// LDR/STR 系で使用される
get_addr() は以下の形式に対応：
    - Immediate Offset: Rn ± imm12
    - Immediate Pre-indexed: Rn ± imm12, then Rd = [Rn±imm12]
    - Immediate Post-indexed: [Rn], then Rn ± imm12
    - Register Offset: Rn ± Rm
    - Register Pre-indexed: Rn ± Rm, then Rd = [Rn±Rm]
    - Register Post-indexed: [Rn], then Rn ± Rm
```

---

## 7. 統合チェックリスト

### フェーズ1: 基本命令の統合（優先度: 高）
- [ ] ADD, SUB, AND, ORR, EOR, BIC, MVN, MOV
- [ ] CMP, TST, CMN, TEQ
- [ ] LDR, STR, LDRB, STRB, LDRH, STRH
- [ ] B, BL, BX, BLX
- [ ] MUL, MLA

### フェーズ2: 拡張命令の統合（優先度: 中）
- [ ] UMULL, SMULL, UMLAL, SMLAL
- [ ] LDM, STM, LDRD, STRD
- [ ] ADC, SBC, RSB, RSC
- [ ] LDREX, STREX
- [ ] Thumb 分岐命令

### フェーズ3: 高度な命令の統合（優先度: 低）
- [ ] SADD16, SADD8, SMUAD, SMLAD など DSP命令
- [ ] QADD, QADD16, QADD8 など飽和命令
- [ ] VFP 浮動小数点命令
- [ ] コプロセッサ命令 (MRC, MCR)

---

## 8. テスト戦略

### テストケース例

```c
// ADD 命令テスト
TEST(CPUInterpreter, ADD_Basic) {
    // Rd = Rn + Rm
    cpu->Reg[1] = 5;
    cpu->Reg[2] = 3;
    ASSERT_EQ(executeADD(1, 2, 3), 8);
    ASSERT_EQ(cpu->Reg[3], 8);
}

// SUB 命令テスト
TEST(CPUInterpreter, SUB_WithFlags) {
    cpu->Reg[1] = 3;
    cpu->Reg[2] = 5;
    executeSUB_S(3, 1, 2);
    ASSERT_EQ(cpu->Reg[3], (u32)(-2));  // 2進数補数
    ASSERT_TRUE(cpu->NFlag);  // 負数
    ASSERT_FALSE(cpu->ZFlag);
    ASSERT_FALSE(cpu->CFlag);  // キャリーなし
}

// LDR/STR テスト
TEST(CPUInterpreter, LDR_STR) {
    cpu->WriteMemory32(0x1000, 0xDEADBEEF);
    cpu->Reg[13] = 0x1000;
    executeLDR(0, 13, 0);
    ASSERT_EQ(cpu->Reg[0], 0xDEADBEEF);
}

// 分岐テスト
TEST(CPUInterpreter, B_Conditional) {
    cpu->Reg[15] = 0x2000;
    cpu->ZFlag = 1;
    executeB_EQ(0x2020);  // BEQ (branch if equal)
    ASSERT_EQ(cpu->Reg[15], 0x2020);
}
```

---

## 参考情報

### ファイルサイズ統計
- arm_dyncom_interpreter.cpp: 4090 行
- arm_dyncom_thumb.cpp: 343 行
- ディスパッチャーテーブル: 205個の命令エントリ
- 実装済み命令: 全命令対応

### 主要な関数/マクロ
- `CondPassed()`: 条件コード判定
- `AddWithCarry()`: キャリー付き加算
- `ROTATE_RIGHT_32()`, `ROTATE_LEFT_32()`: ローテーション
- `DataProcessingOperands*()`: シフター関数群
- `LnSWoUB*()`: メモリアドレッシング関数群
- `LdnStM*()`: 複数レジスタ転送アドレッシング

---

**作成日**: 2026年4月22日  
**分析対象**: arm_dyncom_interpreter.cpp, arm_dyncom_thumb.cpp  
**ステータス**: 分析完了、統合準備中
