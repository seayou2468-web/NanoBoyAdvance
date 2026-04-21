# VFP/NEON gap report (vs current ARM11 interpreter)

This document tracks what is still missing after the current Azahar-derived VFP integration work in
`src/core/mikage/src/core/CPU/cpu_interpreter.cpp`.

## Implemented families (current)

- Scalar VFP arithmetic: `VMLA/VMLS/VNMLA/VNMLS/VMUL/VNMUL/VADD/VSUB/VDIV` (S/D).
- Scalar VFP unary ops: `VMOV (imm/reg)`, `VABS`, `VNEG`, `VSQRT`.
- Scalar VFP compare: `VCMP/VCMPE` (reg and zero forms).
- Scalar VFP conversion subsets:
  - `VCVT` single<->double (`VCVTBDS`).
  - `VCVT` float<->int subset (`FUITO/FSITO/FTOUI/FTOUIZ/FTOSI/FTOSIZ`-style handling).
- VFP memory/register transfer subsets:
  - `VLDR/VSTR`, `VSTM/VLDM` (+ VPUSH/VPOP forms through same path).
  - `VMRS/VMSR` system-register family.
  - `VMOVBRS`, `VMOVBRC/VMOVBCR`, `VMOVBRRSS`, `VMOVBRRD`.

## Still missing / incomplete

1. **`VCVTBFF` fixed-point conversion family is only partially implemented.**
   - Decode path now routes through the same EXT conversion core used by `VCVTBFI`.
   - Full bit-exact fixed-point semantics parity with Azahar/Skyeye softfloat still requires
     dedicated validation for all encoding variants.

2. **VFP vector-length/stride execution (FPSCR LEN/STRIDE) is partially implemented.**
   - Single-precision arithmetic/unary/conversion/compare paths now iterate LEN/STRIDE for banked-register forms.
   - Remaining VFP instruction families still do not have full LEN/STRIDE parity.

3. **Exception/trap behavior is partial.**
   - Host FP exceptions now feed both FPSCR cumulative bits and basic FPEXC trap bits in key scalar paths.
   - Full trap-enable/precise exception model parity with the legacy Skyeye/Azahar VFP core is still not complete.

4. **NEON SIMD ALU/data-processing instruction families are partially implemented.**
   - Added a minimal cp11 CDP subset for common 2x32-bit lane ops (`VAND/VBIC/VADD/VSUB/VEOR/VORR/VORN/VMVN/VBSL`-style handling).
   - Full Advanced SIMD decode/execute parity is still not present.

5. **NEON structured loads/stores and lane/dup/ext/zip/unzip/trn/rev family are partially implemented.**
   - Added a minimal contiguous/interleaved LDC/STC-derived structured transfer subset on cp11.
   - Most advanced structured/lane permutation forms are still missing.

## Practical note

For ARM11 MPCore targets, full NEON parity may not be required for architectural correctness
(because ARM11 MPCore does not expose a full NEON unit), but binaries/toolchains that emit NEON-like
encodings or rely on broader CP10/CP11 behavior still need explicit compatibility handling if that
software is to run under this interpreter.
