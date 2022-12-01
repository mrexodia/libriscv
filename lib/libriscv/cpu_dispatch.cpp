#include "machine.hpp"
#include "decoder_cache.hpp"
#include "instruction_counter.hpp"
#include "instruction_list.hpp"
#include "threaded_bytecodes.hpp"
#include "rv32i_instr.hpp"
#include "rvfd.hpp"
#ifdef RISCV_EXT_VECTOR
#include "rvv.hpp"
#endif

namespace riscv
{
#define VIEW_INSTR() \
	auto instr = decoder->view_instr();
#define VIEW_INSTR_AS(name, x) \
	auto name = decoder->template view_instr<x>();
#define NEXT_INSTR()            \
	goto *computed_opcode[(++decoder)->get_bytecode()];
#define NEXT_BLOCK(len) \
	pc += len; \
	decoder++; \
	goto continue_block;
#define PERFORM_BRANCH() \
	pc += instr.Btype.signed_imm(); \
	goto check_jump;
#define PERFORM_FAST_BRANCH() \
	pc += fi.signed_imm(); \
	goto check_jump;

template <int W> __attribute__((hot))
void CPU<W>::simulate_threaded(uint64_t imax)
{
	static constexpr uint32_t XLEN = W * 8;
	using addr_t  = address_type<W>;
	using saddr_t = std::make_signed_t<addr_t>;

	static constexpr void *computed_opcode[] = {
		[RV32I_BC_ADDI]    = &&rv32i_addi,
		[RV32I_BC_LI]      = &&rv32i_li,
		[RV32I_BC_SLLI]    = &&rv32i_slli,
		[RV32I_BC_SLTI]    = &&rv32i_slti,
		[RV32I_BC_SLTIU]   = &&rv32i_sltiu,
		[RV32I_BC_XORI]    = &&rv32i_xori,
		[RV32I_BC_SRLI]    = &&rv32i_srli,
		[RV32I_BC_SRAI]    = &&rv32i_srai,
		[RV32I_BC_ORI]     = &&rv32i_ori,
		[RV32I_BC_ANDI]    = &&rv32i_andi,
		[RV32I_BC_LUI]     = &&rv32i_lui,
		[RV32I_BC_AUIPC]   = &&rv32i_auipc,
		[RV32I_BC_LDB]     = &&rv32i_ldb,
		[RV32I_BC_LDBU]    = &&rv32i_ldbu,
		[RV32I_BC_LDH]     = &&rv32i_ldh,
		[RV32I_BC_LDHU]    = &&rv32i_ldhu,
		[RV32I_BC_LDW]     = &&rv32i_ldw,
		[RV32I_BC_LDD]     = &&rv32i_ldd,
		[RV32I_BC_STB]     = &&rv32i_stb,
		[RV32I_BC_STH]     = &&rv32i_sth,
		[RV32I_BC_STW]     = &&rv32i_stw,
		[RV32I_BC_STD]     = &&rv32i_std,
		[RV32I_BC_BEQ]     = &&rv32i_beq,
		[RV32I_BC_BNE]     = &&rv32i_bne,
		[RV32I_BC_BLT]     = &&rv32i_blt,
		[RV32I_BC_BGE]     = &&rv32i_bge,
		[RV32I_BC_BLTU]    = &&rv32i_bltu,
		[RV32I_BC_BGEU]    = &&rv32i_bgeu,
		[RV32I_BC_JAL]     = &&rv32i_jal,
		[RV32I_BC_JALR]    = &&rv32i_jalr,
		[RV32I_BC_FAST_JAL] = &&rv32i_fast_jal,
		[RV32I_BC_OP_ADD]  = &&rv32i_op_add,
		[RV32I_BC_OP_SUB]  = &&rv32i_op_sub,
		[RV32I_BC_OP_SLL]  = &&rv32i_op_sll,
		[RV32I_BC_OP_SLT]  = &&rv32i_op_slt,
		[RV32I_BC_OP_SLTU] = &&rv32i_op_sltu,
		[RV32I_BC_OP_XOR]  = &&rv32i_op_xor,
		[RV32I_BC_OP_SRL]  = &&rv32i_op_srl,
		[RV32I_BC_OP_OR]   = &&rv32i_op_or,
		[RV32I_BC_OP_AND]  = &&rv32i_op_and,
		[RV32I_BC_OP_MUL]  = &&rv32i_op_mul,
		[RV32I_BC_OP_MULH] = &&rv32i_op_mulh,
		[RV32I_BC_OP_MULHSU] = &&rv32i_op_mulhsu,
		[RV32I_BC_OP_MULHU]= &&rv32i_op_mulhu,
		[RV32I_BC_OP_DIV]  = &&rv32i_op_div,
		[RV32I_BC_OP_DIVU] = &&rv32i_op_divu,
		[RV32I_BC_OP_REM]  = &&rv32i_op_rem,
		[RV32I_BC_OP_REMU] = &&rv32i_op_remu,
		[RV32I_BC_OP_SRA]  = &&rv32i_op_sra,
		[RV32I_BC_OP_SH1ADD] = &&rv32i_op_sh1add,
		[RV32I_BC_OP_SH2ADD] = &&rv32i_op_sh2add,
		[RV32I_BC_OP_SH3ADD] = &&rv32i_op_sh3add,
		[RV32I_BC_SYSCALL] = &&rv32i_syscall,
		[RV32I_BC_SYSTEM]  = &&rv32i_system,
		[RV32F_BC_FLW]     = &&rv32i_flw,
		[RV32F_BC_FLD]     = &&rv32i_fld,
		[RV32F_BC_FSW]     = &&rv32i_fsw,
		[RV32F_BC_FSD]     = &&rv32i_fsd,
		[RV32F_BC_FADD]    = &&rv32f_fadd,
		[RV32F_BC_FSUB]    = &&rv32f_fsub,
		[RV32F_BC_FMUL]    = &&rv32f_fmul,
		[RV32F_BC_FDIV]    = &&rv32f_fdiv,
#ifdef RISCV_EXT_VECTOR
		[RV32V_BC_VLE32]   = &&rv32v_vle32,
		[RV32V_BC_VSE32]   = &&rv32v_vse32,
#endif
		[RV32I_BC_NOP]     = &&rv32i_nop,
		[RV32I_BC_FUNCTION] = &&execute_decoded_function,
#ifdef RISCV_BINARY_TRANSLATION
		[RV32I_BC_TRANSLATOR] = &&translated_function,
#endif
		[RV32I_BC_INVALID] = &&execute_decoded_function,
	};

	// We need an execute segment matching current PC
	if (UNLIKELY(!is_executable(this->pc())))
	{
		this->next_execute_segment();
	}

	// Calculate the instruction limit
	if (imax != UINT64_MAX)
		machine().set_max_instructions(machine().instruction_counter() + imax);
	else
		machine().set_max_instructions(UINT64_MAX);

	InstrCounter counter{machine()};

	DecodedExecuteSegment<W>* exec = this->m_exec;
	DecoderData<W>* exec_decoder;
	DecoderData<W>* decoder;
	address_t current_begin;
	address_t current_end;
	address_t pc = this->pc();
restart_sim:
	exec_decoder = exec->decoder_cache();
	current_begin = exec->exec_begin();
	current_end = exec->exec_end();

continue_segment:
	decoder = &exec_decoder[pc / DecoderCache<W>::DIVISOR];

continue_block:
	{
		unsigned count = decoder->idxend;
		pc += count * 4;
		counter.increment_counter(count + 1);
	}

	goto *computed_opcode[decoder->get_bytecode()];

rv32i_li: {
	VIEW_INSTR();
	this->reg(instr.Itype.rd) = instr.Itype.signed_imm();
	NEXT_INSTR();
}
rv32i_addi: {
	if constexpr (decoder_rewriter_enabled) {
		VIEW_INSTR_AS(fi, FasterItype);
		this->reg(fi.rs1) =
			this->reg(fi.rs2) + fi.signed_imm();
	} else {
		VIEW_INSTR();
		this->reg(instr.Itype.rd) =
			this->reg(instr.Itype.rs1) + instr.Itype.signed_imm();
	}
	NEXT_INSTR();
}
rv32i_ldw: {
	if constexpr (decoder_rewriter_enabled) {
		VIEW_INSTR_AS(fi, FasterItype);
		const auto addr = this->reg(fi.rs2) + fi.signed_imm();
		this->reg(fi.rs1) =
			(int32_t)machine().memory.template read<uint32_t>(addr);
	} else {
		VIEW_INSTR();
		const auto addr = this->reg(instr.Itype.rs1) + instr.Itype.signed_imm();
		this->reg(instr.Itype.rd) =
			(int32_t)machine().memory.template read<uint32_t>(addr);
	}
	NEXT_INSTR();
}
rv32i_ldd: {
	VIEW_INSTR();
	const auto addr = this->reg(instr.Itype.rs1) + instr.Itype.signed_imm();
	this->reg(instr.Itype.rd) =
		(int64_t)machine().memory.template read<uint64_t>(addr);
	NEXT_INSTR();
}
rv32i_stw: {
	if constexpr (decoder_rewriter_enabled) {
		VIEW_INSTR_AS(fi, FasterItype);
		const auto addr  = reg(fi.rs1) + fi.signed_imm();
		machine().memory.template write<uint32_t>(addr, reg(fi.rs2));
	} else {
		VIEW_INSTR();
		const auto addr  = reg(instr.Stype.rs1) + instr.Stype.signed_imm();
		machine().memory.template write<uint32_t>(addr, reg(instr.Stype.rs2));
	}
	NEXT_INSTR();
}
rv32i_std: {
	VIEW_INSTR();
	const auto addr = reg(instr.Stype.rs1) + instr.Stype.signed_imm();
	machine().memory.template write<uint64_t>(addr, reg(instr.Stype.rs2));
	NEXT_INSTR();
}
rv32i_beq: {
	if constexpr (decoder_rewriter_enabled) {
		VIEW_INSTR_AS(fi, FasterItype);
		if (reg(fi.rs1) == reg(fi.rs2)) {
			PERFORM_FAST_BRANCH();
		}
	} else {
		VIEW_INSTR();
		if (reg(instr.Btype.rs1) == reg(instr.Btype.rs2)) {
			PERFORM_BRANCH();
		}
	}
	NEXT_BLOCK(4);
}
rv32i_bne: {
	if constexpr (decoder_rewriter_enabled) {
		VIEW_INSTR_AS(fi, FasterItype);
		if (reg(fi.rs1) != reg(fi.rs2)) {
			PERFORM_FAST_BRANCH();
		}
	} else {
		VIEW_INSTR();
		if (reg(instr.Btype.rs1) != reg(instr.Btype.rs2)) {
			PERFORM_BRANCH();
		}
	}
	NEXT_BLOCK(4);
}
rv32i_blt: {
	VIEW_INSTR();
	if ((saddr_t)reg(instr.Btype.rs1) < (saddr_t)reg(instr.Btype.rs2)) {
		PERFORM_BRANCH();
	}
	NEXT_BLOCK(4);
}
rv32i_bge: {
	VIEW_INSTR();
	if ((saddr_t)reg(instr.Btype.rs1) >= (saddr_t)reg(instr.Btype.rs2)) {
		PERFORM_BRANCH();
	}
	NEXT_BLOCK(4);
}
rv32i_bltu: {
	VIEW_INSTR();
	if (reg(instr.Btype.rs1) < reg(instr.Btype.rs2)) {
		PERFORM_BRANCH();
	}
	NEXT_BLOCK(4);
}
rv32i_bgeu: {
	VIEW_INSTR();
	if (reg(instr.Btype.rs1) >= reg(instr.Btype.rs2)) {
		PERFORM_BRANCH();
	}
	NEXT_BLOCK(4);
}
rv32i_op_add: {
	VIEW_INSTR();
	if constexpr (decoder_rewriter_enabled) {
		VIEW_INSTR_AS(fi, FasterOpType);
		this->reg(fi.rd) = reg(fi.rs1) + reg(fi.rs2);
	} else {
		this->reg(instr.Rtype.rd) =
			reg(instr.Rtype.rs1) + reg(instr.Rtype.rs2);
	}
	NEXT_INSTR();
}
rv32i_op_sub: {
	VIEW_INSTR();
	this->reg(instr.Rtype.rd) =
		reg(instr.Rtype.rs1) - reg(instr.Rtype.rs2);
	NEXT_INSTR();
}
rv32i_slli: {
	VIEW_INSTR();
	// SLLI: Logical left-shift 5/6/7-bit immediate
	this->reg(instr.Itype.rd) =
		reg(instr.Itype.rs1) << (instr.Itype.imm & (XLEN - 1));
	NEXT_INSTR();
}
rv32i_slti: {
	VIEW_INSTR();
	// SLTI: Set less than immediate
	this->reg(instr.Itype.rd) =
		(saddr_t(reg(instr.Itype.rs1)) < instr.Itype.signed_imm());
	NEXT_INSTR();
}
rv32i_sltiu: {
	VIEW_INSTR();
	// SLTIU: Sign-extend, then treat as unsigned
	this->reg(instr.Itype.rd) =
		(reg(instr.Itype.rs1) < addr_t(instr.Itype.signed_imm()));
	NEXT_INSTR();
}
rv32i_xori: {
	VIEW_INSTR();
	// XORI
	this->reg(instr.Itype.rd) =
		reg(instr.Itype.rs1) ^ instr.Itype.signed_imm();
	NEXT_INSTR();
}
rv32i_srli: {
	VIEW_INSTR();
	// SRLI: Shift-right logical 5/6/7-bit immediate
	this->reg(instr.Itype.rd) =
		reg(instr.Itype.rs1) >> (instr.Itype.imm & (XLEN - 1));
	NEXT_INSTR();
}
rv32i_srai: {
	VIEW_INSTR();
	// SRAI: Shift-right arithmetical (preserve the sign bit)
	this->reg(instr.Itype.rd) =
		saddr_t(reg(instr.Itype.rs1)) >> (instr.Itype.imm & (XLEN - 1));
	NEXT_INSTR();
}
rv32i_ori: {
	VIEW_INSTR();
	// ORI: Or sign-extended 12-bit immediate
	this->reg(instr.Itype.rd) =
		reg(instr.Itype.rs1) | instr.Itype.signed_imm();
	NEXT_INSTR();
}
rv32i_andi: {
	VIEW_INSTR();
	// ANDI: And sign-extended 12-bit immediate
	this->reg(instr.Itype.rd) =
		reg(instr.Itype.rs1) & instr.Itype.signed_imm();
	NEXT_INSTR();
}
rv32i_jal: {
	if constexpr (decoder_rewriter_enabled)
	{
		VIEW_INSTR_AS(fi, FasterJtype);
		if (fi.rd != 0)
			reg(fi.rd) = pc + 4;
		pc += fi.offset;
		goto check_jump;
	} else {
		VIEW_INSTR();
		// Link *next* instruction (rd = PC + 4)
		if (instr.Jtype.rd != 0) {
			reg(instr.Jtype.rd) = pc + 4;
		}
		// And jump relative
		pc += instr.Jtype.jump_offset();
		goto check_unaligned_jump;
	}
}
rv32i_fast_jal: {
	VIEW_INSTR();
	pc = instr.whole;
	goto check_jump;
}
rv32i_jalr: {
	VIEW_INSTR();
	// jump to register + immediate
	// NOTE: if rs1 == rd, avoid clobber by storing address first
	const auto address = reg(instr.Itype.rs1) + instr.Itype.signed_imm();
	// Link *next* instruction (rd = PC + 4)
	if (instr.Itype.rd != 0) {
		reg(instr.Itype.rd) = pc + 4;
	}
	pc = address;
	goto check_unaligned_jump;
}
rv32i_lui: {
	VIEW_INSTR();
	this->reg(instr.Utype.rd) = instr.Utype.upper_imm();
	NEXT_INSTR();
}

rv32i_op_sll: {
	VIEW_INSTR();
	#define OPREGS() \
		auto& dst = reg(instr.Rtype.rd); \
		const auto src1 = reg(instr.Rtype.rs1); \
		const auto src2 = reg(instr.Rtype.rs2);
	OPREGS();
	dst = src1 << (src2 & (XLEN-1));
	NEXT_INSTR();
}
rv32i_op_slt: {
	VIEW_INSTR();
	OPREGS();
	dst = (saddr_t(src1) < saddr_t(src2));
	NEXT_INSTR();
}
rv32i_op_sltu: {
	VIEW_INSTR();
	OPREGS();
	dst = (src1 < src2);
	NEXT_INSTR();
}
rv32i_op_xor: {
	VIEW_INSTR();
	OPREGS();
	dst = src1 ^ src2;
	NEXT_INSTR();
}
rv32i_op_srl: {
	VIEW_INSTR();
	OPREGS();
	dst = src1 >> (src2 & (XLEN - 1));
	NEXT_INSTR();
}
rv32i_op_or: {
	VIEW_INSTR();
	OPREGS();
	dst = src1 | src2;
	NEXT_INSTR();
}
rv32i_op_and: {
	VIEW_INSTR();
	OPREGS();
	dst = src1 & src2;
	NEXT_INSTR();
}
rv32i_op_mul: {
	VIEW_INSTR();
	OPREGS();
	dst = saddr_t(src1) * saddr_t(src2);
	NEXT_INSTR();
}
rv32i_op_div: {
	VIEW_INSTR();
	OPREGS();
	// division by zero is not an exception
	if (LIKELY(saddr_t(src2) != 0)) {
		if constexpr (W == 8) {
			// vi_instr.cpp:444:2: runtime error:
			// division of -9223372036854775808 by -1 cannot be represented in type 'long'
			if (LIKELY(!((int64_t)src1 == INT64_MIN && (int64_t)src2 == -1ll)))
				dst = saddr_t(src1) / saddr_t(src2);
		} else {
			// rv32i_instr.cpp:301:2: runtime error:
			// division of -2147483648 by -1 cannot be represented in type 'int'
			if (LIKELY(!(src1 == 2147483648 && src2 == 4294967295)))
				dst = saddr_t(src1) / saddr_t(src2);
		}
	} else {
		dst = addr_t(-1);
	}
	NEXT_INSTR();
}
rv32i_op_sh1add: {
	VIEW_INSTR();
	OPREGS();
	dst = src2 + (src1 << 1);
	NEXT_INSTR();
}
rv32i_op_sh2add: {
	VIEW_INSTR();
	OPREGS();
	dst = src2 + (src1 << 2);
	NEXT_INSTR();
}
rv32i_op_sh3add: {
	VIEW_INSTR();
	OPREGS();
	dst = src2 + (src1 << 3);
	NEXT_INSTR();
}

rv32i_syscall: {
	// Make the current PC visible
	this->registers().pc = pc;
	// Make the instruction counter visible
	counter.apply();
	// Invoke system call
	machine().system_call(this->reg(REG_ECALL));
	// Restore counter
	counter.retrieve();
	if (UNLIKELY(counter.overflowed() || pc != this->registers().pc))
	{
		// System calls are always full-length instructions
		pc = registers().pc + 4;
		goto check_jump;
	}
	NEXT_BLOCK(4);
}

rv32i_ldb: {
	VIEW_INSTR();
	const auto addr = reg(instr.Itype.rs1) + instr.Itype.signed_imm();
	reg(instr.Itype.rd) =
		int8_t(machine().memory.template read<uint8_t>(addr));
	NEXT_INSTR();
}
rv32i_ldbu: {
	VIEW_INSTR();
	const auto addr = reg(instr.Itype.rs1) + instr.Itype.signed_imm();
	reg(instr.Itype.rd) =
		saddr_t(machine().memory.template read<uint8_t>(addr));
	NEXT_INSTR();
}
rv32i_ldh: {
	VIEW_INSTR();
	const auto addr = reg(instr.Itype.rs1) + instr.Itype.signed_imm();
	reg(instr.Itype.rd) =
		int16_t(machine().memory.template read<uint16_t>(addr));
	NEXT_INSTR();
}
rv32i_ldhu: {
	VIEW_INSTR();
	const auto addr = reg(instr.Itype.rs1) + instr.Itype.signed_imm();
	reg(instr.Itype.rd) =
		saddr_t(machine().memory.template read<uint16_t>(addr));
	NEXT_INSTR();
}
rv32i_stb: {
	VIEW_INSTR();
	const auto addr = reg(instr.Stype.rs1) + instr.Stype.signed_imm();
	machine().memory.template write<uint8_t>(addr, reg(instr.Stype.rs2));
	NEXT_INSTR();
}
rv32i_sth: {
	VIEW_INSTR();
	const auto addr = reg(instr.Stype.rs1) + instr.Stype.signed_imm();
	machine().memory.template write<uint16_t>(addr, reg(instr.Stype.rs2));
	NEXT_INSTR();
}

rv32i_flw: {
	VIEW_INSTR_AS(fi, rv32f_instruction);
	auto addr = reg(fi.Itype.rs1) + fi.Itype.signed_imm();
	auto& dst = registers().getfl(fi.Itype.rd);
	dst.load_u32(machine().memory.template read<uint32_t> (addr));
	NEXT_INSTR();
}
rv32i_fld: {
	VIEW_INSTR_AS(fi, rv32f_instruction);
	auto addr = reg(fi.Itype.rs1) + fi.Itype.signed_imm();
	auto& dst = registers().getfl(fi.Itype.rd);
	dst.load_u64(machine().memory.template read<uint64_t> (addr));
	NEXT_INSTR();
}
rv32i_fsw: {
	VIEW_INSTR_AS(fi, rv32f_instruction);
	const auto& src = registers().getfl(fi.Stype.rs2);
	auto addr = reg(fi.Stype.rs1) + fi.Stype.signed_imm();
	machine().memory.template write<uint32_t> (addr, src.i32[0]);
	NEXT_INSTR();
}
rv32i_fsd: {
	VIEW_INSTR_AS(fi, rv32f_instruction);
	const auto& src = registers().getfl(fi.Stype.rs2);
	auto addr = reg(fi.Stype.rs1) + fi.Stype.signed_imm();
	machine().memory.template write<uint64_t> (addr, src.i64);
	NEXT_INSTR();
}
rv32i_system: {
	VIEW_INSTR();
	machine().system(instr);
	// Check if machine stopped
	if (UNLIKELY(counter.overflowed()))
	{
		registers().pc = pc + 4;
		return;
	}
	NEXT_BLOCK(4);
}
rv32f_fadd: {
	VIEW_INSTR_AS(fi, rv32f_instruction);
	#define FLREGS() \
		auto& dst = registers().getfl(fi.R4type.rd); \
		const auto& rs1 = registers().getfl(fi.R4type.rs1); \
		const auto& rs2 = registers().getfl(fi.R4type.rs2);
	FLREGS();
	if (fi.R4type.funct2 == 0x0)
	{ // float32
		dst.set_float(rs1.f32[0] + rs2.f32[0]);
	}
	else if (fi.R4type.funct2 == 0x1)
	{ // float64
		dst.f64 = rs1.f64 + rs2.f64;
	}
	NEXT_INSTR();
}
rv32f_fsub: {
	VIEW_INSTR_AS(fi, rv32f_instruction);
	FLREGS();
	if (fi.R4type.funct2 == 0x0)
	{ // float32
		dst.set_float(rs1.f32[0] - rs2.f32[0]);
	}
	else if (fi.R4type.funct2 == 0x1)
	{ // float64
		dst.f64 = rs1.f64 - rs2.f64;
	}
	NEXT_INSTR();
}
rv32f_fmul: {
	VIEW_INSTR_AS(fi, rv32f_instruction);
	FLREGS();
	if (fi.R4type.funct2 == 0x0)
	{ // float32
		dst.set_float(rs1.f32[0] * rs2.f32[0]);
	}
	else if (fi.R4type.funct2 == 0x1)
	{ // float64
		dst.f64 = rs1.f64 * rs2.f64;
	}
	NEXT_INSTR();
}
rv32f_fdiv: {
	VIEW_INSTR_AS(fi, rv32f_instruction);
	FLREGS();
	if (fi.R4type.funct2 == 0x0)
	{ // float32
		dst.set_float(rs1.f32[0] / rs2.f32[0]);
	}
	else if (fi.R4type.funct2 == 0x1)
	{ // float64
		dst.f64 = rs1.f64 / rs2.f64;
	}
	NEXT_INSTR();
}

check_jump:
	if (UNLIKELY(counter.overflowed())) {
		registers().pc = pc;
		return;
	}
	if (UNLIKELY(!(pc >= current_begin && pc < current_end)))
	{
		// We have to store and restore PC here as there are
		// custom callbacks when changing segments that can
		// jump around.
		registers().pc = pc;
		// Change execute segment
		exec = this->next_execute_segment();
		pc = registers().pc;
		// Restart with new execute boundaries
		goto restart_sim;
	}
	goto continue_segment;

/** UNLIKELY INSTRUCTIONS **/
/** UNLIKELY INSTRUCTIONS **/

execute_decoded_function: {
	VIEW_INSTR();
	auto handler = decoder->get_handler();
	handler(*this, instr);
	NEXT_INSTR();
}

rv32i_nop: {
	NEXT_INSTR();
}
rv32i_auipc: {
	VIEW_INSTR();
	this->reg(instr.Utype.rd) = pc + instr.Utype.upper_imm();
	NEXT_BLOCK(4);
}
rv32i_op_sra: {
	VIEW_INSTR();
	OPREGS();
	dst = saddr_t(src1) >> (src2 & (XLEN-1));
	NEXT_INSTR();
}
rv32i_op_mulh: {
	VIEW_INSTR();
	OPREGS();
	if constexpr (W == 4) {
		dst = uint64_t((int64_t)saddr_t(src1) * (int64_t)saddr_t(src2)) >> 32u;
	} else if constexpr (W == 8) {
		dst = ((__int128_t) src1 * (__int128_t) src2) >> 64u;
	} else {
		dst = 0;
	}
	NEXT_INSTR();
}
rv32i_op_mulhsu: {
	VIEW_INSTR();
	OPREGS();
	if constexpr (W == 4) {
		dst = uint64_t((int64_t)saddr_t(src1) * (uint64_t)src2) >> 32u;
	} else if constexpr (W == 8) {
		dst = ((__int128_t) src1 * (__int128_t) src2) >> 64u;
	} else {
		dst = 0;
	}
	NEXT_INSTR();
}
rv32i_op_mulhu: {
	VIEW_INSTR();
	OPREGS();
	if constexpr (W == 4) {
		dst = uint64_t((uint64_t)src1 * (uint64_t)src2) >> 32u;
	} else if constexpr (W == 8) {
		dst = ((__int128_t) src1 * (__int128_t) src2) >> 64u;
	} else {
		dst = 0;
	}
	NEXT_INSTR();
}
rv32i_op_divu: {
	VIEW_INSTR();
	OPREGS();
	if (LIKELY(src2 != 0)) {
		dst = src1 / src2;
	} else {
		dst = addr_t(-1);
	}
	NEXT_INSTR();
}
rv32i_op_rem: {
	VIEW_INSTR();
	OPREGS();
	if (LIKELY(src2 != 0)) {
		if constexpr(W == 4) {
			if (LIKELY(!(src1 == 2147483648 && src2 == 4294967295)))
				dst = saddr_t(src1) % saddr_t(src2);
		} else if constexpr (W == 8) {
			if (LIKELY(!((int64_t)src1 == INT64_MIN && (int64_t)src2 == -1ll)))
				dst = saddr_t(src1) % saddr_t(src2);
		} else {
			dst = saddr_t(src1) % saddr_t(src2);
		}
	}
	NEXT_INSTR();
}
rv32i_op_remu: {
	VIEW_INSTR();
	OPREGS();
	if (LIKELY(src2 != 0)) {
		dst = src1 % src2;
	} else {
		dst = addr_t(-1);
	}
	NEXT_INSTR();
}
#ifdef RISCV_EXT_VECTOR
rv32v_vle32: {
	VIEW_INSTR_AS(vi, rv32v_instruction);
	const auto addr = reg(vi.VLS.rs1) & ~address_t(VectorLane::size()-1);
	registers().rvv().get(vi.VLS.vd) =
		machine().memory.template read<VectorLane> (addr);
	NEXT_INSTR();
}
rv32v_vse32: {
	VIEW_INSTR_AS(vi, rv32v_instruction);
	const auto addr = reg(vi.VLS.rs1) & ~address_t(VectorLane::size()-1);
	auto& dst = registers().rvv().get(vi.VLS.vd);
	machine().memory.template write<VectorLane> (addr, dst);
	NEXT_INSTR();
}
#endif // RISCV_EXT_VECTOR

/** UNLIKELY INSTRUCTIONS **/
/** UNLIKELY INSTRUCTIONS **/

#ifdef RISCV_BINARY_TRANSLATION
translated_function: {
	VIEW_INSTR();
	counter.apply();
	auto handler = decoder->get_handler();
	handler(*this, instr);
	// Restore instruction counter and PC
	counter.retrieve();
	pc = registers().pc;
	goto check_jump;
}
#endif

check_unaligned_jump:
	if constexpr (!compressed_enabled) {
		if (UNLIKELY(pc & 0x3)) {
			registers().pc = pc;
			trigger_exception(MISALIGNED_INSTRUCTION, this->pc());
		}
	} else {
		if (UNLIKELY(pc & 0x1)) {
			registers().pc = pc;
			trigger_exception(MISALIGNED_INSTRUCTION, this->pc());
		}
	}
	goto check_jump;

} // CPU::simulate_computed()

template <int W>
size_t CPU<W>::computed_index_for(rv32i_instruction instr)
{
#ifdef RISCV_BINARY_TRANSLATION
	if (instr.whole == FASTSIM_BLOCK_END)
		return RV32I_BC_TRANSLATOR;
#endif

	switch (instr.opcode())
	{
		case RV32I_LOAD:
			// XXX: Support dummy loads
			if (instr.Itype.rd == 0)
				return RV32I_BC_NOP;
			switch (instr.Itype.funct3) {
			case 0x0: // LD.B
				return RV32I_BC_LDB;
			case 0x1: // LD.H
				return RV32I_BC_LDH;
			case 0x2: // LD.W
				return RV32I_BC_LDW;
			case 0x3:
				if constexpr (W >= 8) {
					return RV32I_BC_LDD;
				}
				return RV32I_BC_INVALID;
			case 0x4: // LD.BU
				return RV32I_BC_LDBU;
			case 0x5: // LD.HU
				return RV32I_BC_LDHU;
			default:
				return RV32I_BC_INVALID;
			}
		case RV32I_STORE:
			switch (instr.Stype.funct3)
			{
			case 0x0: // SD.B
				return RV32I_BC_STB;
			case 0x1: // SD.H
				return RV32I_BC_STH;
			case 0x2: // SD.W
				return RV32I_BC_STW;
			case 0x3:
				if constexpr (W >= 8) {
					return RV32I_BC_STD;
				}
				return RV32I_BC_INVALID;
			default:
				return RV32I_BC_INVALID;
			}
		case RV32I_BRANCH:
			switch (instr.Btype.funct3) {
			case 0x0: // BEQ
				return RV32I_BC_BEQ;
			case 0x1: // BNE
				return RV32I_BC_BNE;
			case 0x4: // BLT
				return RV32I_BC_BLT;
			case 0x5: // BGE
				return RV32I_BC_BGE;
			case 0x6: // BLTU
				return RV32I_BC_BLTU;
			case 0x7: // BGEU
				return RV32I_BC_BGEU;
			default:
				return RV32I_BC_INVALID;
			}
		case RV32I_LUI:
			if (instr.Utype.rd == 0)
				return RV32I_BC_NOP;
			return RV32I_BC_LUI;
		case RV32I_AUIPC:
			if (instr.Utype.rd == 0)
				return RV32I_BC_NOP;
			return RV32I_BC_AUIPC;
		case RV32I_JAL:
			return RV32I_BC_JAL;
		case RV32I_JALR:
			return RV32I_BC_JALR;
		case RV32I_OP_IMM:
			if (instr.Itype.rd == 0)
				return RV32I_BC_NOP;
			switch (instr.Itype.funct3)
			{
			case 0x0:
				if (instr.Itype.rs1 == 0)
					return RV32I_BC_LI;
				else
					return RV32I_BC_ADDI;
			case 0x1: // SLLI
				return RV32I_BC_SLLI;
			case 0x2: // SLTI
				return RV32I_BC_SLTI;
			case 0x3: // SLTIU
				return RV32I_BC_SLTIU;
			case 0x4: // XORI
				return RV32I_BC_XORI;
			case 0x5:
				if (instr.Itype.is_srai())
					return RV32I_BC_SRAI;
				else
					return RV32I_BC_SRLI;
			case 0x6:
				return RV32I_BC_ORI;
			case 0x7:
				return RV32I_BC_ANDI;
			default:
				return RV32I_BC_INVALID;
			}
		case RV32I_OP:
			if (instr.Itype.rd == 0)
				return RV32I_BC_NOP;
			switch (instr.Rtype.jumptable_friendly_op())
			{
			case 0x0:
				return RV32I_BC_OP_ADD;
			case 0x200:
				return RV32I_BC_OP_SUB;
			case 0x1:
				return RV32I_BC_OP_SLL;
			case 0x2:
				return RV32I_BC_OP_SLT;
			case 0x3:
				return RV32I_BC_OP_SLTU;
			case 0x4:
				return RV32I_BC_OP_XOR;
			case 0x5:
				return RV32I_BC_OP_SRL;
			case 0x6:
				return RV32I_BC_OP_OR;
			case 0x7:
				return RV32I_BC_OP_AND;
			case 0x10:
				return RV32I_BC_OP_MUL;
			case 0x11:
				return RV32I_BC_OP_MULH;
			case 0x12:
				return RV32I_BC_OP_MULHSU;
			case 0x13:
				return RV32I_BC_OP_MULHU;
			case 0x14:
				return RV32I_BC_OP_DIV;
			case 0x15:
				return RV32I_BC_OP_DIVU;
			case 0x16:
				return RV32I_BC_OP_REM;
			case 0x17:
				return RV32I_BC_OP_REMU;
			case 0x102:
				return RV32I_BC_OP_SH1ADD;
			case 0x104:
				return RV32I_BC_OP_SH2ADD;
			case 0x106:
				return RV32I_BC_OP_SH3ADD;
			//case 0x204:
			//	return RV32I_BC_OP_XNOR;
			case 0x205:
				return RV32I_BC_OP_SRA;
			default:
				return RV32I_BC_INVALID;
			}
		case RV64I_OP32:
		case RV64I_OP_IMM32:
			return RV32I_BC_FUNCTION;
		case RV32I_SYSTEM:
			if (LIKELY(instr.Itype.funct3 == 0))
			{
				if (instr.Itype.imm == 0) {
					return RV32I_BC_SYSCALL;
				}
			}
			return RV32I_BC_SYSTEM;
		case RV32I_FENCE:
			return RV32I_BC_NOP;
		case RV32F_LOAD: {
			const rv32f_instruction fi{instr};
			switch (fi.Itype.funct3) {
			case 0x2: // FLW
				return RV32F_BC_FLW;
			case 0x3: // FLD
				return RV32F_BC_FLD;
#ifdef RISCV_EXT_VECTOR
			case 0x6: // VLE32
				return RV32V_BC_VLE32;
#endif
			default:
				return RV32I_BC_INVALID;
			}
		}
		case RV32F_STORE: {
			const rv32f_instruction fi{instr};
			switch (fi.Itype.funct3) {
			case 0x2: // FSW
				return RV32F_BC_FSW;
			case 0x3: // FSD
				return RV32F_BC_FSD;
#ifdef RISCV_EXT_VECTOR
			case 0x6: // VSE32
				return RV32V_BC_VSE32;
#endif
			default:
				return RV32I_BC_INVALID;
			}
		}
		case RV32F_FMADD:
		case RV32F_FMSUB:
		case RV32F_FNMADD:
		case RV32F_FNMSUB:
			return RV32I_BC_FUNCTION;
		case RV32F_FPFUNC:
			switch (instr.fpfunc())
			{
				case 0b00000: // FADD
					return RV32F_BC_FADD;
				case 0b00001: // FSUB
					return RV32F_BC_FSUB;
				case 0b00010: // FMUL
					return RV32F_BC_FMUL;
				case 0b00011: // FDIV
					return RV32F_BC_FDIV;
				default:
					return RV32I_BC_FUNCTION;
				}
#ifdef RISCV_EXT_VECTOR
		case RV32V_OP:
			return RV32I_BC_FUNCTION;
#endif
#ifdef RISCV_EXT_ATOMICS
		case RV32A_ATOMIC:
			return RV32I_BC_FUNCTION;
#endif
		default:
			return RV32I_BC_INVALID;
	}
} // computed_index_for()

	template struct CPU<4>;
	template struct CPU<8>;
	template struct CPU<16>;
} // riscv
