# CPU Interpreter Full Decoder/Execution Gap Report

このレポートは `cpu_interpreter.cpp` の現状を、reference (`arm_dyncom_dec.cpp` / `arm_dyncom_interpreter.cpp`) と照合した「未完了点」メモです。

## 1) まだ "subset" 実装になっている領域

- VFP演算系が subset 実装（`VMLA/VMLS/...` など主要命令はあるが、デコーダ/実行の網羅性は reference 完全一致ではない）。
- VFPの `VMOV immediate` / `VCVT` / unary / compare も subset 注記。
- CP15/CP10/CP11 系は required subset を主眼にした実装で、完全系ではない。
- NEON/CDP は minimal subset として扱われている。

## 2) HLE no-op 扱いの領域（reference完全実装との差分になりうる）

- `LDC/STC/CDP` の未対応操作は no-op フォールバック。
- CP15 の一部 barrier/cache maintenance を no-op 受理。
- `PLD` は architectural no-op 扱い。
- `NOP/YIELD/WFE/WFI/SEV/DMB/DSB/ISB` は同期 no-op 扱い。

## 3) 「フルデコーダー＆実行エンジン」化のために残る作業

1. **reference decoderテーブルの完全移植（class単位）**
   - `arm_dyncom_dec.cpp` の命令クラスを 1:1 で `cpu_interpreter.cpp` 側にクラス列挙として持つ。
2. **subset/no-op の解消**
   - VFP/NEON/CDP/LDC/STC/CP15 maintenance で no-op/partial の箇所を reference 振る舞いで埋める。
3. **命令クラス単位の網羅テスト追加**
   - referenceの命令クラスごとにデコード一致・実行一致（状態遷移一致）を検証する回帰テストを追加。
4. **Thumb/ARM双方での完全一致検証**
   - Thumb変換・分岐系を含めた PC/CPSR/FPSCR の遷移一致を継続的に検証。

## 4) 補足

本リポジトリ側では、既に多くの ARMv6 DSP/media クラス（Q/SH/UH/UQ 系、SMUAD系、SMLA/SMUL/SMLAL 系など）を統合済みですが、
`subset` / `no-op` と明記されているクラスが残っているため、厳密な意味での "reference完全一致フルセット" は未達です。


## 5) subset/no-op 注記の現物行（cpu_interpreter.cpp）

- L1333: `// Parallel add/sub family (8-bit + 16-bit subsets).`
- L1543: `// VFP scalar arithmetic subset (single/double): VMLA/VMLS/VNMLA/VNMLS/VMUL/VNMUL/VADD/VSUB/VDIV.`
- L1693: `// VFP VMOV immediate (single/double scalar subset).`
- L1748: `// VCVT between single and double precision (VCVTBDS subset).`
- L1806: `// VCVT between floating-point and integer/fixed-point (VCVTBFI + VCVTBFF EXT opcode subsets).`
- L1949: `// VFP VMOV register-to-register (single/double scalar subset).`
- L2000: `// VFP unary arithmetic subset: VABS / VNEG / VSQRT (single/double).`
- L2094: `// VFP compare subset: VCMP/VCMPE/VCMP2 and compare-with-zero forms (single/double), updates FPSCR NZCV.`
- L2227: `// Coprocessor 64-bit transfer (MRRC/MCRR) subset for CP15.`
- L2237: `// Minimal pair mapping used by software that probes CP15 through MRRC/MCRR.`
- L2302: `// LDC/STC/CDP subset handling.`
- L2304: `// treat unsupported coprocessor memory/data-processing ops as safe no-ops in HLE path.`
- L2311: `// NEON/CDP subset: D/Q register integer ALU paths on cp10/cp11.`
- L2317: `const u32 elem_size = (inst >> 18) & 0x3u;  // minimal NEON element-size decode subset`
- L2546: `break;  // VSHR (subset)`
- L2549: `break;  // VSRA (subset)`
- L2721: `// Coprocessor register transfer (MRC/MCR) - implement required CP15 subset used by HLE/userland.`
- L2829: `// CP15 barrier/cache maintenance instructions are accepted as no-ops in HLE interpreter path.`
- L2837: `// VFP/NEON register transfers (VMRS/VMSR/VMOV scalar transfer subset).`
- L2925: `// Matches VMOVBRS-like register transfer subset.`
- L2941: `// VMOV between ARM core register and D<n>[index] lane (32-bit lane transfer subset).`
- L2962: `// PLD (preload data hint) - architectural no-op in this HLE interpreter.`
- L3058: `// NOP/YIELD/WFE/WFI/SEV and DMB/DSB/ISB are treated as synchronization no-ops in this interpreter path.`
- L3066: `// VFP single-precision VLDR/VSTR (memory transfer subset).`
- L3110: `// VFP single-precision VSTM/VLDM (multiple transfer subset, includes VPUSH/VPOP forms).`


## 6) 実装優先度（フルセット化の実作業順）

- **P0: decode class 1:1化**
  - reference命令クラスを `cpu_interpreter.cpp` 側に完全列挙して、クラス未一致をビルド時/実行時に検出可能にする。
- **P1: no-op除去**
  - `LDC/STC/CDP` の no-op 分岐、barrier/cache maintenance の no-op 分岐を reference準拠実装へ置換。
- **P2: VFP/NEON subset除去**
  - scalarのみ/一部演算のみの実装を、reference相当のクラス網羅に拡張。
- **P3: クラス単位回帰テスト**
  - 命令クラスごとの decode/execute 一致試験を追加（ARM/Thumb 両方）。

## 7) 現時点判定

- 現在の `cpu_interpreter.cpp` は「大幅拡張済み」だが、`subset` / `minimal` / `no-op` と明記されたクラスが残るため、
  **strictな意味でのフルデコーダー＆フル実行エンジンは未完了**。

## 8) 今回着手した実作業

- P0 の先行着手として、reference decoder (`arm_dyncom_dec.cpp`) から導出した
  **命令クラスパターン表**を `reference_arm_class_table.inc` として追加し、
  `cpu_interpreter.cpp` 側で `DecodeReferenceArmClass()` を呼び出せる状態にした。
- これにより、クラス単位 1:1 照合の土台（188クラスのマッチャ）をコード上に統合済み。
