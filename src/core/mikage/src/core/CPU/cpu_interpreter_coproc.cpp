#include "../../../include/cpu_interpreter.hpp"
#include "./cpu_interpreter_internal.hpp"
u32 CPU::executeCoproc(u32 inst) {
	const auto get_reg_operand = [&](u32 index) { return getRegOperand(index, old_pc); };
	const auto set_carry = [&](bool value) { setCarry(value); };
	const auto write_reg = [&](u32 index, u32 value) { writeReg(index, value); };
	const auto clear_exclusive = [&]() { clearExclusive(); };
	const u32 old_pc = gprs[PC_INDEX];
	const auto cp1011_inst_class = Coproc10Decoder::DecodeInstructionClass(inst);

	// Coprocessor 64-bit transfer (MRRC/MCRR) subset for CP15.
	// Behavior adapted from Azahar DynCom's MRRC/MCRR instruction paths.
	if (cp1011_inst_class == Coproc10Decoder::InstructionClass::McrrMrrc) {
		const bool load = (inst & 0x00100000u) != 0;  // 1=MRRC, 0=MCRR
		const u32 rt = (inst >> 12) & 0xF;
		const u32 rt2 = (inst >> 16) & 0xF;
		const u32 coproc = (inst >> 8) & 0xF;
		const u32 crm = inst & 0xF;

		if (coproc == 15) {
			// Minimal pair mapping used by software that probes CP15 through MRRC/MCRR.
			// We expose TTBR0/TTBR1 as a logical 64-bit register pair on CRm==2 and
			// otherwise use a deterministic zero pair.
			if (load) {
				u32 lo = 0;
				u32 hi = 0;
				if (crm == 2) {
					lo = cp15TTBR0;
					hi = cp15TTBR1;
				}
				write_reg(rt, lo);
				write_reg(rt2, hi);
				if (rt != PC_INDEX && rt2 != PC_INDEX) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 1;
			}

			if (crm == 2) {
				cp15TTBR0 = get_reg_operand(rt);
				cp15TTBR1 = get_reg_operand(rt2);
			}
			gprs[PC_INDEX] = old_pc + 4;
			return 1;
		}

		// VFP/NEON 64-bit register transfers via MRRC/MCRR:
		// - coproc=10 (VMOVBRRSS): two ARM core regs <-> two S regs
		// - coproc=11 (VMOVBRRD):  two ARM core regs <-> one D reg
		if (coproc == 10 || coproc == 11) {
			if (coproc == 10) {
				const u32 s_base = ((inst & 0xFu) << 1) | ((inst >> 5) & 1u);
				if (s_base + 1 < 32) {
					if (load) {
						write_reg(rt, extRegs[s_base]);
						write_reg(rt2, extRegs[s_base + 1]);
					} else {
						extRegs[s_base] = get_reg_operand(rt);
						extRegs[s_base + 1] = get_reg_operand(rt2);
					}
					if (!(load && (rt == PC_INDEX || rt2 == PC_INDEX))) {
						gprs[PC_INDEX] = old_pc + 4;
					}
					return 1;
				}
			} else {
				const u32 d = (inst & 0xFu) | (((inst >> 5) & 1u) << 4);
				if (d < 32) {
					const u32 s_base = d * 2;
					if (load) {
						write_reg(rt, extRegs[s_base]);
						write_reg(rt2, extRegs[s_base + 1]);
					} else {
						extRegs[s_base] = get_reg_operand(rt);
						extRegs[s_base + 1] = get_reg_operand(rt2);
					}
					if (!(load && (rt == PC_INDEX || rt2 == PC_INDEX))) {
						gprs[PC_INDEX] = old_pc + 4;
					}
					return 1;
				}
			}
		}
	}

	// LDC/STC/CDP subset handling.
	// Imported in spirit from Azahar DynCom: keep decode-compatible behavior and
	// treat unsupported coprocessor memory/data-processing ops as safe no-ops in HLE path.
	if (cp1011_inst_class == Coproc10Decoder::InstructionClass::LdcStc ||
		cp1011_inst_class == Coproc10Decoder::InstructionClass::StructuredLdcStc ||
		cp1011_inst_class == Coproc10Decoder::InstructionClass::Cdp) {
		const u32 coproc = (inst >> 8) & 0xF;
		if (coproc == 10 || coproc == 11) {
			const Coproc10Decoder::Class coproc_class = Coproc10Decoder::DecodeClass(inst);
			// NEON/CDP subset: D/Q register integer ALU paths on cp10/cp11.
			// Decode/coverage expanded from Azahar-style interpreter behavior.
			if (coproc_class == Coproc10Decoder::Class::DataProc) {
				const u32 op = (inst >> 20) & 0xFu;
				const u32 op5 = (inst >> 5) & 0x1u;
				const u32 op6 = (inst >> 6) & 0x1u;
				const u32 elem_size = (inst >> 18) & 0x3u;  // minimal NEON element-size decode subset
				const bool use_q = (inst & (1u << 6)) != 0;
				const u32 vd = ((inst >> 12) & 0xFu) | (((inst >> 22) & 1u) << 4);
				const u32 vn = ((inst >> 16) & 0xFu) | (((inst >> 7) & 1u) << 4);
				const u32 vm = (inst & 0xFu) | (((inst >> 5) & 1u) << 4);
				const u32 reg_words = use_q ? 4u : 2u;
				if (vd < 32 && vn < 32 && vm < 32 && (vd * 2 + reg_words) <= 64 && (vn * 2 + reg_words) <= 64 &&
					(vm * 2 + reg_words) <= 64) {
					std::array<u32, 4> src_n{};
					std::array<u32, 4> src_m{};
					std::array<u32, 4> src_d_old{};
					std::array<u32, 4> dst{};
					for (u32 i = 0; i < reg_words; i++) {
						src_n[i] = extRegs[vn * 2 + i];
						src_m[i] = extRegs[vm * 2 + i];
						src_d_old[i] = extRegs[vd * 2 + i];
						dst[i] = src_n[i];
					}
					const auto lane_op = [&](u32 a, u32 b, bool sub, bool mul) -> u32 {
						switch (elem_size) {
							case 0: {  // 8-bit lanes
								u32 out = 0;
								for (u32 i = 0; i < 4; i++) {
									const u32 shift = i * 8;
									const u32 av = (a >> shift) & 0xFFu;
									const u32 bv = (b >> shift) & 0xFFu;
									const u32 rv = mul ? (av * bv) : (sub ? (av - bv) : (av + bv));
									out |= (rv & 0xFFu) << shift;
								}
								return out;
							}
							case 1: {  // 16-bit lanes
								u32 out = 0;
								for (u32 i = 0; i < 2; i++) {
									const u32 shift = i * 16;
									const u32 av = (a >> shift) & 0xFFFFu;
									const u32 bv = (b >> shift) & 0xFFFFu;
									const u32 rv = mul ? (av * bv) : (sub ? (av - bv) : (av + bv));
									out |= (rv & 0xFFFFu) << shift;
								}
								return out;
							}
							default:  // 32-bit lane
								return mul ? (a * b) : (sub ? (a - b) : (a + b));
						}
					};
						const auto lane_shift = [&](u32 a, u32 shift, bool right, bool arithmetic) -> u32 {
						switch (elem_size) {
							case 0: {
								u32 out = 0;
								for (u32 i = 0; i < 4; i++) {
									const u32 s = std::min<u32>(shift & 0xFFu, 8u);
									const u32 part = (a >> (i * 8)) & 0xFFu;
									u32 rv = part;
									if (right) {
										if (arithmetic && (part & 0x80u)) {
											const s32 signed_part = static_cast<s32>(static_cast<s8>(part));
											rv = static_cast<u32>(signed_part >> s) & 0xFFu;
										} else {
											rv = part >> s;
										}
									} else {
										rv = (part << s) & 0xFFu;
									}
									out |= (rv & 0xFFu) << (i * 8);
								}
								return out;
							}
							case 1: {
								u32 out = 0;
								for (u32 i = 0; i < 2; i++) {
									const u32 s = std::min<u32>(shift & 0xFFu, 16u);
									const u32 part = (a >> (i * 16)) & 0xFFFFu;
									u32 rv = part;
									if (right) {
										if (arithmetic && (part & 0x8000u)) {
											const s32 signed_part = static_cast<s32>(static_cast<s16>(part));
											rv = static_cast<u32>(signed_part >> s) & 0xFFFFu;
										} else {
											rv = part >> s;
										}
									} else {
										rv = (part << s) & 0xFFFFu;
									}
									out |= (rv & 0xFFFFu) << (i * 16);
								}
								return out;
							}
							default: {
								const u32 s = std::min<u32>(shift & 0xFFu, 32u);
								if (right) {
									if (s == 32) {
										return arithmetic && (a & 0x80000000u) ? 0xFFFFFFFFu : 0u;
									}
									if (arithmetic) {
										return static_cast<u32>(static_cast<s32>(a) >> s);
									}
									return a >> s;
								}
								return s == 32 ? 0u : (a << s);
							}
						}
						};
						const auto lane_minmax = [&](u32 a, u32 b, bool signed_cmp, bool take_max) -> u32 {
							switch (elem_size) {
								case 0: {
									u32 out = 0;
									for (u32 i = 0; i < 4; i++) {
										const u32 shift = i * 8;
										const u8 av = static_cast<u8>((a >> shift) & 0xFFu);
										const u8 bv = static_cast<u8>((b >> shift) & 0xFFu);
										const bool pick_a = signed_cmp
											? (take_max ? static_cast<s8>(av) >= static_cast<s8>(bv)
													   : static_cast<s8>(av) <= static_cast<s8>(bv))
											: (take_max ? av >= bv : av <= bv);
										out |= static_cast<u32>(pick_a ? av : bv) << shift;
									}
									return out;
								}
								case 1: {
									u32 out = 0;
									for (u32 i = 0; i < 2; i++) {
										const u32 shift = i * 16;
										const u16 av = static_cast<u16>((a >> shift) & 0xFFFFu);
										const u16 bv = static_cast<u16>((b >> shift) & 0xFFFFu);
										const bool pick_a = signed_cmp
											? (take_max ? static_cast<s16>(av) >= static_cast<s16>(bv)
													   : static_cast<s16>(av) <= static_cast<s16>(bv))
											: (take_max ? av >= bv : av <= bv);
										out |= static_cast<u32>(pick_a ? av : bv) << shift;
									}
									return out;
								}
								default: {
									if (signed_cmp) {
										const s32 av = static_cast<s32>(a);
										const s32 bv = static_cast<s32>(b);
										return static_cast<u32>(take_max ? std::max(av, bv) : std::min(av, bv));
									}
									return take_max ? std::max(a, b) : std::min(a, b);
								}
							}
						};
						const auto lane_qaddsub = [&](u32 a, u32 b, bool sub, bool signed_sat) -> u32 {
							switch (elem_size) {
								case 0: {
									u32 out = 0;
									for (u32 i = 0; i < 4; i++) {
										const u32 shift = i * 8;
										if (signed_sat) {
											const s32 av = static_cast<s8>((a >> shift) & 0xFFu);
											const s32 bv = static_cast<s8>((b >> shift) & 0xFFu);
											const s32 rv = sub ? (av - bv) : (av + bv);
											const s32 sat = std::clamp(rv, -128, 127);
											out |= static_cast<u32>(static_cast<u8>(static_cast<s8>(sat))) << shift;
											if (rv != sat) cpsr |= CPSR::StickyOverflow;
										} else {
											const s32 av = (a >> shift) & 0xFFu;
											const s32 bv = (b >> shift) & 0xFFu;
											const s32 rv = sub ? (av - bv) : (av + bv);
											const s32 sat = std::clamp(rv, 0, 255);
											out |= static_cast<u32>(sat) << shift;
											if (rv != sat) cpsr |= CPSR::StickyOverflow;
										}
									}
									return out;
								}
								case 1: {
									u32 out = 0;
									for (u32 i = 0; i < 2; i++) {
										const u32 shift = i * 16;
										if (signed_sat) {
											const s32 av = static_cast<s16>((a >> shift) & 0xFFFFu);
											const s32 bv = static_cast<s16>((b >> shift) & 0xFFFFu);
											const s32 rv = sub ? (av - bv) : (av + bv);
											const s32 sat = std::clamp(rv, -32768, 32767);
											out |= static_cast<u32>(static_cast<u16>(static_cast<s16>(sat))) << shift;
											if (rv != sat) cpsr |= CPSR::StickyOverflow;
										} else {
											const s32 av = (a >> shift) & 0xFFFFu;
											const s32 bv = (b >> shift) & 0xFFFFu;
											const s32 rv = sub ? (av - bv) : (av + bv);
											const s32 sat = std::clamp(rv, 0, 65535);
											out |= static_cast<u32>(sat) << shift;
											if (rv != sat) cpsr |= CPSR::StickyOverflow;
										}
									}
									return out;
								}
								default: {
									if (signed_sat) {
										const s64 av = static_cast<s32>(a);
										const s64 bv = static_cast<s32>(b);
										const s64 rv = sub ? (av - bv) : (av + bv);
										const s64 sat = std::clamp<s64>(rv, std::numeric_limits<s32>::min(), std::numeric_limits<s32>::max());
										if (rv != sat) cpsr |= CPSR::StickyOverflow;
										return static_cast<u32>(static_cast<s32>(sat));
									}
									const s64 av = a;
									const s64 bv = b;
									const s64 rv = sub ? (av - bv) : (av + bv);
									const s64 sat = std::clamp<s64>(rv, 0, std::numeric_limits<u32>::max());
									if (rv != sat) cpsr |= CPSR::StickyOverflow;
									return static_cast<u32>(sat);
								}
							}
						};

						switch (op) {
						case 0x0:
							for (u32 i = 0; i < reg_words; i++) dst[i] = src_n[i] & src_m[i];
							break;  // VAND
							case 0x1:
								for (u32 i = 0; i < reg_words; i++) dst[i] = src_n[i] & ~src_m[i];
								break;  // VBIC
								case 0x2:
									for (u32 i = 0; i < reg_words; i++) dst[i] = lane_minmax(src_n[i], src_m[i], op5, true);
									break;  // VMAX.{U,S}
								case 0x3:
									for (u32 i = 0; i < reg_words; i++) dst[i] = lane_minmax(src_n[i], src_m[i], op5, false);
									break;  // VMIN.{U,S}
								case 0x4:
									for (u32 i = 0; i < reg_words; i++) dst[i] = lane_qaddsub(src_n[i], src_m[i], false, op5);
									break;  // VQADD.{U,S}
								case 0x5:
									for (u32 i = 0; i < reg_words; i++) dst[i] = lane_qaddsub(src_n[i], src_m[i], true, op5);
									break;  // VQSUB.{U,S}
						case 0x6:
							for (u32 i = 0; i < reg_words; i++) dst[i] = lane_shift(src_n[i], src_m[i], true, false);
							break;  // VSHR (subset)
						case 0x7:
							for (u32 i = 0; i < reg_words; i++) dst[i] = lane_shift(src_n[i], src_m[i], true, true);
							break;  // VSRA (subset)
						case 0x8:
							for (u32 i = 0; i < reg_words; i++) dst[i] = lane_op(src_n[i], src_m[i], false, false);
							break;  // VADD
						case 0x9:
							for (u32 i = 0; i < reg_words; i++) dst[i] = lane_op(src_n[i], src_m[i], true, false);
							break;  // VSUB
						case 0xA:
							for (u32 i = 0; i < reg_words; i++) dst[i] = src_n[i] ^ src_m[i];
							break;  // VEOR
						case 0xB:
							for (u32 i = 0; i < reg_words; i++) dst[i] = src_n[i] | src_m[i];
							break;  // VORR
						case 0xC:
							for (u32 i = 0; i < reg_words; i++) dst[i] = ~src_n[i] | src_m[i];
							break;  // VORN
						case 0xD:
							for (u32 i = 0; i < reg_words; i++) dst[i] = ~src_m[i];
							break;  // VMVN
						case 0xE:
							if (!op6 && !op5) {
								for (u32 i = 0; i < reg_words; i++) dst[i] = (src_d_old[i] & src_n[i]) | (~src_d_old[i] & src_m[i]);  // VBSL
							} else if (!op6 && op5) {
								for (u32 i = 0; i < reg_words; i++) dst[i] = (src_m[i] & src_n[i]) | (~src_m[i] & src_d_old[i]);  // VBIT
							} else {
								for (u32 i = 0; i < reg_words; i++) dst[i] = (src_d_old[i] & src_m[i]) | (~src_d_old[i] & src_n[i]);  // VBIF
							}
							break;
						case 0xF:
							for (u32 i = 0; i < reg_words; i++) dst[i] = lane_op(src_n[i], src_m[i], false, true);
							break;  // VMUL
						default: break;
					}

					for (u32 i = 0; i < reg_words; i++) {
						extRegs[vd * 2 + i] = dst[i];
					}
					gprs[PC_INDEX] = old_pc + 4;
					return 1;
				}
			}

			if (coproc_class == Coproc10Decoder::Class::MemTransfer ||
				coproc_class == Coproc10Decoder::Class::StructuredMemTransfer) {
				const auto mem_dec = Coproc10Decoder::DecodeMem(inst, old_pc, get_reg_operand);
				const bool load = mem_dec.load;
				const bool add = mem_dec.add;
				const bool write_back = mem_dec.write_back;
				const u32 rn = mem_dec.rn;
				const u32 vd = mem_dec.vd;
				const u32 transfer_words = mem_dec.transfer_words;
				const bool interleaved = mem_dec.interleaved;
				const u32 interleave_factor = mem_dec.interleave_factor;
				const u32 base = mem_dec.base;
				u32 address = mem_dec.address;

				const u32 max_words = (interleaved && transfer_words <= interleave_factor) ? interleave_factor : transfer_words;
				if (vd * 2 + max_words <= 64) {
					if (coproc_class == Coproc10Decoder::Class::StructuredMemTransfer) {
						// Decode/execute by structured transfer class (VLD/VST 1-4 structures).
						const auto kind = mem_dec.structured_kind;
						const bool packed_lanes = transfer_words <= interleave_factor;
						if (packed_lanes) {
							const u32 lane_width = 32u / interleave_factor;
							const u32 lane_mask = (1u << lane_width) - 1u;
							for (u32 i = 0; i < transfer_words; i++) {
								const u32 lane_shift = i * lane_width;
								if (load) {
									const u32 value = mem.read32(address + i * 4);
									for (u32 lane = 0; lane < interleave_factor; lane++) {
										const u32 old = extRegs[vd * 2 + lane];
										const u32 lane_value = (value >> lane_shift) & lane_mask;
										extRegs[vd * 2 + lane] = (old & ~(lane_mask << lane_shift)) | (lane_value << lane_shift);
									}
								} else {
									u32 packed = 0;
									for (u32 lane = 0; lane < interleave_factor; lane++) {
										packed |= ((extRegs[vd * 2 + lane] >> lane_shift) & lane_mask) << lane_shift;
									}
									mem.write32(address + i * 4, packed);
									clear_exclusive();
								}
							}
						} else {
							u32 structure_regs = mem_dec.structure_regs;
							switch (kind) {
								case Coproc10Decoder::StructuredKind::VLD1:
								case Coproc10Decoder::StructuredKind::VST1: structure_regs = 1; break;
								case Coproc10Decoder::StructuredKind::VLD2:
								case Coproc10Decoder::StructuredKind::VST2: structure_regs = 2; break;
								case Coproc10Decoder::StructuredKind::VLD3:
								case Coproc10Decoder::StructuredKind::VST3: structure_regs = 3; break;
								case Coproc10Decoder::StructuredKind::VLD4:
								case Coproc10Decoder::StructuredKind::VST4: structure_regs = 4; break;
								default: break;
							}
							for (u32 i = 0; i < transfer_words; i++) {
								const u32 group = i / structure_regs;
								const u32 lane = i % structure_regs;
								const u32 group_words = (transfer_words + structure_regs - 1) / structure_regs;
								const u32 reg_index = vd * 2 + lane * group_words + group;
								if (load) {
									extRegs[reg_index] = mem.read32(address + i * 4);
								} else {
									mem.write32(address + i * 4, extRegs[reg_index]);
									clear_exclusive();
								}
							}
						}
					} else {
						if (interleaved && transfer_words <= interleave_factor) {
							const u32 lane_width = 32u / interleave_factor;
							const u32 lane_mask = (1u << lane_width) - 1u;
							for (u32 i = 0; i < transfer_words; i++) {
								const u32 lane_shift = i * lane_width;
								if (load) {
									const u32 value = mem.read32(address + i * 4);
									for (u32 lane = 0; lane < interleave_factor; lane++) {
										const u32 old = extRegs[vd * 2 + lane];
										const u32 lane_value = (value >> lane_shift) & lane_mask;
										extRegs[vd * 2 + lane] = (old & ~(lane_mask << lane_shift)) | (lane_value << lane_shift);
									}
								} else {
									u32 packed = 0;
									for (u32 lane = 0; lane < interleave_factor; lane++) {
										packed |= ((extRegs[vd * 2 + lane] >> lane_shift) & lane_mask) << lane_shift;
									}
									mem.write32(address + i * 4, packed);
									clear_exclusive();
								}
							}
						} else {
							for (u32 i = 0; i < transfer_words; i++) {
								u32 reg_index = vd * 2 + i;
								if (interleaved && transfer_words >= interleave_factor) {
									const u32 group = i / interleave_factor;
									const u32 lane = i % interleave_factor;
									const u32 group_words = (transfer_words + interleave_factor - 1) / interleave_factor;
									reg_index = vd * 2 + lane * group_words + group;
								}

								if (load) {
									extRegs[reg_index] = mem.read32(address + i * 4);
								} else {
									mem.write32(address + i * 4, extRegs[reg_index]);
									clear_exclusive();
								}
							}
						}
					}
					if (write_back && rn != PC_INDEX) {
						// LDC/STC post-index style addressing: when the low nibble encodes a valid ARM register,
						// use it as post-index source for compatibility with structured transfer assembler forms.
						const u32 post_rm = inst & 0xFu;
						u32 writeback_delta = transfer_words * 4;
						if (post_rm != 0xDu && post_rm != PC_INDEX) {
							writeback_delta = get_reg_operand(post_rm);
						}
						gprs[rn] = add ? (base + writeback_delta) : (base - writeback_delta);
					}
					gprs[PC_INDEX] = old_pc + 4;
					return 2 + transfer_words;
				}
			}
		}

		if (coproc == 10 || coproc == 11 || coproc == 15) {
			gprs[PC_INDEX] = old_pc + 4;
			return 1;
		}
	}

	// Coprocessor register transfer (MRC/MCR) - implement required CP15 subset used by HLE/userland.
	if (cp1011_inst_class == Coproc10Decoder::InstructionClass::McrMrc) {
		const bool load = (inst & (1u << 20)) != 0;  // 1=MRC, 0=MCR
		const u32 crn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 coproc = (inst >> 8) & 0xF;
		const u32 opc2 = (inst >> 5) & 0x7;
		const u32 crm = inst & 0xF;
		const u32 opc1 = (inst >> 21) & 0x7;

		if (coproc == 15) {
			const auto cp15_read = [&]() -> std::optional<u32> {
				// CPU ID / capability registers
				if (crn == 0 && crm == 0 && opc1 == 0 && opc2 == 0) return 0x410FC0F0u;  // MIDR (Cortex-A9-like)
				if (crn == 0 && crm == 0 && opc1 == 0 && opc2 == 1) return 0x0F0D2112u;  // CTR
				if (crn == 0 && crm == 0 && opc1 == 0 && opc2 == 5) return 0u;           // MPIDR (single-core view)

				// System control and MMU registers
				if (crn == 1 && crm == 0 && opc1 == 0 && opc2 == 0) return cp15SCTLR;
				if (crn == 1 && crm == 0 && opc1 == 0 && opc2 == 1) return cp15ACTLR;
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 0) return cp15TTBR0;
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 1) return cp15TTBR1;
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 2) return cp15TTBCR;
				if (crn == 3 && crm == 0 && opc1 == 0 && opc2 == 0) return cp15DACR;

				// Fault status/address registers
				if (crn == 5 && crm == 0 && opc1 == 0 && opc2 == 0) return cp15DFSR;
				if (crn == 5 && crm == 0 && opc1 == 0 && opc2 == 1) return cp15IFSR;
				if (crn == 6 && crm == 0 && opc1 == 0 && opc2 == 0) return cp15DFAR;
				if (crn == 6 && crm == 0 && opc1 == 0 && opc2 == 2) return cp15IFAR;
				return std::nullopt;
			};
			const auto cp15_write = [&](u32 value) -> bool {
				if (crn == 1 && crm == 0 && opc1 == 0 && opc2 == 0) {
					cp15SCTLR = value;
					return true;
				}
				if (crn == 1 && crm == 0 && opc1 == 0 && opc2 == 1) {
					cp15ACTLR = value;
					return true;
				}
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 0) {
					cp15TTBR0 = value;
					return true;
				}
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 1) {
					cp15TTBR1 = value;
					return true;
				}
				if (crn == 2 && crm == 0 && opc1 == 0 && opc2 == 2) {
					cp15TTBCR = value;
					return true;
				}
				if (crn == 3 && crm == 0 && opc1 == 0 && opc2 == 0) {
					cp15DACR = value;
					return true;
				}
				if (crn == 5 && crm == 0 && opc1 == 0 && opc2 == 0) {
					cp15DFSR = value;
					return true;
				}
				if (crn == 5 && crm == 0 && opc1 == 0 && opc2 == 1) {
					cp15IFSR = value;
					return true;
				}
				if (crn == 6 && crm == 0 && opc1 == 0 && opc2 == 0) {
					cp15DFAR = value;
					return true;
				}
				if (crn == 6 && crm == 0 && opc1 == 0 && opc2 == 2) {
					cp15IFAR = value;
					return true;
				}
				return false;
			};

			// TPIDRURW/TPIDRURO-style TLS register accesses used by many binaries.
			if (crn == 13 && crm == 0 && opc1 == 0 && opc2 == 2) {
				if (load) write_reg(rd, tlsBase);
				else tlsBase = get_reg_operand(rd);
				if (rd != PC_INDEX || !load) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 1;
			}
			if (crn == 13 && crm == 0 && opc1 == 0 && opc2 == 3) {
				if (load) write_reg(rd, tlsBase);
				else tlsBase = get_reg_operand(rd);
				if (rd != PC_INDEX || !load) {
					gprs[PC_INDEX] = old_pc + 4;
				}
				return 1;
			}

			if (load) {
				const auto value = cp15_read();
				if (value.has_value()) {
					write_reg(rd, *value);
					if (rd != PC_INDEX) {
						gprs[PC_INDEX] = old_pc + 4;
					}
					return 1;
				}
			} else if (cp15_write(get_reg_operand(rd))) {
				gprs[PC_INDEX] = old_pc + 4;
				return 1;
			}

			// CP15 barrier/cache maintenance instructions are accepted as no-ops in HLE interpreter path.
			if (!load) {
				clear_exclusive();
			}
			gprs[PC_INDEX] = old_pc + 4;
			return 1;
		}

		// VFP/NEON register transfers (VMRS/VMSR/VMOV scalar transfer subset).
		if (coproc == 10 || coproc == 11) {
			const auto azahar_cp1011_op = AzaharCp1011Decoder::Decode(inst);
			// VMRS / VMSR system register transfer family.
			// Matches Azahar DynCom's register map for reg selectors in CRn.
			if (azahar_cp1011_op == AzaharCp1011Decoder::Op::Vmrs ||
				azahar_cp1011_op == AzaharCp1011Decoder::Op::Vmsr) {
				if (load) {  // VMRS
					u32 value = 0;
					bool handled = true;
					switch (crn) {
						case 0: value = vfpFPSID; break;      // FPSID
						case 1: value = fpscr; break;          // FPSCR
						case 6: value = vfpMVFR1; break;       // MVFR1
						case 7: value = vfpMVFR0; break;       // MVFR0
						case 8: value = vfpFPEXC; break;       // FPEXC
						case 9: value = vfpFPINST; break;      // FPINST
						case 10: value = vfpFPINST2; break;    // FPINST2
						default: handled = false; break;
					}

					if (handled) {
						if (rd == PC_INDEX && crn == 1) {
							cpsr = (cpsr & ~0xF0000000u) | (value & 0xF0000000u);
						} else {
							write_reg(rd, value);
						}
						if (!(rd == PC_INDEX && crn == 1)) {
							gprs[PC_INDEX] = old_pc + 4;
						}
						return 1;
					}
				} else {  // VMSR
					const u32 value = get_reg_operand(rd);
					bool handled = true;
					switch (crn) {
						case 1: fpscr = value; break;        // FPSCR
						case 8: vfpFPEXC = value; break;     // FPEXC
						case 9: vfpFPINST = value; break;    // FPINST
						case 10: vfpFPINST2 = value; break;  // FPINST2
						default: handled = false; break;
					}

					if (handled) {
						gprs[PC_INDEX] = old_pc + 4;
						return 1;
					}
				}
			}

			// VMOVBRS exact MRC/MCR form:
			// cond 1110 000o Vn-- Rt-- 1010 N001 0000
			if ((inst & 0x0FE00F7Fu) == 0x0E000A10u) {
				const u32 s_index = (((inst >> 16) & 0xF) << 1) | ((inst >> 7) & 1u);
				if (s_index < 32) {
					if (load) {
						write_reg(rd, extRegs[s_index]);
					} else {
						extRegs[s_index] = get_reg_operand(rd);
					}
					if (!(load && rd == PC_INDEX)) {
						gprs[PC_INDEX] = old_pc + 4;
					}
					return 1;
				}
			}

			// VMOVBRC / VMOVBCR exact lane transfer forms on cp11:
			// - VMOVBRC (reg->scalar lane): cond 1110 0XX0 Vd-- Rt-- 1011 DXX1 0000
			// - VMOVBCR (scalar lane->reg): cond 1110 XXX1 Vd-- Rt-- 1011 NXX1 0000
			if ((inst & 0x0F900F1Fu) == 0x0E000B10u || (inst & 0x0F100F1Fu) == 0x0E100B10u) {
				const u32 d = ((inst >> 16) & 0xF) | (((inst >> 7) & 1u) << 4);
				const u32 lane = (inst >> 21) & 1u;
				const u32 s_index = d * 2 + lane;
				if (s_index < 64) {
					if (load) {
						write_reg(rd, extRegs[s_index]);
					} else {
						extRegs[s_index] = get_reg_operand(rd);
					}
					if (!(load && rd == PC_INDEX)) {
						gprs[PC_INDEX] = old_pc + 4;
					}
					return 1;
				}
			}

			// VMOV between ARM core register and single-precision VFP/NEON scalar S<n>.
			// Matches VMOVBRS-like register transfer subset.
			if (crm == 0 && opc2 == 0 && opc1 <= 1) {
				const u32 s_index = (((inst >> 16) & 0xF) << 1) | ((inst >> 7) & 1u);
				if (s_index < 32) {
					if (load) {
						write_reg(rd, extRegs[s_index]);
					} else {
						extRegs[s_index] = get_reg_operand(rd);
					}
					if (!(load && rd == PC_INDEX)) {
						gprs[PC_INDEX] = old_pc + 4;
					}
					return 1;
				}
			}

			// VMOV between ARM core register and D<n>[index] lane (32-bit lane transfer subset).
			// This provides a useful NEON scalar transfer path without full vector ALU implementation.
			if (crm == 0 && opc2 == 1) {
				const u32 d = ((inst >> 16) & 0xF) | (((inst >> 7) & 1u) << 4);
				const u32 lane = (inst >> 21) & 1u;
				const u32 s_index = d * 2 + lane;
				if (s_index < 32) {
					if (load) {
						write_reg(rd, extRegs[s_index]);
					} else {
						extRegs[s_index] = get_reg_operand(rd);
					}
					if (!(load && rd == PC_INDEX)) {
						gprs[PC_INDEX] = old_pc + 4;
					}
					return 1;
				}
			}
		}
	}

	// PLD (preload data hint) - architectural no-op in this HLE interpreter.
	// Pattern ported from Azahar DynCom's PLD_INST handling.
	if ((inst & 0x0C100000u) == 0x04100000u) {
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	// SETEND LE/BE: update CPSR.E.
	// In this little-endian-focused emulator path this is metadata-only, matching reference behavior.
	if ((inst & 0x0FFFFF7Fu) == 0x01010000u) {
		if (inst & (1u << 9)) {
			cpsr |= (1u << 9);
		} else {
			cpsr &= ~(1u << 9);
		}
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	// CPS[IE/ID] AIF,#mode
	// Pattern and semantics adapted from Azahar DynCom's CPS_INST handling.
	if ((inst & 0x0FF1FE00u) == 0x01000000u) {
		if (inst & (1u << 19)) {
			const bool disable = (inst & (1u << 18)) != 0;  // ID when set, IE when clear
			const u32 mask = inst & 0xE0u;
			if (disable) {
				cpsr |= mask;
			} else {
				cpsr &= ~mask;
			}
		}

		if (inst & (1u << 17)) {
			cpsr = (cpsr & ~0x1Fu) | (inst & 0x1Fu);
		}

		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	// SRS[DA/IA/DB/IB] sp!,#mode
	// Pattern and memory ordering adapted from Azahar DynCom's SRS_INST.
	if ((inst & 0x0E5FFFE0u) == 0x084D0500u) {
		u32* sp = &gprs[13];
		switch ((inst >> 23) & 0x3u) {
			case 0x0:  // DA
				mem.write32(*sp - 4, gprs[LR_INDEX]);
				mem.write32(*sp - 0, cpsr);
				if (inst & (1u << 21)) *sp -= 8;
				break;
			case 0x1:  // IA
				mem.write32(*sp + 0, gprs[LR_INDEX]);
				mem.write32(*sp + 4, cpsr);
				if (inst & (1u << 21)) *sp += 8;
				break;
			case 0x2:  // DB
				mem.write32(*sp - 8, gprs[LR_INDEX]);
				mem.write32(*sp - 4, cpsr);
				if (inst & (1u << 21)) *sp -= 8;
				break;
			case 0x3:  // IB
				mem.write32(*sp + 4, gprs[LR_INDEX]);
				mem.write32(*sp + 8, cpsr);
				if (inst & (1u << 21)) *sp += 8;
				break;
		}
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	// RFE[DA/IA/DB/IB] Rn!
	// Pattern and load order adapted from Azahar DynCom's RFE_INST.
	if ((inst & 0x0E50FFFFu) == 0x08100A00u) {
		const u32 rn = (inst >> 16) & 0xF;
		const u32 base = get_reg_operand(rn);
		u32 status_addr = base;
		u32 pc_addr = base + 4;
		switch ((inst >> 23) & 0x3u) {
			case 0x0: status_addr = base - 4; pc_addr = base - 0; if (inst & (1u << 21)) gprs[rn] = base - 8; break;  // DA
			case 0x1: status_addr = base + 0; pc_addr = base + 4; if (inst & (1u << 21)) gprs[rn] = base + 8; break;  // IA
			case 0x2: status_addr = base - 8; pc_addr = base - 4; if (inst & (1u << 21)) gprs[rn] = base - 8; break;  // DB
			case 0x3: status_addr = base + 4; pc_addr = base + 8; if (inst & (1u << 21)) gprs[rn] = base + 8; break;  // IB
		}
		cpsr = mem.read32(status_addr);
		gprs[PC_INDEX] = mem.read32(pc_addr);
		return 3;
	}

	// CLREX
	if ((inst & 0x0FFFFFF0u) == 0x057FF01Fu) {
		clear_exclusive();
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	// ARM architectural hints / barriers in SYSTEM space.
	// NOP/YIELD/WFE/WFI/SEV and DMB/DSB/ISB are treated as synchronization no-ops in this interpreter path.
	if ((inst & 0x0FFFFFF0u) == 0x0320F000u || (inst & 0x0FFFFFF0u) == 0x0320F010u || (inst & 0x0FFFFFF0u) == 0x0320F020u ||
		(inst & 0x0FFFFFF0u) == 0x0320F030u || (inst & 0x0FFFFFF0u) == 0x0320F040u || (inst & 0x0FFFFFF0u) == 0x057FF040u ||
		(inst & 0x0FFFFFF0u) == 0x057FF050u || (inst & 0x0FFFFFF0u) == 0x057FF060u) {
		gprs[PC_INDEX] = old_pc + 4;
		return 1;
	}

	// VFP single-precision VLDR/VSTR (memory transfer subset).
	// Encoding form: cond 1101 U D 0/1 Rn Vd 101X imm8
	if ((inst & 0x0F000E00u) == 0x0D000A00u) {
		const bool load = (inst & (1u << 20)) != 0;
		const bool single = (inst & (1u << 8)) == 0;
		if (single) {
			const u32 rn = (inst >> 16) & 0xF;
			const u32 vd = (((inst >> 12) & 0xF) << 1) | ((inst >> 22) & 1u);
			const u32 imm32 = (inst & 0xFFu) << 2;
			const bool add = (inst & (1u << 23)) != 0;
			if (vd < 32) {
				const u32 base = (rn == PC_INDEX) ? (((old_pc + 8) & ~3u)) : get_reg_operand(rn);
				const u32 addr = add ? (base + imm32) : (base - imm32);
				if (load) {
					extRegs[vd] = mem.read32(addr);
				} else {
					mem.write32(addr, extRegs[vd]);
					clear_exclusive();
				}
				gprs[PC_INDEX] = old_pc + 4;
				return 2;
			}
		} else {
			const u32 rn = (inst >> 16) & 0xF;
			const u32 vd = ((inst >> 12) & 0xF) | (((inst >> 22) & 1u) << 4);
			const u32 imm32 = (inst & 0xFFu) << 2;
			const bool add = (inst & (1u << 23)) != 0;
			if (vd < 32) {
				const u32 base = (rn == PC_INDEX) ? (((old_pc + 8) & ~3u)) : get_reg_operand(rn);
				const u32 addr = add ? (base + imm32) : (base - imm32);
				if (load) {
					extRegs[vd * 2] = mem.read32(addr);
					extRegs[vd * 2 + 1] = mem.read32(addr + 4);
				} else {
					mem.write32(addr, extRegs[vd * 2]);
					mem.write32(addr + 4, extRegs[vd * 2 + 1]);
					clear_exclusive();
				}
				gprs[PC_INDEX] = old_pc + 4;
				return 3;
			}
		}
	}

	// VFP single-precision VSTM/VLDM (multiple transfer subset, includes VPUSH/VPOP forms).
	// Encoding form: cond 110P U D W L Rn Vd 101X imm8
	if ((inst & 0x0F000E00u) == 0x0C000A00u) {
		const bool load = (inst & (1u << 20)) != 0;
		const bool single = (inst & (1u << 8)) == 0;
		if (single) {
			const bool add = (inst & (1u << 23)) != 0;
			const bool write_back = (inst & (1u << 21)) != 0;
			const u32 rn = (inst >> 16) & 0xF;
			const u32 vd = (((inst >> 12) & 0xF) << 1) | ((inst >> 22) & 1u);
			const u32 regs = inst & 0xFFu;
			const u32 imm32 = regs << 2;

			if (vd + regs <= 32) {
				u32 address = (rn == PC_INDEX) ? (old_pc + 8) : gprs[rn];
				if (!add) {
					address -= imm32;
				}

				for (u32 i = 0; i < regs; i++) {
					if (load) {
						extRegs[vd + i] = mem.read32(address);
					} else {
						mem.write32(address, extRegs[vd + i]);
						clear_exclusive();
					}
					address += 4;
				}

				if (write_back) {
					gprs[rn] = add ? (gprs[rn] + imm32) : (gprs[rn] - imm32);
				}
				gprs[PC_INDEX] = old_pc + 4;
				return 2 + regs;
			}
		} else {
			const bool add = (inst & (1u << 23)) != 0;
			const bool write_back = (inst & (1u << 21)) != 0;
			const u32 rn = (inst >> 16) & 0xF;
			const u32 vd = ((inst >> 12) & 0xF) | (((inst >> 22) & 1u) << 4);
			const u32 regs = (inst & 0xFEu) >> 1;
			const u32 imm32 = (inst & 0xFFu) << 2;

			if (vd + regs <= 32) {
				u32 address = (rn == PC_INDEX) ? (old_pc + 8) : gprs[rn];
				if (!add) {
					address -= imm32;
				}

				for (u32 i = 0; i < regs; i++) {
					if (load) {
						extRegs[(vd + i) * 2] = mem.read32(address);
						extRegs[(vd + i) * 2 + 1] = mem.read32(address + 4);
					} else {
						mem.write32(address, extRegs[(vd + i) * 2]);
						mem.write32(address + 4, extRegs[(vd + i) * 2 + 1]);
						clear_exclusive();
					}
					address += 8;
				}

				if (write_back) {
					gprs[rn] = add ? (gprs[rn] + imm32) : (gprs[rn] - imm32);
				}
				gprs[PC_INDEX] = old_pc + 4;
				return 2 + regs * 2;
			}
		}
	}

	// Halfword/signed data transfer (LDRH/LDRSH/LDRSB/STRH)
	if ((inst & 0x0E000090u) == 0x00000090u && (inst & 0x0C000000u) == 0x00000000u) {
		const bool pre = (inst & (1u << 24)) != 0;
		const bool up = (inst & (1u << 23)) != 0;
		const bool immediate = (inst & (1u << 22)) != 0;
		const bool write_back = (inst & (1u << 21)) != 0;
		const bool load = (inst & (1u << 20)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;
		const u32 sh = (inst >> 5) & 0x3;

		u32 offset = 0;
		if (immediate) {
			offset = ((inst >> 8) & 0xF) << 4;
			offset |= inst & 0xF;
		} else {
			offset = get_reg_operand(inst & 0xF);
		}

		const u32 base = get_reg_operand(rn);
		u32 effective = pre ? (up ? base + offset : base - offset) : base;

		if ((inst & 0xF0u) == 0xD0u && (rd & 1u) == 0) {  // STRD/LDRD
			if (load) {
				write_reg(rd, mem.read32(effective));
				write_reg(rd + 1, mem.read32(effective + 4));
			} else {
				mem.write32(effective, gprs[rd]);
				mem.write32(effective + 4, gprs[rd + 1]);
				clear_exclusive();
			}
		} else if (!load && sh == 0x1) {  // STRH
			mem.write16(effective, static_cast<u16>(gprs[rd]));
			clear_exclusive();
		} else if (load && sh == 0x1) {  // LDRH
			write_reg(rd, mem.read16(effective));
		} else if (load && sh == 0x2) {  // LDRSB
			const s8 value = static_cast<s8>(mem.read8(effective));
			write_reg(rd, static_cast<u32>(static_cast<s32>(value)));
		} else if (load && sh == 0x3) {  // LDRSH
			const s16 value = static_cast<s16>(mem.read16(effective));
			write_reg(rd, static_cast<u32>(static_cast<s32>(value)));
		} else {
			gprs[PC_INDEX] = old_pc + 4;
			return 1;
		}

		if (!pre) {
			effective = up ? base + offset : base - offset;
		}
		if (write_back || !pre) {
			gprs[rn] = effective;
		}
		if (!(load && rd == PC_INDEX)) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 2;
	}

	if ((inst & 0x0E000000u) == 0x08000000u) {
		const bool pre = (inst & (1u << 24)) != 0;
		const bool up = (inst & (1u << 23)) != 0;
		const bool psr = (inst & (1u << 22)) != 0;
		const bool write_back = (inst & (1u << 21)) != 0;
		const bool load = (inst & (1u << 20)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 reg_list = inst & 0xFFFF;
		if (psr || reg_list == 0) {
			gprs[PC_INDEX] = old_pc + 4;
			return 1;
		}

		u32 count = std::popcount(reg_list);
		u32 base = get_reg_operand(rn);
		u32 addr = base;
		if (up) {
			if (pre) {
				addr += 4;
			}
		} else {
			addr = base - (count * 4);
			if (!pre) {
				addr += 4;
			}
		}

		for (u32 reg = 0; reg < 16; reg++) {
			if ((reg_list & (1u << reg)) == 0) {
				continue;
			}
			if (load) {
				write_reg(reg, mem.read32(addr));
			} else {
				const u32 value = (reg == PC_INDEX) ? (old_pc + 12) : gprs[reg];
				mem.write32(addr, value);
				clear_exclusive();
			}
			addr += 4;
		}

		if (write_back) {
			gprs[rn] = up ? (base + count * 4) : (base - count * 4);
		}

		if ((load && (reg_list & (1u << PC_INDEX)) == 0) || !load) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 2 + count;
	}

	// STRT/STRBT/LDRBT/LDRT (unprivileged post-index single data transfer class).
	// Kept as explicit class decode to mirror reference naming and behavior.
	if ((inst & 0x0D300000u) == 0x04200000u || (inst & 0x0D300000u) == 0x04600000u ||
		(inst & 0x0D300000u) == 0x04700000u || (inst & 0x0D300000u) == 0x04300000u) {
		const bool immediate = (inst & (1u << 25)) == 0;
		const bool up = (inst & (1u << 23)) != 0;
		const bool byte = (inst & (1u << 22)) != 0; // STRBT/LDRBT
		const bool load = (inst & (1u << 20)) != 0; // LDRBT/LDRT
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;

		u32 offset = 0;
		if (immediate) {
			offset = inst & 0xFFFu;
		} else {
			const u32 rm = inst & 0xF;
			const u32 shift_type = (inst >> 5) & 0x3;
			const u32 shift_imm = (inst >> 7) & 0x1F;
			u32 rm_val = get_reg_operand(rm);
			switch (shift_type) {
				case 0: offset = rm_val << shift_imm; break;
				case 1: offset = (shift_imm == 0) ? 0u : (rm_val >> shift_imm); break;
				case 2:
					offset = (shift_imm == 0) ? (((rm_val >> 31) & 1u) ? 0xFFFFFFFFu : 0u)
											  : static_cast<u32>(static_cast<s32>(rm_val) >> shift_imm);
					break;
				default:
					offset = (shift_imm == 0) ? (((cpsr & CPSR::Carry) ? 1u : 0u) << 31) | (rm_val >> 1)
											  : ror32(rm_val, shift_imm);
					break;
			}
		}

		const u32 base = get_reg_operand(rn);
		const u32 effective = base; // post-index access uses base first
		if (load) {
			const u32 value = byte ? static_cast<u32>(mem.read8(effective)) : mem.read32(effective);
			write_reg(rd, value);
		} else {
			const u32 value = (rd == PC_INDEX) ? old_pc + 12 : gprs[rd];
			if (byte) mem.write8(effective, static_cast<u8>(value));
			else mem.write32(effective, value);
			clear_exclusive();
		}
		gprs[rn] = up ? (base + offset) : (base - offset);
		if (!(load && rd == PC_INDEX)) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 2;
	}

	// LDRCOND explicit class (condition != AL) from reference decoder table.
	// This is a class-level alias for conditional single data transfer and mirrors LDR addressing behavior.
	if ((inst & 0x0E500000u) == 0x04100000u && ((inst >> 28) & 0xFu) != 0xEu) {
		const bool immediate = (inst & (1u << 25)) == 0;
		const bool pre = (inst & (1u << 24)) != 0;
		const bool up = (inst & (1u << 23)) != 0;
		const bool byte = (inst & (1u << 22)) != 0;
		const bool write_back = (inst & (1u << 21)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;

		u32 offset = 0;
		if (immediate) {
			offset = inst & 0xFFFu;
		} else {
			const u32 rm = inst & 0xF;
			const u32 shift_type = (inst >> 5) & 0x3;
			const u32 shift_imm = (inst >> 7) & 0x1F;
			u32 rm_val = get_reg_operand(rm);
			switch (shift_type) {
				case 0: offset = rm_val << shift_imm; break;
				case 1: offset = (shift_imm == 0) ? 0u : (rm_val >> shift_imm); break;
				case 2:
					offset = (shift_imm == 0) ? (((rm_val >> 31) & 1u) ? 0xFFFFFFFFu : 0u)
											  : static_cast<u32>(static_cast<s32>(rm_val) >> shift_imm);
					break;
				default:
					offset = (shift_imm == 0) ? (((cpsr & CPSR::Carry) ? 1u : 0u) << 31) | (rm_val >> 1)
											  : ror32(rm_val, shift_imm);
					break;
			}
		}

		const u32 base = get_reg_operand(rn);
		u32 effective = pre ? (up ? (base + offset) : (base - offset)) : base;
		const u32 value = byte ? static_cast<u32>(mem.read8(effective)) : mem.read32(effective);
		write_reg(rd, value);
		if (!pre) {
			effective = up ? (base + offset) : (base - offset);
		}
		if (write_back || !pre) {
			gprs[rn] = effective;
		}
		if (rd != PC_INDEX) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 2;
	}

	// LDR/STR/LDRB/STRB/LDRCOND and related single-data-transfer class.
	if ((inst & 0x0C000000u) == 0x04000000u) {
		const bool immediate = (inst & (1u << 25)) == 0;
		const bool pre = (inst & (1u << 24)) != 0;
		const bool up = (inst & (1u << 23)) != 0;
		const bool byte = (inst & (1u << 22)) != 0;
		const bool write_back = (inst & (1u << 21)) != 0;
		const bool load = (inst & (1u << 20)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;

		u32 offset = 0;
		if (immediate) {
			offset = inst & 0xFFF;
		} else {
			const u32 rm = inst & 0xF;
			const u32 shift_type = (inst >> 5) & 0x3;
			const u32 shift_imm = (inst >> 7) & 0x1F;
			u32 rm_val = get_reg_operand(rm);
			switch (shift_type) {
				case 0: offset = rm_val << shift_imm; break;
				case 1: offset = (shift_imm == 0) ? 0 : (rm_val >> shift_imm); break;
				case 2: offset = (shift_imm == 0) ? (((rm_val >> 31) & 1u) ? 0xFFFFFFFFu : 0u)
										 : static_cast<u32>(static_cast<s32>(rm_val) >> shift_imm);
						break;
				case 3: offset = (shift_imm == 0) ? ((cpsr & CPSR::Carry) ? 0x80000000u : 0u) | (rm_val >> 1)
										 : ror32(rm_val, shift_imm);
						break;
			}
		}

		const u32 base = get_reg_operand(rn);
		u32 effective = base;
		if (pre) {
			effective = up ? (base + offset) : (base - offset);
		}

			if (load) {
				u32 value = byte ? static_cast<u32>(mem.read8(effective)) : mem.read32(effective);
				write_reg(rd, value);
			} else {
				const u32 value = (rd == PC_INDEX) ? old_pc + 12 : gprs[rd];
				if (byte) mem.write8(effective, static_cast<u8>(value));
				else mem.write32(effective, value);
				clear_exclusive();
			}

		if (!pre) {
			effective = up ? (base + offset) : (base - offset);
		}
		if (write_back || !pre) {
			gprs[rn] = effective;
		}

		if (!(load && rd == PC_INDEX)) {
			gprs[PC_INDEX] = old_pc + 4;
		}
		return 2;
	}

	if ((inst & 0x0C000000u) == 0x00000000u) {
		const bool immediate = (inst & (1u << 25)) != 0;
		const u32 opcode = (inst >> 21) & 0xF;
		const bool set_flags = (inst & (1u << 20)) != 0;
		const u32 rn = (inst >> 16) & 0xF;
		const u32 rd = (inst >> 12) & 0xF;

		bool shifter_carry = (cpsr & CPSR::Carry) != 0;
		u32 op2 = 0;
		if (immediate) {
			const u32 imm8 = inst & 0xFF;
			const u32 rot = ((inst >> 8) & 0xF) * 2;
			op2 = ror32(imm8, rot);
			if (rot != 0) {
				shifter_carry = (op2 >> 31) != 0;
			}
		} else {
			u32 rm_val = get_reg_operand(inst & 0xF);
			const bool by_register = (inst & (1u << 4)) != 0;
			const u32 shift_type = (inst >> 5) & 0x3;
			u32 shift_amount = 0;
			if (by_register) {
				shift_amount = get_reg_operand((inst >> 8) & 0xF) & 0xFF;
			} else {
				shift_amount = (inst >> 7) & 0x1F;
			}

			switch (shift_type) {
				case 0:  // LSL
					if (shift_amount == 0) {
						op2 = rm_val;
					} else if (shift_amount < 32) {
						op2 = rm_val << shift_amount;
						shifter_carry = ((rm_val >> (32 - shift_amount)) & 1u) != 0;
					} else if (shift_amount == 32) {
						op2 = 0;
						shifter_carry = (rm_val & 1u) != 0;
					} else {
						op2 = 0;
						shifter_carry = false;
					}
					break;
				case 1:  // LSR
					if (shift_amount == 0) {
						shift_amount = by_register ? 32 : 32;
					}
					if (shift_amount < 32) {
						op2 = rm_val >> shift_amount;
						shifter_carry = ((rm_val >> (shift_amount - 1)) & 1u) != 0;
					} else if (shift_amount == 32) {
						op2 = 0;
						shifter_carry = (rm_val >> 31) != 0;
					} else {
						op2 = 0;
						shifter_carry = false;
					}
					break;
				case 2:  // ASR
					if (shift_amount == 0) {
						shift_amount = by_register ? 32 : 32;
					}
					if (shift_amount < 32) {
						op2 = static_cast<u32>(static_cast<s32>(rm_val) >> shift_amount);
						shifter_carry = ((rm_val >> (shift_amount - 1)) & 1u) != 0;
					} else {
						op2 = (rm_val & 0x80000000u) ? 0xFFFFFFFFu : 0;
						shifter_carry = (rm_val >> 31) != 0;
					}
					break;
				case 3:  // ROR/RRX
					if (!by_register && shift_amount == 0) {
						op2 = ((cpsr & CPSR::Carry) ? 0x80000000u : 0u) | (rm_val >> 1);
						shifter_carry = (rm_val & 1u) != 0;
					} else {
						shift_amount &= 31;
						if (shift_amount == 0) {
							op2 = rm_val;
							shifter_carry = (rm_val >> 31) != 0;
						} else {
							op2 = ror32(rm_val, shift_amount);
							shifter_carry = (op2 >> 31) != 0;
						}
					}
					break;
			}
		}

		u32 lhs = get_reg_operand(rn);
		u32 result = 0;
		bool write_result = true;
		switch (opcode) {
			case 0x0: result = lhs & op2; break;                     // AND
			case 0x1: result = lhs ^ op2; break;                     // EOR
			case 0x2: result = lhs - op2; break;                     // SUB
			case 0x3: result = op2 - lhs; break;                     // RSB
			case 0x4: result = lhs + op2; break;                     // ADD
			case 0x5: {
				const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
				result = lhs + op2 + carry;
				break;
			}
			case 0x6: {
				const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
				result = lhs - op2 - (1u - carry);
				break;
			}
			case 0x7: {
				const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
				result = op2 - lhs - (1u - carry);
				break;
			}
			case 0x8: result = lhs & op2; write_result = false; break;  // TST
			case 0x9: result = lhs ^ op2; write_result = false; break;  // TEQ
			case 0xA: result = lhs - op2; write_result = false; break;  // CMP
			case 0xB: result = lhs + op2; write_result = false; break;  // CMN
			case 0xC: result = lhs | op2; break;                      // ORR
			case 0xD: result = op2; break;                            // MOV
			case 0xE: result = lhs & ~op2; break;                     // BIC
			case 0xF: result = ~op2; break;                           // MVN
		}

		if (set_flags || !write_result) {
			switch (opcode) {
				case 0x0:
				case 0x1:
				case 0x8:
				case 0x9:
				case 0xC:
				case 0xD:
				case 0xE:
				case 0xF:
					setNZFlags(result);
					set_carry(shifter_carry);
					break;
				case 0x2:
				case 0xA:
					setSubFlags(lhs, op2, result);
					break;
				case 0x3:
					setSubFlags(op2, lhs, result);
					break;
				case 0x4:
				case 0xB:
					setAddFlags(lhs, op2, result);
					break;
				case 0x5: {
					const u32 carry = (cpsr & CPSR::Carry) ? 1u : 0u;
					const u64 wide = static_cast<u64>(lhs) + static_cast<u64>(op2) + carry;
					setNZFlags(result);
					set_carry((wide >> 32) != 0);
					const bool overflow = ((~(lhs ^ op2) & (lhs ^ result)) & 0x80000000u) != 0;
					if (overflow) cpsr |= CPSR::Overflow;
					else cpsr &= ~CPSR::Overflow;
					break;
				}
				case 0x6:
				case 0x7: {                                                // RSC
					const u32 rhs = (opcode == 0x6) ? op2 : lhs;
					const u32 l = (opcode == 0x6) ? lhs : op2;
					setNZFlags(result);
					const u64 sub = static_cast<u64>(l) - static_cast<u64>(rhs) - ((cpsr & CPSR::Carry) ? 0u : 1u);
					set_carry((sub >> 63) == 0);
					const bool overflow = (((l ^ rhs) & (l ^ result)) & 0x80000000u) != 0;
					if (overflow) cpsr |= CPSR::Overflow;
					else cpsr &= ~CPSR::Overflow;
					break;
				}
			}
		}

		if (write_result) {
			write_reg(rd, result);
		}

		if (!(write_result && rd == PC_INDEX)) {
	return 0;
}
