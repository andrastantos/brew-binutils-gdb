/* RISC-V opcode list
   Copyright (C) 2011-2021 Free Software Foundation, Inc.

   Contributed by Andrew Waterman (andrew@sifive.com).
   Based on MIPS target.

   This file is part of the GNU opcodes library.

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   It is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING3. If not,
   see <http://www.gnu.org/licenses/>.  */

#include "sysdep.h"
#include "opcode/riscv.h"
#include <stdio.h>

/* Register names used by gas and objdump.  */

const char * const riscv_gpr_names_numeric[NGPR] =
{
  "x0",   "x1",   "x2",   "x3",   "x4",   "x5",   "x6",   "x7",
  "x8",   "x9",   "x10",  "x11",  "x12",  "x13",  "x14",  "x15",
  "x16",  "x17",  "x18",  "x19",  "x20",  "x21",  "x22",  "x23",
  "x24",  "x25",  "x26",  "x27",  "x28",  "x29",  "x30",  "x31"
};

const char * const riscv_gpr_names_abi[NGPR] =
{
  "zero", "ra",   "sp",   "gp",   "tp",   "t0",   "t1",   "t2",
  "s0",   "s1",   "a0",   "a1",   "a2",   "a3",   "a4",   "a5",
  "a6",   "a7",   "s2",   "s3",   "s4",   "s5",   "s6",   "s7",
  "s8",   "s9",   "s10",  "s11",  "t3",   "t4",   "t5",   "t6"
};

const char * const riscv_fpr_names_numeric[NFPR] =
{
  "f0",   "f1",   "f2",   "f3",   "f4",   "f5",   "f6",   "f7",
  "f8",   "f9",   "f10",  "f11",  "f12",  "f13",  "f14",  "f15",
  "f16",  "f17",  "f18",  "f19",  "f20",  "f21",  "f22",  "f23",
  "f24",  "f25",  "f26",  "f27",  "f28",  "f29",  "f30",  "f31"
};

const char * const riscv_fpr_names_abi[NFPR] =
{
  "ft0",  "ft1",  "ft2",  "ft3",  "ft4",  "ft5",  "ft6",  "ft7",
  "fs0",  "fs1",  "fa0",  "fa1",  "fa2",  "fa3",  "fa4",  "fa5",
  "fa6",  "fa7",  "fs2",  "fs3",  "fs4",  "fs5",  "fs6",  "fs7",
  "fs8",  "fs9",  "fs10", "fs11", "ft8",  "ft9",  "ft10", "ft11"
};

/* The order of overloaded instructions matters.  Label arguments and
   register arguments look the same. Instructions that can have either
   for arguments must apear in the correct order in this table for the
   assembler to pick the right one. In other words, entries with
   immediate operands must apear after the same instruction with
   registers.

   Because of the lookup algorithm used, entries with the same opcode
   name must be contiguous.  */

#define MASK_RS1 (OP_MASK_RS1 << OP_SH_RS1)
#define MASK_RS2 (OP_MASK_RS2 << OP_SH_RS2)
#define MASK_RD (OP_MASK_RD << OP_SH_RD)
#define MASK_CRS2 (OP_MASK_CRS2 << OP_SH_CRS2)
#define MASK_IMM ENCODE_ITYPE_IMM (-1U)
#define MASK_RVC_IMM ENCODE_CITYPE_IMM (-1U)
#define MASK_UIMM ENCODE_UTYPE_IMM (-1U)
#define MASK_RM (OP_MASK_RM << OP_SH_RM)
#define MASK_PRED (OP_MASK_PRED << OP_SH_PRED)
#define MASK_SUCC (OP_MASK_SUCC << OP_SH_SUCC)
#define MASK_AQ (OP_MASK_AQ << OP_SH_AQ)
#define MASK_RL (OP_MASK_RL << OP_SH_RL)
#define MASK_AQRL (MASK_AQ | MASK_RL)
#define MASK_SHAMT (OP_MASK_SHAMT << OP_SH_SHAMT)
#define MATCH_SHAMT_REV8_32 (0b11000 << OP_SH_SHAMT)
#define MATCH_SHAMT_REV8_64 (0b111000 << OP_SH_SHAMT)
#define MATCH_SHAMT_ORC_B (0b00111 << OP_SH_SHAMT)

static int
match_opcode (const struct riscv_opcode *op,
	      insn_t insn,
	      int constraints ATTRIBUTE_UNUSED,
	      const char **error ATTRIBUTE_UNUSED)
{
  return ((insn ^ op->match) & op->mask) == 0;
}

static int
match_never (const struct riscv_opcode *op ATTRIBUTE_UNUSED,
	     insn_t insn ATTRIBUTE_UNUSED,
	     int constraints ATTRIBUTE_UNUSED,
	     const char **error ATTRIBUTE_UNUSED)
{
  return 0;
}

static int
match_rs1_eq_rs2 (const struct riscv_opcode *op,
		  insn_t insn,
		  int constraints ATTRIBUTE_UNUSED,
		  const char **error ATTRIBUTE_UNUSED)
{
  int rs1 = (insn & MASK_RS1) >> OP_SH_RS1;
  int rs2 = (insn & MASK_RS2) >> OP_SH_RS2;
  return match_opcode (op, insn, 0, NULL) && rs1 == rs2;
}

static int
match_rd_nonzero (const struct riscv_opcode *op,
		  insn_t insn,
		  int constraints ATTRIBUTE_UNUSED,
		  const char **error ATTRIBUTE_UNUSED)
{
  return match_opcode (op, insn, 0, NULL) && ((insn & MASK_RD) != 0);
}

static int
match_c_add (const struct riscv_opcode *op,
	     insn_t insn,
	     int constraints ATTRIBUTE_UNUSED,
	     const char **error ATTRIBUTE_UNUSED)
{
  return match_rd_nonzero (op, insn, 0, NULL) && ((insn & MASK_CRS2) != 0);
}

/* We don't allow mv zero,X to become a c.mv hint, so we need a separate
   matching function for this.  */

static int
match_c_add_with_hint (const struct riscv_opcode *op, insn_t insn,
		       int constraints ATTRIBUTE_UNUSED,
		       const char **error ATTRIBUTE_UNUSED)
{
  return match_opcode (op, insn, 0, NULL) && ((insn & MASK_CRS2) != 0);
}

static int
match_c_nop (const struct riscv_opcode *op,
	     insn_t insn,
	     int constraints ATTRIBUTE_UNUSED,
	     const char **error ATTRIBUTE_UNUSED)
{
  return (match_opcode (op, insn, 0, NULL)
	  && (((insn & MASK_RD) >> OP_SH_RD) == 0));
}

static int
match_c_addi16sp (const struct riscv_opcode *op,
		  insn_t insn,
		  int constraints ATTRIBUTE_UNUSED,
		  const char **error ATTRIBUTE_UNUSED)
{
  return (match_opcode (op, insn, 0, NULL)
	  && (((insn & MASK_RD) >> OP_SH_RD) == 2)
	  && EXTRACT_CITYPE_ADDI16SP_IMM (insn) != 0);
}

static int
match_c_lui (const struct riscv_opcode *op,
	     insn_t insn,
	     int constraints ATTRIBUTE_UNUSED,
	     const char **error ATTRIBUTE_UNUSED)
{
  return (match_rd_nonzero (op, insn, 0, NULL)
	  && (((insn & MASK_RD) >> OP_SH_RD) != 2)
	  && EXTRACT_CITYPE_LUI_IMM (insn) != 0);
}

/* We don't allow lui zero,X to become a c.lui hint, so we need a separate
   matching function for this.  */

static int
match_c_lui_with_hint (const struct riscv_opcode *op,
		       insn_t insn,
		       int constraints ATTRIBUTE_UNUSED,
		       const char **error ATTRIBUTE_UNUSED)
{
  return (match_opcode (op, insn, 0, NULL)
	  && (((insn & MASK_RD) >> OP_SH_RD) != 2)
	  && EXTRACT_CITYPE_LUI_IMM (insn) != 0);
}

static int
match_c_addi4spn (const struct riscv_opcode *op,
		  insn_t insn,
		  int constraints ATTRIBUTE_UNUSED,
		  const char **error ATTRIBUTE_UNUSED)
{
  return (match_opcode (op, insn, 0, NULL)
	  && EXTRACT_CIWTYPE_ADDI4SPN_IMM (insn) != 0);
}

/* This requires a non-zero shift.  A zero rd is a hint, so is allowed.  */

static int
match_c_slli (const struct riscv_opcode *op,
	      insn_t insn,
	      int constraints ATTRIBUTE_UNUSED,
	      const char **error ATTRIBUTE_UNUSED)
{
  return match_opcode (op, insn, 0, NULL) && EXTRACT_CITYPE_IMM (insn) != 0;
}

/* This requires a non-zero rd, and a non-zero shift.  */

static int
match_slli_as_c_slli (const struct riscv_opcode *op,
		      insn_t insn,
		      int constraints ATTRIBUTE_UNUSED,
		      const char **error ATTRIBUTE_UNUSED)
{
  return (match_rd_nonzero (op, insn, 0, NULL)
	  && EXTRACT_CITYPE_IMM (insn) != 0);
}

/* This requires a zero shift.  A zero rd is a hint, so is allowed.  */

static int
match_c_slli64 (const struct riscv_opcode *op,
		insn_t insn,
		int constraints ATTRIBUTE_UNUSED,
		const char **error ATTRIBUTE_UNUSED)
{
  return match_opcode (op, insn, 0, NULL) && EXTRACT_CITYPE_IMM (insn) == 0;
}

/* This is used for both srli and srai.  This requires a non-zero shift.
   A zero rd is not possible.  */

static int
match_srxi_as_c_srxi (const struct riscv_opcode *op,
		      insn_t insn,
		      int constraints ATTRIBUTE_UNUSED,
		      const char **error ATTRIBUTE_UNUSED)
{
  return match_opcode (op, insn, 0, NULL) && EXTRACT_CITYPE_IMM (insn) != 0;
}

const struct riscv_opcode riscv_opcodes[] =
{
/* name, xlen, isa, operands, match, mask, match_func, pinfo.  */
{"unimp",       0, INSN_CLASS_C, "",          0, 0xffffU, match_opcode, INSN_ALIAS },
{"unimp",       0, INSN_CLASS_I, "",          MATCH_CSRRW|(CSR_CYCLE << OP_SH_CSR), 0xffffffffU,  match_opcode, 0 }, /* csrw cycle, x0  */
{"ebreak",      0, INSN_CLASS_C, "",          MATCH_C_EBREAK, MASK_C_EBREAK, match_opcode, INSN_ALIAS },
{"ebreak",      0, INSN_CLASS_I, "",          MATCH_EBREAK, MASK_EBREAK, match_opcode, 0 },
{"sbreak",      0, INSN_CLASS_C, "",          MATCH_C_EBREAK, MASK_C_EBREAK, match_opcode, INSN_ALIAS },
{"sbreak",      0, INSN_CLASS_I, "",          MATCH_EBREAK, MASK_EBREAK, match_opcode, INSN_ALIAS },
{"ret",         0, INSN_CLASS_C, "",          MATCH_C_JR|(X_RA << OP_SH_RD), MASK_C_JR|MASK_RD, match_opcode, INSN_ALIAS|INSN_BRANCH },
{"ret",         0, INSN_CLASS_I, "",          MATCH_JALR|(X_RA << OP_SH_RS1), MASK_JALR|MASK_RD|MASK_RS1|MASK_IMM, match_opcode, INSN_ALIAS|INSN_BRANCH },
{"jr",          0, INSN_CLASS_C, "d",         MATCH_C_JR, MASK_C_JR, match_rd_nonzero, INSN_ALIAS|INSN_BRANCH },
{"jr",          0, INSN_CLASS_I, "s",         MATCH_JALR, MASK_JALR|MASK_RD|MASK_IMM, match_opcode, INSN_ALIAS|INSN_BRANCH },
{"jr",          0, INSN_CLASS_I, "o(s)",      MATCH_JALR, MASK_JALR|MASK_RD, match_opcode, INSN_ALIAS|INSN_BRANCH },
{"jr",          0, INSN_CLASS_I, "s,j",       MATCH_JALR, MASK_JALR|MASK_RD, match_opcode, INSN_ALIAS|INSN_BRANCH },
{"jalr",        0, INSN_CLASS_C, "d",         MATCH_C_JALR, MASK_C_JALR, match_rd_nonzero, INSN_ALIAS|INSN_JSR },
{"jalr",        0, INSN_CLASS_I, "s",         MATCH_JALR|(X_RA << OP_SH_RD), MASK_JALR|MASK_RD|MASK_IMM, match_opcode, INSN_ALIAS|INSN_JSR },
{"jalr",        0, INSN_CLASS_I, "o(s)",      MATCH_JALR|(X_RA << OP_SH_RD), MASK_JALR|MASK_RD, match_opcode, INSN_ALIAS|INSN_JSR },
{"jalr",        0, INSN_CLASS_I, "s,j",       MATCH_JALR|(X_RA << OP_SH_RD), MASK_JALR|MASK_RD, match_opcode, INSN_ALIAS|INSN_JSR },
{"jalr",        0, INSN_CLASS_I, "d,s",       MATCH_JALR, MASK_JALR|MASK_IMM, match_opcode, INSN_ALIAS|INSN_JSR },
{"jalr",        0, INSN_CLASS_I, "d,o(s)",    MATCH_JALR, MASK_JALR, match_opcode, INSN_JSR },
{"jalr",        0, INSN_CLASS_I, "d,s,j",     MATCH_JALR, MASK_JALR, match_opcode, INSN_JSR },
{"j",           0, INSN_CLASS_C, "Ca",        MATCH_C_J, MASK_C_J, match_opcode, INSN_ALIAS|INSN_BRANCH },
{"j",           0, INSN_CLASS_I, "a",         MATCH_JAL, MASK_JAL|MASK_RD, match_opcode, INSN_ALIAS|INSN_BRANCH },
{"jal",         0, INSN_CLASS_I, "d,a",       MATCH_JAL, MASK_JAL, match_opcode, INSN_JSR },
{"jal",        32, INSN_CLASS_C, "Ca",        MATCH_C_JAL, MASK_C_JAL, match_opcode, INSN_ALIAS|INSN_JSR },
{"jal",         0, INSN_CLASS_I, "a",         MATCH_JAL|(X_RA << OP_SH_RD), MASK_JAL|MASK_RD, match_opcode, INSN_ALIAS|INSN_JSR },
{"call",        0, INSN_CLASS_I, "d,c",       (X_T1 << OP_SH_RS1), (int) M_CALL, match_never, INSN_MACRO },
{"call",        0, INSN_CLASS_I, "c",         (X_RA << OP_SH_RS1)|(X_RA << OP_SH_RD), (int) M_CALL, match_never, INSN_MACRO },
{"tail",        0, INSN_CLASS_I, "c",         (X_T1 << OP_SH_RS1), (int) M_CALL, match_never, INSN_MACRO },
{"jump",        0, INSN_CLASS_I, "c,s",       0, (int) M_CALL, match_never, INSN_MACRO },
{"nop",         0, INSN_CLASS_C, "",          MATCH_C_ADDI, 0xffff, match_opcode, INSN_ALIAS },
{"nop",         0, INSN_CLASS_I, "",          MATCH_ADDI, MASK_ADDI|MASK_RD|MASK_RS1|MASK_IMM, match_opcode, INSN_ALIAS },
{"lui",         0, INSN_CLASS_C, "d,Cu",      MATCH_C_LUI, MASK_C_LUI, match_c_lui, INSN_ALIAS },
{"lui",         0, INSN_CLASS_I, "d,u",       MATCH_LUI, MASK_LUI, match_opcode, 0 },
{"li",          0, INSN_CLASS_C, "d,Cv",      MATCH_C_LUI, MASK_C_LUI, match_c_lui, INSN_ALIAS },
{"li",          0, INSN_CLASS_C, "d,Co",      MATCH_C_LI, MASK_C_LI, match_rd_nonzero, INSN_ALIAS },
{"li",          0, INSN_CLASS_I, "d,j",       MATCH_ADDI, MASK_ADDI|MASK_RS1, match_opcode, INSN_ALIAS }, /* addi  */
{"li",          0, INSN_CLASS_I, "d,I",       0, (int) M_LI,  match_never, INSN_MACRO },
{"mv",          0, INSN_CLASS_C, "d,CV",      MATCH_C_MV, MASK_C_MV, match_c_add, INSN_ALIAS },
{"mv",          0, INSN_CLASS_I, "d,s",       MATCH_ADDI, MASK_ADDI|MASK_IMM, match_opcode, INSN_ALIAS },
{"move",        0, INSN_CLASS_C, "d,CV",      MATCH_C_MV, MASK_C_MV, match_c_add, INSN_ALIAS },
{"move",        0, INSN_CLASS_I, "d,s",       MATCH_ADDI, MASK_ADDI|MASK_IMM, match_opcode, INSN_ALIAS },
{"zext.b",      0, INSN_CLASS_I, "d,s",       MATCH_ANDI|ENCODE_ITYPE_IMM (255), MASK_ANDI | MASK_IMM, match_opcode, INSN_ALIAS },
{"andi",        0, INSN_CLASS_C, "Cs,Cw,Co",  MATCH_C_ANDI, MASK_C_ANDI, match_opcode, INSN_ALIAS },
{"andi",        0, INSN_CLASS_I, "d,s,j",     MATCH_ANDI, MASK_ANDI, match_opcode, 0 },
{"and",         0, INSN_CLASS_C, "Cs,Cw,Ct",  MATCH_C_AND, MASK_C_AND, match_opcode, INSN_ALIAS },
{"and",         0, INSN_CLASS_C, "Cs,Ct,Cw",  MATCH_C_AND, MASK_C_AND, match_opcode, INSN_ALIAS },
{"and",         0, INSN_CLASS_C, "Cs,Cw,Co",  MATCH_C_ANDI, MASK_C_ANDI, match_opcode, INSN_ALIAS },
{"and",         0, INSN_CLASS_I, "d,s,t",     MATCH_AND, MASK_AND, match_opcode, 0 },
{"and",         0, INSN_CLASS_I, "d,s,j",     MATCH_ANDI, MASK_ANDI, match_opcode, INSN_ALIAS },
{"beqz",        0, INSN_CLASS_C, "Cs,Cp",     MATCH_C_BEQZ, MASK_C_BEQZ, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"beqz",        0, INSN_CLASS_I, "s,p",       MATCH_BEQ, MASK_BEQ|MASK_RS2, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"beq",         0, INSN_CLASS_C, "Cs,Cz,Cp",  MATCH_C_BEQZ, MASK_C_BEQZ, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"beq",         0, INSN_CLASS_I, "s,t,p",     MATCH_BEQ, MASK_BEQ, match_opcode, INSN_CONDBRANCH },
{"blez",        0, INSN_CLASS_I, "t,p",       MATCH_BGE, MASK_BGE|MASK_RS1, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"bgez",        0, INSN_CLASS_I, "s,p",       MATCH_BGE, MASK_BGE|MASK_RS2, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"bge",         0, INSN_CLASS_I, "s,t,p",     MATCH_BGE, MASK_BGE, match_opcode, INSN_CONDBRANCH },
{"bgeu",        0, INSN_CLASS_I, "s,t,p",     MATCH_BGEU, MASK_BGEU, match_opcode, INSN_CONDBRANCH },
{"ble",         0, INSN_CLASS_I, "t,s,p",     MATCH_BGE, MASK_BGE, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"bleu",        0, INSN_CLASS_I, "t,s,p",     MATCH_BGEU, MASK_BGEU, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"bltz",        0, INSN_CLASS_I, "s,p",       MATCH_BLT, MASK_BLT|MASK_RS2, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"bgtz",        0, INSN_CLASS_I, "t,p",       MATCH_BLT, MASK_BLT|MASK_RS1, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"blt",         0, INSN_CLASS_I, "s,t,p",     MATCH_BLT, MASK_BLT, match_opcode, INSN_CONDBRANCH },
{"bltu",        0, INSN_CLASS_I, "s,t,p",     MATCH_BLTU, MASK_BLTU, match_opcode, INSN_CONDBRANCH },
{"bgt",         0, INSN_CLASS_I, "t,s,p",     MATCH_BLT, MASK_BLT, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"bgtu",        0, INSN_CLASS_I, "t,s,p",     MATCH_BLTU, MASK_BLTU, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"bnez",        0, INSN_CLASS_C, "Cs,Cp",     MATCH_C_BNEZ, MASK_C_BNEZ, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"bnez",        0, INSN_CLASS_I, "s,p",       MATCH_BNE, MASK_BNE|MASK_RS2, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"bne",         0, INSN_CLASS_C, "Cs,Cz,Cp",  MATCH_C_BNEZ, MASK_C_BNEZ, match_opcode, INSN_ALIAS|INSN_CONDBRANCH },
{"bne",         0, INSN_CLASS_I, "s,t,p",     MATCH_BNE, MASK_BNE, match_opcode, INSN_CONDBRANCH },
{"addi",        0, INSN_CLASS_C, "Ct,Cc,CK",  MATCH_C_ADDI4SPN, MASK_C_ADDI4SPN, match_c_addi4spn, INSN_ALIAS },
{"addi",        0, INSN_CLASS_C, "d,CU,Cj",   MATCH_C_ADDI, MASK_C_ADDI, match_rd_nonzero, INSN_ALIAS },
{"addi",        0, INSN_CLASS_C, "d,CU,z",    MATCH_C_NOP, MASK_C_ADDI|MASK_RVC_IMM, match_c_nop, INSN_ALIAS },
{"addi",        0, INSN_CLASS_C, "Cc,Cc,CL",  MATCH_C_ADDI16SP, MASK_C_ADDI16SP, match_c_addi16sp, INSN_ALIAS },
{"addi",        0, INSN_CLASS_C, "d,Cz,Co",   MATCH_C_LI, MASK_C_LI, match_rd_nonzero, INSN_ALIAS },
{"addi",        0, INSN_CLASS_C, "d,CV,z",      MATCH_C_MV, MASK_C_MV, match_c_add, INSN_ALIAS },
{"addi",        0, INSN_CLASS_I, "d,s,j",     MATCH_ADDI, MASK_ADDI, match_opcode, 0 },
{"add",         0, INSN_CLASS_C, "d,CU,CV",   MATCH_C_ADD, MASK_C_ADD, match_c_add, INSN_ALIAS },
{"add",         0, INSN_CLASS_C, "d,CV,CU",   MATCH_C_ADD, MASK_C_ADD, match_c_add, INSN_ALIAS },
{"add",         0, INSN_CLASS_C, "d,CU,Co",   MATCH_C_ADDI, MASK_C_ADDI, match_rd_nonzero, INSN_ALIAS },
{"add",         0, INSN_CLASS_C, "Ct,Cc,CK",  MATCH_C_ADDI4SPN, MASK_C_ADDI4SPN, match_c_addi4spn, INSN_ALIAS },
{"add",         0, INSN_CLASS_C, "Cc,Cc,CL",  MATCH_C_ADDI16SP, MASK_C_ADDI16SP, match_c_addi16sp, INSN_ALIAS },
{"add",         0, INSN_CLASS_C, "d,Cz,CV",   MATCH_C_MV, MASK_C_MV, match_c_add, INSN_ALIAS },
{"add",         0, INSN_CLASS_I, "d,s,t",     MATCH_ADD, MASK_ADD, match_opcode, 0 },
{"add",         0, INSN_CLASS_I, "d,s,t,1",   MATCH_ADD, MASK_ADD, match_opcode, 0 },
{"add",         0, INSN_CLASS_I, "d,s,j",     MATCH_ADDI, MASK_ADDI, match_opcode, INSN_ALIAS },
{"la",          0, INSN_CLASS_I, "d,B",       0, (int) M_LA, match_never, INSN_MACRO },
{"lla",         0, INSN_CLASS_I, "d,B",       0, (int) M_LLA, match_never, INSN_MACRO },
{"la.tls.gd",   0, INSN_CLASS_I, "d,A",       0, (int) M_LA_TLS_GD, match_never, INSN_MACRO },
{"la.tls.ie",   0, INSN_CLASS_I, "d,A",       0, (int) M_LA_TLS_IE, match_never, INSN_MACRO },
{"neg",         0, INSN_CLASS_I, "d,t",       MATCH_SUB, MASK_SUB|MASK_RS1, match_opcode, INSN_ALIAS }, /* sub 0  */
{"slli",        0, INSN_CLASS_C, "d,CU,C>",   MATCH_C_SLLI, MASK_C_SLLI, match_slli_as_c_slli, INSN_ALIAS },
{"slli",        0, INSN_CLASS_I, "d,s,>",     MATCH_SLLI, MASK_SLLI, match_opcode, 0 },
{"sll",         0, INSN_CLASS_C, "d,CU,C>",   MATCH_C_SLLI, MASK_C_SLLI, match_slli_as_c_slli, INSN_ALIAS },
{"sll",         0, INSN_CLASS_I, "d,s,t",     MATCH_SLL, MASK_SLL, match_opcode, 0 },
{"sll",         0, INSN_CLASS_I, "d,s,>",     MATCH_SLLI, MASK_SLLI, match_opcode, INSN_ALIAS },
{"srli",        0, INSN_CLASS_C, "Cs,Cw,C>",  MATCH_C_SRLI, MASK_C_SRLI, match_srxi_as_c_srxi, INSN_ALIAS },
{"srli",        0, INSN_CLASS_I, "d,s,>",     MATCH_SRLI, MASK_SRLI, match_opcode, 0 },
{"srl",         0, INSN_CLASS_C, "Cs,Cw,C>",  MATCH_C_SRLI, MASK_C_SRLI, match_srxi_as_c_srxi, INSN_ALIAS },
{"srl",         0, INSN_CLASS_I, "d,s,t",     MATCH_SRL, MASK_SRL, match_opcode, 0 },
{"srl",         0, INSN_CLASS_I, "d,s,>",     MATCH_SRLI, MASK_SRLI, match_opcode, INSN_ALIAS },
{"srai",        0, INSN_CLASS_C, "Cs,Cw,C>",  MATCH_C_SRAI, MASK_C_SRAI, match_srxi_as_c_srxi, INSN_ALIAS },
{"srai",        0, INSN_CLASS_I, "d,s,>",     MATCH_SRAI, MASK_SRAI, match_opcode, 0 },
{"sra",         0, INSN_CLASS_C, "Cs,Cw,C>",  MATCH_C_SRAI, MASK_C_SRAI, match_srxi_as_c_srxi, INSN_ALIAS },
{"sra",         0, INSN_CLASS_I, "d,s,t",     MATCH_SRA, MASK_SRA, match_opcode, 0 },
{"sra",         0, INSN_CLASS_I, "d,s,>",     MATCH_SRAI, MASK_SRAI, match_opcode, INSN_ALIAS },
{"sub",         0, INSN_CLASS_C, "Cs,Cw,Ct",  MATCH_C_SUB, MASK_C_SUB, match_opcode, INSN_ALIAS },
{"sub",         0, INSN_CLASS_I, "d,s,t",     MATCH_SUB, MASK_SUB, match_opcode, 0 },
{"lb",          0, INSN_CLASS_I, "d,o(s)",    MATCH_LB, MASK_LB, match_opcode, INSN_DREF|INSN_1_BYTE },
{"lb",          0, INSN_CLASS_I, "d,A",       0, (int) M_LB, match_never, INSN_MACRO },
{"lbu",         0, INSN_CLASS_I, "d,o(s)",    MATCH_LBU, MASK_LBU, match_opcode, INSN_DREF|INSN_1_BYTE },
{"lbu",         0, INSN_CLASS_I, "d,A",       0, (int) M_LBU, match_never, INSN_MACRO },
{"lh",          0, INSN_CLASS_I, "d,o(s)",    MATCH_LH, MASK_LH, match_opcode, INSN_DREF|INSN_2_BYTE },
{"lh",          0, INSN_CLASS_I, "d,A",       0, (int) M_LH, match_never, INSN_MACRO },
{"lhu",         0, INSN_CLASS_I, "d,o(s)",    MATCH_LHU, MASK_LHU, match_opcode, INSN_DREF|INSN_2_BYTE },
{"lhu",         0, INSN_CLASS_I, "d,A",       0, (int) M_LHU, match_never, INSN_MACRO },
{"lw",          0, INSN_CLASS_C, "d,Cm(Cc)",  MATCH_C_LWSP, MASK_C_LWSP, match_rd_nonzero, INSN_ALIAS|INSN_DREF|INSN_4_BYTE },
{"lw",          0, INSN_CLASS_C, "Ct,Ck(Cs)", MATCH_C_LW, MASK_C_LW, match_opcode, INSN_ALIAS|INSN_DREF|INSN_4_BYTE },
{"lw",          0, INSN_CLASS_I, "d,o(s)",    MATCH_LW, MASK_LW, match_opcode, INSN_DREF|INSN_4_BYTE },
{"lw",          0, INSN_CLASS_I, "d,A",       0, (int) M_LW, match_never, INSN_MACRO },
{"not",         0, INSN_CLASS_I, "d,s",       MATCH_XORI|MASK_IMM, MASK_XORI|MASK_IMM, match_opcode, INSN_ALIAS },
{"ori",         0, INSN_CLASS_I, "d,s,j",     MATCH_ORI, MASK_ORI, match_opcode, 0 },
{"or",          0, INSN_CLASS_C, "Cs,Cw,Ct",  MATCH_C_OR, MASK_C_OR, match_opcode, INSN_ALIAS },
{"or",          0, INSN_CLASS_C, "Cs,Ct,Cw",  MATCH_C_OR, MASK_C_OR, match_opcode, INSN_ALIAS },
{"or",          0, INSN_CLASS_I, "d,s,t",     MATCH_OR, MASK_OR, match_opcode, 0 },
{"or",          0, INSN_CLASS_I, "d,s,j",     MATCH_ORI, MASK_ORI, match_opcode, INSN_ALIAS },
{"auipc",       0, INSN_CLASS_I, "d,u",       MATCH_AUIPC, MASK_AUIPC, match_opcode, 0 },
{"seqz",        0, INSN_CLASS_I, "d,s",       MATCH_SLTIU|ENCODE_ITYPE_IMM (1), MASK_SLTIU | MASK_IMM, match_opcode, INSN_ALIAS },
{"snez",        0, INSN_CLASS_I, "d,t",       MATCH_SLTU, MASK_SLTU|MASK_RS1, match_opcode, INSN_ALIAS },
{"sltz",        0, INSN_CLASS_I, "d,s",       MATCH_SLT, MASK_SLT|MASK_RS2, match_opcode, INSN_ALIAS },
{"sgtz",        0, INSN_CLASS_I, "d,t",       MATCH_SLT, MASK_SLT|MASK_RS1, match_opcode, INSN_ALIAS },
{"slti",        0, INSN_CLASS_I, "d,s,j",     MATCH_SLTI, MASK_SLTI, match_opcode, 0 },
{"slt",         0, INSN_CLASS_I, "d,s,t",     MATCH_SLT, MASK_SLT, match_opcode, 0 },
{"slt",         0, INSN_CLASS_I, "d,s,j",     MATCH_SLTI, MASK_SLTI, match_opcode, INSN_ALIAS },
{"sltiu",       0, INSN_CLASS_I, "d,s,j",     MATCH_SLTIU, MASK_SLTIU, match_opcode, 0 },
{"sltu",        0, INSN_CLASS_I, "d,s,t",     MATCH_SLTU, MASK_SLTU, match_opcode, 0 },
{"sltu",        0, INSN_CLASS_I, "d,s,j",     MATCH_SLTIU, MASK_SLTIU, match_opcode, INSN_ALIAS },
{"sgt",         0, INSN_CLASS_I, "d,t,s",     MATCH_SLT, MASK_SLT, match_opcode, INSN_ALIAS },
{"sgtu",        0, INSN_CLASS_I, "d,t,s",     MATCH_SLTU, MASK_SLTU, match_opcode, INSN_ALIAS },
{"sb",          0, INSN_CLASS_I, "t,q(s)",    MATCH_SB, MASK_SB, match_opcode, INSN_DREF|INSN_1_BYTE },
{"sb",          0, INSN_CLASS_I, "t,A,s",     0, (int) M_SB, match_never, INSN_MACRO },
{"sh",          0, INSN_CLASS_I, "t,q(s)",    MATCH_SH, MASK_SH, match_opcode, INSN_DREF|INSN_2_BYTE },
{"sh",          0, INSN_CLASS_I, "t,A,s",     0, (int) M_SH, match_never, INSN_MACRO },
{"sw",          0, INSN_CLASS_C, "CV,CM(Cc)", MATCH_C_SWSP, MASK_C_SWSP, match_opcode, INSN_ALIAS|INSN_DREF|INSN_4_BYTE },
{"sw",          0, INSN_CLASS_C, "Ct,Ck(Cs)", MATCH_C_SW, MASK_C_SW, match_opcode, INSN_ALIAS|INSN_DREF|INSN_4_BYTE },
{"sw",          0, INSN_CLASS_I, "t,q(s)",    MATCH_SW, MASK_SW, match_opcode, INSN_DREF|INSN_4_BYTE },
{"sw",          0, INSN_CLASS_I, "t,A,s",     0, (int) M_SW, match_never, INSN_MACRO },
{"pause",       0, INSN_CLASS_ZIHINTPAUSE, "",MATCH_PAUSE, MASK_PAUSE, match_opcode, 0 },
{"fence",       0, INSN_CLASS_I, "",          MATCH_FENCE|MASK_PRED|MASK_SUCC, MASK_FENCE|MASK_RD|MASK_RS1|MASK_IMM, match_opcode, INSN_ALIAS },
{"fence",       0, INSN_CLASS_I, "P,Q",       MATCH_FENCE, MASK_FENCE|MASK_RD|MASK_RS1|(MASK_IMM & ~MASK_PRED & ~MASK_SUCC), match_opcode, 0 },
{"fence.i",     0, INSN_CLASS_ZIFENCEI, "",   MATCH_FENCE_I, MASK_FENCE|MASK_RD|MASK_RS1|MASK_IMM, match_opcode, 0 },
{"fence.tso",   0, INSN_CLASS_I, "",          MATCH_FENCE_TSO, MASK_FENCE_TSO|MASK_RD|MASK_RS1, match_opcode, INSN_ALIAS },
{"rdcycle",     0, INSN_CLASS_I, "d",         MATCH_RDCYCLE, MASK_RDCYCLE, match_opcode, INSN_ALIAS },
{"rdinstret",   0, INSN_CLASS_I, "d",         MATCH_RDINSTRET, MASK_RDINSTRET, match_opcode, INSN_ALIAS },
{"rdtime",      0, INSN_CLASS_I, "d",         MATCH_RDTIME, MASK_RDTIME, match_opcode, INSN_ALIAS },
{"rdcycleh",   32, INSN_CLASS_I, "d",         MATCH_RDCYCLEH, MASK_RDCYCLEH, match_opcode, INSN_ALIAS },
{"rdinstreth", 32, INSN_CLASS_I, "d",         MATCH_RDINSTRETH, MASK_RDINSTRETH, match_opcode, INSN_ALIAS },
{"rdtimeh",    32, INSN_CLASS_I, "d",         MATCH_RDTIMEH, MASK_RDTIMEH, match_opcode, INSN_ALIAS },
{"ecall",       0, INSN_CLASS_I, "",          MATCH_SCALL, MASK_SCALL, match_opcode, 0 },
{"scall",       0, INSN_CLASS_I, "",          MATCH_SCALL, MASK_SCALL, match_opcode, 0 },
{"xori",        0, INSN_CLASS_I, "d,s,j",     MATCH_XORI, MASK_XORI, match_opcode, 0 },
{"xor",         0, INSN_CLASS_C, "Cs,Cw,Ct",  MATCH_C_XOR, MASK_C_XOR, match_opcode, INSN_ALIAS },
{"xor",         0, INSN_CLASS_C, "Cs,Ct,Cw",  MATCH_C_XOR, MASK_C_XOR, match_opcode, INSN_ALIAS },
{"xor",         0, INSN_CLASS_I, "d,s,t",     MATCH_XOR, MASK_XOR, match_opcode, 0 },
{"xor",         0, INSN_CLASS_I, "d,s,j",     MATCH_XORI, MASK_XORI, match_opcode, INSN_ALIAS },
{"lwu",        64, INSN_CLASS_I, "d,o(s)",    MATCH_LWU, MASK_LWU, match_opcode, INSN_DREF|INSN_4_BYTE },
{"lwu",        64, INSN_CLASS_I, "d,A",       0, (int) M_LWU, match_never, INSN_MACRO },
{"ld",         64, INSN_CLASS_C, "d,Cn(Cc)",  MATCH_C_LDSP, MASK_C_LDSP, match_rd_nonzero, INSN_ALIAS|INSN_DREF|INSN_8_BYTE },
{"ld",         64, INSN_CLASS_C, "Ct,Cl(Cs)", MATCH_C_LD, MASK_C_LD, match_opcode, INSN_ALIAS|INSN_DREF|INSN_8_BYTE },
{"ld",         64, INSN_CLASS_I, "d,o(s)",    MATCH_LD, MASK_LD, match_opcode, INSN_DREF|INSN_8_BYTE },
{"ld",         64, INSN_CLASS_I, "d,A",       0, (int) M_LD, match_never, INSN_MACRO },
{"sd",         64, INSN_CLASS_C, "CV,CN(Cc)", MATCH_C_SDSP, MASK_C_SDSP, match_opcode, INSN_ALIAS|INSN_DREF|INSN_8_BYTE },
{"sd",         64, INSN_CLASS_C, "Ct,Cl(Cs)", MATCH_C_SD, MASK_C_SD, match_opcode, INSN_ALIAS|INSN_DREF|INSN_8_BYTE },
{"sd",         64, INSN_CLASS_I, "t,q(s)",    MATCH_SD, MASK_SD, match_opcode, INSN_DREF|INSN_8_BYTE },
{"sd",         64, INSN_CLASS_I, "t,A,s",     0, (int) M_SD, match_never, INSN_MACRO },
{"sext.w",     64, INSN_CLASS_C, "d,CU",      MATCH_C_ADDIW, MASK_C_ADDIW|MASK_RVC_IMM, match_rd_nonzero, INSN_ALIAS },
{"sext.w",     64, INSN_CLASS_I, "d,s",       MATCH_ADDIW, MASK_ADDIW|MASK_IMM, match_opcode, INSN_ALIAS },
{"addiw",      64, INSN_CLASS_C, "d,CU,Co",   MATCH_C_ADDIW, MASK_C_ADDIW, match_rd_nonzero, INSN_ALIAS },
{"addiw",      64, INSN_CLASS_I, "d,s,j",     MATCH_ADDIW, MASK_ADDIW, match_opcode, 0 },
{"addw",       64, INSN_CLASS_C, "Cs,Cw,Ct",  MATCH_C_ADDW, MASK_C_ADDW, match_opcode, INSN_ALIAS },
{"addw",       64, INSN_CLASS_C, "Cs,Ct,Cw",  MATCH_C_ADDW, MASK_C_ADDW, match_opcode, INSN_ALIAS },
{"addw",       64, INSN_CLASS_C, "d,CU,Co",   MATCH_C_ADDIW, MASK_C_ADDIW, match_rd_nonzero, INSN_ALIAS },
{"addw",       64, INSN_CLASS_I, "d,s,t",     MATCH_ADDW, MASK_ADDW, match_opcode, 0 },
{"addw",       64, INSN_CLASS_I, "d,s,j",     MATCH_ADDIW, MASK_ADDIW, match_opcode, INSN_ALIAS },
{"negw",       64, INSN_CLASS_I, "d,t",       MATCH_SUBW, MASK_SUBW|MASK_RS1, match_opcode, INSN_ALIAS }, /* sub 0  */
{"slliw",      64, INSN_CLASS_I, "d,s,<",     MATCH_SLLIW, MASK_SLLIW, match_opcode, 0 },
{"sllw",       64, INSN_CLASS_I, "d,s,t",     MATCH_SLLW, MASK_SLLW, match_opcode, 0 },
{"sllw",       64, INSN_CLASS_I, "d,s,<",     MATCH_SLLIW, MASK_SLLIW, match_opcode, INSN_ALIAS },
{"srliw",      64, INSN_CLASS_I, "d,s,<",     MATCH_SRLIW, MASK_SRLIW, match_opcode, 0 },
{"srlw",       64, INSN_CLASS_I, "d,s,t",     MATCH_SRLW, MASK_SRLW, match_opcode, 0 },
{"srlw",       64, INSN_CLASS_I, "d,s,<",     MATCH_SRLIW, MASK_SRLIW, match_opcode, INSN_ALIAS },
{"sraiw",      64, INSN_CLASS_I, "d,s,<",     MATCH_SRAIW, MASK_SRAIW, match_opcode, 0 },
{"sraw",       64, INSN_CLASS_I, "d,s,t",     MATCH_SRAW, MASK_SRAW, match_opcode, 0 },
{"sraw",       64, INSN_CLASS_I, "d,s,<",     MATCH_SRAIW, MASK_SRAIW, match_opcode, INSN_ALIAS },
{"subw",       64, INSN_CLASS_C, "Cs,Cw,Ct",  MATCH_C_SUBW, MASK_C_SUBW, match_opcode, INSN_ALIAS },
{"subw",       64, INSN_CLASS_I, "d,s,t",     MATCH_SUBW, MASK_SUBW, match_opcode, 0 },

/* Atomic memory operation instruction subset.  */
{"lr.w",            0, INSN_CLASS_A, "d,0(s)",   MATCH_LR_W, MASK_LR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"sc.w",            0, INSN_CLASS_A, "d,t,0(s)", MATCH_SC_W, MASK_SC_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoadd.w",        0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOADD_W, MASK_AMOADD_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoswap.w",       0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOSWAP_W, MASK_AMOSWAP_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoand.w",        0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOAND_W, MASK_AMOAND_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoor.w",         0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOOR_W, MASK_AMOOR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoxor.w",        0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOXOR_W, MASK_AMOXOR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomax.w",        0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAX_W, MASK_AMOMAX_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomaxu.w",       0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAXU_W, MASK_AMOMAXU_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomin.w",        0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMIN_W, MASK_AMOMIN_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amominu.w",       0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMINU_W, MASK_AMOMINU_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"lr.w.aq",         0, INSN_CLASS_A, "d,0(s)",   MATCH_LR_W|MASK_AQ, MASK_LR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"sc.w.aq",         0, INSN_CLASS_A, "d,t,0(s)", MATCH_SC_W|MASK_AQ, MASK_SC_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoadd.w.aq",     0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOADD_W|MASK_AQ, MASK_AMOADD_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoswap.w.aq",    0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOSWAP_W|MASK_AQ, MASK_AMOSWAP_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoand.w.aq",     0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOAND_W|MASK_AQ, MASK_AMOAND_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoor.w.aq",      0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOOR_W|MASK_AQ, MASK_AMOOR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoxor.w.aq",     0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOXOR_W|MASK_AQ, MASK_AMOXOR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomax.w.aq",     0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAX_W|MASK_AQ, MASK_AMOMAX_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomaxu.w.aq",    0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAXU_W|MASK_AQ, MASK_AMOMAXU_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomin.w.aq",     0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMIN_W|MASK_AQ, MASK_AMOMIN_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amominu.w.aq",    0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMINU_W|MASK_AQ, MASK_AMOMINU_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"lr.w.rl",         0, INSN_CLASS_A, "d,0(s)",   MATCH_LR_W|MASK_RL, MASK_LR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"sc.w.rl",         0, INSN_CLASS_A, "d,t,0(s)", MATCH_SC_W|MASK_RL, MASK_SC_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoadd.w.rl",     0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOADD_W|MASK_RL, MASK_AMOADD_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoswap.w.rl",    0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOSWAP_W|MASK_RL, MASK_AMOSWAP_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoand.w.rl",     0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOAND_W|MASK_RL, MASK_AMOAND_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoor.w.rl",      0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOOR_W|MASK_RL, MASK_AMOOR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoxor.w.rl",     0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOXOR_W|MASK_RL, MASK_AMOXOR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomax.w.rl",     0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAX_W|MASK_RL, MASK_AMOMAX_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomaxu.w.rl",    0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAXU_W|MASK_RL, MASK_AMOMAXU_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomin.w.rl",     0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMIN_W|MASK_RL, MASK_AMOMIN_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amominu.w.rl",    0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMINU_W|MASK_RL, MASK_AMOMINU_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"lr.w.aqrl",       0, INSN_CLASS_A, "d,0(s)",   MATCH_LR_W|MASK_AQRL, MASK_LR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"sc.w.aqrl",       0, INSN_CLASS_A, "d,t,0(s)", MATCH_SC_W|MASK_AQRL, MASK_SC_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoadd.w.aqrl",   0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOADD_W|MASK_AQRL, MASK_AMOADD_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoswap.w.aqrl",  0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOSWAP_W|MASK_AQRL, MASK_AMOSWAP_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoand.w.aqrl",   0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOAND_W|MASK_AQRL, MASK_AMOAND_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoor.w.aqrl",    0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOOR_W|MASK_AQRL, MASK_AMOOR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amoxor.w.aqrl",   0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOXOR_W|MASK_AQRL, MASK_AMOXOR_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomax.w.aqrl",   0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAX_W|MASK_AQRL, MASK_AMOMAX_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomaxu.w.aqrl",  0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAXU_W|MASK_AQRL, MASK_AMOMAXU_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amomin.w.aqrl",   0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMIN_W|MASK_AQRL, MASK_AMOMIN_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"amominu.w.aqrl",  0, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMINU_W|MASK_AQRL, MASK_AMOMINU_W|MASK_AQRL, match_opcode, INSN_DREF|INSN_4_BYTE },
{"lr.d",           64, INSN_CLASS_A, "d,0(s)",   MATCH_LR_D, MASK_LR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"sc.d",           64, INSN_CLASS_A, "d,t,0(s)", MATCH_SC_D, MASK_SC_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoadd.d",       64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOADD_D, MASK_AMOADD_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoswap.d",      64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOSWAP_D, MASK_AMOSWAP_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoand.d",       64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOAND_D, MASK_AMOAND_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoor.d",        64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOOR_D, MASK_AMOOR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoxor.d",       64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOXOR_D, MASK_AMOXOR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomax.d",       64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAX_D, MASK_AMOMAX_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomaxu.d",      64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAXU_D, MASK_AMOMAXU_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomin.d",       64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMIN_D, MASK_AMOMIN_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amominu.d",      64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMINU_D, MASK_AMOMINU_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"lr.d.aq",        64, INSN_CLASS_A, "d,0(s)",   MATCH_LR_D|MASK_AQ, MASK_LR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"sc.d.aq",        64, INSN_CLASS_A, "d,t,0(s)", MATCH_SC_D|MASK_AQ, MASK_SC_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoadd.d.aq",    64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOADD_D|MASK_AQ, MASK_AMOADD_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoswap.d.aq",   64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOSWAP_D|MASK_AQ, MASK_AMOSWAP_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoand.d.aq",    64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOAND_D|MASK_AQ, MASK_AMOAND_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoor.d.aq",     64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOOR_D|MASK_AQ, MASK_AMOOR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoxor.d.aq",    64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOXOR_D|MASK_AQ, MASK_AMOXOR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomax.d.aq",    64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAX_D|MASK_AQ, MASK_AMOMAX_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomaxu.d.aq",   64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAXU_D|MASK_AQ, MASK_AMOMAXU_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomin.d.aq",    64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMIN_D|MASK_AQ, MASK_AMOMIN_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amominu.d.aq",   64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMINU_D|MASK_AQ, MASK_AMOMINU_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"lr.d.rl",        64, INSN_CLASS_A, "d,0(s)",   MATCH_LR_D|MASK_RL, MASK_LR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"sc.d.rl",        64, INSN_CLASS_A, "d,t,0(s)", MATCH_SC_D|MASK_RL, MASK_SC_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoadd.d.rl",    64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOADD_D|MASK_RL, MASK_AMOADD_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoswap.d.rl",   64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOSWAP_D|MASK_RL, MASK_AMOSWAP_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoand.d.rl",    64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOAND_D|MASK_RL, MASK_AMOAND_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoor.d.rl",     64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOOR_D|MASK_RL, MASK_AMOOR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoxor.d.rl",    64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOXOR_D|MASK_RL, MASK_AMOXOR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomax.d.rl",    64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAX_D|MASK_RL, MASK_AMOMAX_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomaxu.d.rl",   64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAXU_D|MASK_RL, MASK_AMOMAXU_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomin.d.rl",    64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMIN_D|MASK_RL, MASK_AMOMIN_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amominu.d.rl",   64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMINU_D|MASK_RL, MASK_AMOMINU_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"lr.d.aqrl",      64, INSN_CLASS_A, "d,0(s)",   MATCH_LR_D|MASK_AQRL, MASK_LR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"sc.d.aqrl",      64, INSN_CLASS_A, "d,t,0(s)", MATCH_SC_D|MASK_AQRL, MASK_SC_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoadd.d.aqrl",  64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOADD_D|MASK_AQRL, MASK_AMOADD_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoswap.d.aqrl", 64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOSWAP_D|MASK_AQRL, MASK_AMOSWAP_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoand.d.aqrl",  64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOAND_D|MASK_AQRL, MASK_AMOAND_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoor.d.aqrl",   64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOOR_D|MASK_AQRL, MASK_AMOOR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amoxor.d.aqrl",  64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOXOR_D|MASK_AQRL, MASK_AMOXOR_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomax.d.aqrl",  64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAX_D|MASK_AQRL, MASK_AMOMAX_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomaxu.d.aqrl", 64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMAXU_D|MASK_AQRL, MASK_AMOMAXU_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amomin.d.aqrl",  64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMIN_D|MASK_AQRL, MASK_AMOMIN_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },
{"amominu.d.aqrl", 64, INSN_CLASS_A, "d,t,0(s)", MATCH_AMOMINU_D|MASK_AQRL, MASK_AMOMINU_D|MASK_AQRL, match_opcode, INSN_DREF|INSN_8_BYTE },

/* Multiply/Divide instruction subset.  */
{"mul",        0, INSN_CLASS_M,   "d,s,t",     MATCH_MUL, MASK_MUL, match_opcode, 0 },
{"mulh",       0, INSN_CLASS_M,   "d,s,t",     MATCH_MULH, MASK_MULH, match_opcode, 0 },
{"mulhu",      0, INSN_CLASS_M,   "d,s,t",     MATCH_MULHU, MASK_MULHU, match_opcode, 0 },
{"mulhsu",     0, INSN_CLASS_M,   "d,s,t",     MATCH_MULHSU, MASK_MULHSU, match_opcode, 0 },
{"div",        0, INSN_CLASS_M,   "d,s,t",     MATCH_DIV, MASK_DIV, match_opcode, 0 },
{"divu",       0, INSN_CLASS_M,   "d,s,t",     MATCH_DIVU, MASK_DIVU, match_opcode, 0 },
{"rem",        0, INSN_CLASS_M,   "d,s,t",     MATCH_REM, MASK_REM, match_opcode, 0 },
{"remu",       0, INSN_CLASS_M,   "d,s,t",     MATCH_REMU, MASK_REMU, match_opcode, 0 },
{"mulw",      64, INSN_CLASS_M,   "d,s,t",     MATCH_MULW, MASK_MULW, match_opcode, 0 },
{"divw",      64, INSN_CLASS_M,   "d,s,t",     MATCH_DIVW, MASK_DIVW, match_opcode, 0 },
{"divuw",     64, INSN_CLASS_M,   "d,s,t",     MATCH_DIVUW, MASK_DIVUW, match_opcode, 0 },
{"remw",      64, INSN_CLASS_M,   "d,s,t",     MATCH_REMW, MASK_REMW, match_opcode, 0 },
{"remuw",     64, INSN_CLASS_M,   "d,s,t",     MATCH_REMUW, MASK_REMUW, match_opcode, 0 },

/* Single-precision floating-point instruction subset.  */
{"frcsr",      0, INSN_CLASS_F,   "d",         MATCH_FRCSR, MASK_FRCSR, match_opcode, INSN_ALIAS },
{"frsr",       0, INSN_CLASS_F,   "d",         MATCH_FRCSR, MASK_FRCSR, match_opcode, INSN_ALIAS },
{"fscsr",      0, INSN_CLASS_F,   "s",         MATCH_FSCSR, MASK_FSCSR|MASK_RD, match_opcode, INSN_ALIAS },
{"fscsr",      0, INSN_CLASS_F,   "d,s",       MATCH_FSCSR, MASK_FSCSR, match_opcode, INSN_ALIAS },
{"fssr",       0, INSN_CLASS_F,   "s",         MATCH_FSCSR, MASK_FSCSR|MASK_RD, match_opcode, INSN_ALIAS },
{"fssr",       0, INSN_CLASS_F,   "d,s",       MATCH_FSCSR, MASK_FSCSR, match_opcode, INSN_ALIAS },
{"frrm",       0, INSN_CLASS_F,   "d",         MATCH_FRRM, MASK_FRRM, match_opcode, INSN_ALIAS },
{"fsrm",       0, INSN_CLASS_F,   "s",         MATCH_FSRM, MASK_FSRM|MASK_RD, match_opcode, INSN_ALIAS },
{"fsrm",       0, INSN_CLASS_F,   "d,s",       MATCH_FSRM, MASK_FSRM, match_opcode, INSN_ALIAS },
{"fsrmi",      0, INSN_CLASS_F,   "d,Z",       MATCH_FSRMI, MASK_FSRMI, match_opcode, INSN_ALIAS },
{"fsrmi",      0, INSN_CLASS_F,   "Z",         MATCH_FSRMI, MASK_FSRMI|MASK_RD, match_opcode, INSN_ALIAS },
{"frflags",    0, INSN_CLASS_F,   "d",         MATCH_FRFLAGS, MASK_FRFLAGS, match_opcode, INSN_ALIAS },
{"fsflags",    0, INSN_CLASS_F,   "s",         MATCH_FSFLAGS, MASK_FSFLAGS|MASK_RD, match_opcode, INSN_ALIAS },
{"fsflags",    0, INSN_CLASS_F,   "d,s",       MATCH_FSFLAGS, MASK_FSFLAGS, match_opcode, INSN_ALIAS },
{"fsflagsi",   0, INSN_CLASS_F,   "d,Z",       MATCH_FSFLAGSI, MASK_FSFLAGSI, match_opcode, INSN_ALIAS },
{"fsflagsi",   0, INSN_CLASS_F,   "Z",         MATCH_FSFLAGSI, MASK_FSFLAGSI|MASK_RD, match_opcode, INSN_ALIAS },
{"flw",       32, INSN_CLASS_F_AND_C, "D,Cm(Cc)",  MATCH_C_FLWSP, MASK_C_FLWSP, match_opcode, INSN_ALIAS|INSN_DREF|INSN_4_BYTE },
{"flw",       32, INSN_CLASS_F_AND_C, "CD,Ck(Cs)", MATCH_C_FLW, MASK_C_FLW, match_opcode, INSN_ALIAS|INSN_DREF|INSN_4_BYTE },
{"flw",        0, INSN_CLASS_F,   "D,o(s)",    MATCH_FLW, MASK_FLW, match_opcode, INSN_DREF|INSN_4_BYTE },
{"flw",        0, INSN_CLASS_F,   "D,A,s",     0, (int) M_FLW, match_never, INSN_MACRO },
{"fsw",       32, INSN_CLASS_F_AND_C, "CT,CM(Cc)", MATCH_C_FSWSP, MASK_C_FSWSP, match_opcode, INSN_ALIAS|INSN_DREF|INSN_4_BYTE },
{"fsw",       32, INSN_CLASS_F_AND_C, "CD,Ck(Cs)", MATCH_C_FSW, MASK_C_FSW, match_opcode, INSN_ALIAS|INSN_DREF|INSN_4_BYTE },
{"fsw",        0, INSN_CLASS_F,   "T,q(s)",    MATCH_FSW, MASK_FSW, match_opcode, INSN_DREF|INSN_4_BYTE },
{"fsw",        0, INSN_CLASS_F,   "T,A,s",     0, (int) M_FSW, match_never, INSN_MACRO },
{"fmv.x.w",    0, INSN_CLASS_F,   "d,S",       MATCH_FMV_X_S, MASK_FMV_X_S, match_opcode, 0 },
{"fmv.w.x",    0, INSN_CLASS_F,   "D,s",       MATCH_FMV_S_X, MASK_FMV_S_X, match_opcode, 0 },
{"fmv.x.s",    0, INSN_CLASS_F,   "d,S",       MATCH_FMV_X_S, MASK_FMV_X_S, match_opcode, 0 },
{"fmv.s.x",    0, INSN_CLASS_F,   "D,s",       MATCH_FMV_S_X, MASK_FMV_S_X, match_opcode, 0 },
{"fmv.s",      0, INSN_CLASS_F,   "D,U",       MATCH_FSGNJ_S, MASK_FSGNJ_S, match_rs1_eq_rs2, INSN_ALIAS },
{"fneg.s",     0, INSN_CLASS_F,   "D,U",       MATCH_FSGNJN_S, MASK_FSGNJN_S, match_rs1_eq_rs2, INSN_ALIAS },
{"fabs.s",     0, INSN_CLASS_F,   "D,U",       MATCH_FSGNJX_S, MASK_FSGNJX_S, match_rs1_eq_rs2, INSN_ALIAS },
{"fsgnj.s",    0, INSN_CLASS_F,   "D,S,T",     MATCH_FSGNJ_S, MASK_FSGNJ_S, match_opcode, 0 },
{"fsgnjn.s",   0, INSN_CLASS_F,   "D,S,T",     MATCH_FSGNJN_S, MASK_FSGNJN_S, match_opcode, 0 },
{"fsgnjx.s",   0, INSN_CLASS_F,   "D,S,T",     MATCH_FSGNJX_S, MASK_FSGNJX_S, match_opcode, 0 },
{"fadd.s",     0, INSN_CLASS_F,   "D,S,T",     MATCH_FADD_S|MASK_RM, MASK_FADD_S|MASK_RM, match_opcode, 0 },
{"fadd.s",     0, INSN_CLASS_F,   "D,S,T,m",   MATCH_FADD_S, MASK_FADD_S, match_opcode, 0 },
{"fsub.s",     0, INSN_CLASS_F,   "D,S,T",     MATCH_FSUB_S|MASK_RM, MASK_FSUB_S|MASK_RM, match_opcode, 0 },
{"fsub.s",     0, INSN_CLASS_F,   "D,S,T,m",   MATCH_FSUB_S, MASK_FSUB_S, match_opcode, 0 },
{"fmul.s",     0, INSN_CLASS_F,   "D,S,T",     MATCH_FMUL_S|MASK_RM, MASK_FMUL_S|MASK_RM, match_opcode, 0 },
{"fmul.s",     0, INSN_CLASS_F,   "D,S,T,m",   MATCH_FMUL_S, MASK_FMUL_S, match_opcode, 0 },
{"fdiv.s",     0, INSN_CLASS_F,   "D,S,T",     MATCH_FDIV_S|MASK_RM, MASK_FDIV_S|MASK_RM, match_opcode, 0 },
{"fdiv.s",     0, INSN_CLASS_F,   "D,S,T,m",   MATCH_FDIV_S, MASK_FDIV_S, match_opcode, 0 },
{"fsqrt.s",    0, INSN_CLASS_F,   "D,S",       MATCH_FSQRT_S|MASK_RM, MASK_FSQRT_S|MASK_RM, match_opcode, 0 },
{"fsqrt.s",    0, INSN_CLASS_F,   "D,S,m",     MATCH_FSQRT_S, MASK_FSQRT_S, match_opcode, 0 },
{"fmin.s",     0, INSN_CLASS_F,   "D,S,T",     MATCH_FMIN_S, MASK_FMIN_S, match_opcode, 0 },
{"fmax.s",     0, INSN_CLASS_F,   "D,S,T",     MATCH_FMAX_S, MASK_FMAX_S, match_opcode, 0 },
{"fmadd.s",    0, INSN_CLASS_F,   "D,S,T,R",   MATCH_FMADD_S|MASK_RM, MASK_FMADD_S|MASK_RM, match_opcode, 0 },
{"fmadd.s",    0, INSN_CLASS_F,   "D,S,T,R,m", MATCH_FMADD_S, MASK_FMADD_S, match_opcode, 0 },
{"fnmadd.s",   0, INSN_CLASS_F,   "D,S,T,R",   MATCH_FNMADD_S|MASK_RM, MASK_FNMADD_S|MASK_RM, match_opcode, 0 },
{"fnmadd.s",   0, INSN_CLASS_F,   "D,S,T,R,m", MATCH_FNMADD_S, MASK_FNMADD_S, match_opcode, 0 },
{"fmsub.s",    0, INSN_CLASS_F,   "D,S,T,R",   MATCH_FMSUB_S|MASK_RM, MASK_FMSUB_S|MASK_RM, match_opcode, 0 },
{"fmsub.s",    0, INSN_CLASS_F,   "D,S,T,R,m", MATCH_FMSUB_S, MASK_FMSUB_S, match_opcode, 0 },
{"fnmsub.s",   0, INSN_CLASS_F,   "D,S,T,R",   MATCH_FNMSUB_S|MASK_RM, MASK_FNMSUB_S|MASK_RM, match_opcode, 0 },
{"fnmsub.s",   0, INSN_CLASS_F,   "D,S,T,R,m", MATCH_FNMSUB_S, MASK_FNMSUB_S, match_opcode, 0 },
{"fcvt.w.s",   0, INSN_CLASS_F,   "d,S",       MATCH_FCVT_W_S|MASK_RM, MASK_FCVT_W_S|MASK_RM, match_opcode, 0 },
{"fcvt.w.s",   0, INSN_CLASS_F,   "d,S,m",     MATCH_FCVT_W_S, MASK_FCVT_W_S, match_opcode, 0 },
{"fcvt.wu.s",  0, INSN_CLASS_F,   "d,S",       MATCH_FCVT_WU_S|MASK_RM, MASK_FCVT_WU_S|MASK_RM, match_opcode, 0 },
{"fcvt.wu.s",  0, INSN_CLASS_F,   "d,S,m",     MATCH_FCVT_WU_S, MASK_FCVT_WU_S, match_opcode, 0 },
{"fcvt.s.w",   0, INSN_CLASS_F,   "D,s",       MATCH_FCVT_S_W|MASK_RM, MASK_FCVT_S_W|MASK_RM, match_opcode, 0 },
{"fcvt.s.w",   0, INSN_CLASS_F,   "D,s,m",     MATCH_FCVT_S_W, MASK_FCVT_S_W, match_opcode, 0 },
{"fcvt.s.wu",  0, INSN_CLASS_F,   "D,s",       MATCH_FCVT_S_WU|MASK_RM, MASK_FCVT_S_W|MASK_RM, match_opcode, 0 },
{"fcvt.s.wu",  0, INSN_CLASS_F,   "D,s,m",     MATCH_FCVT_S_WU, MASK_FCVT_S_WU, match_opcode, 0 },
{"fclass.s",   0, INSN_CLASS_F,   "d,S",       MATCH_FCLASS_S, MASK_FCLASS_S, match_opcode, 0 },
{"feq.s",      0, INSN_CLASS_F,   "d,S,T",     MATCH_FEQ_S, MASK_FEQ_S, match_opcode, 0 },
{"flt.s",      0, INSN_CLASS_F,   "d,S,T",     MATCH_FLT_S, MASK_FLT_S, match_opcode, 0 },
{"fle.s",      0, INSN_CLASS_F,   "d,S,T",     MATCH_FLE_S, MASK_FLE_S, match_opcode, 0 },
{"fgt.s",      0, INSN_CLASS_F,   "d,T,S",     MATCH_FLT_S, MASK_FLT_S, match_opcode, 0 },
{"fge.s",      0, INSN_CLASS_F,   "d,T,S",     MATCH_FLE_S, MASK_FLE_S, match_opcode, 0 },
{"fcvt.l.s",  64, INSN_CLASS_F,   "d,S",       MATCH_FCVT_L_S|MASK_RM, MASK_FCVT_L_S|MASK_RM, match_opcode, 0 },
{"fcvt.l.s",  64, INSN_CLASS_F,   "d,S,m",     MATCH_FCVT_L_S, MASK_FCVT_L_S, match_opcode, 0 },
{"fcvt.lu.s", 64, INSN_CLASS_F,   "d,S",       MATCH_FCVT_LU_S|MASK_RM, MASK_FCVT_LU_S|MASK_RM, match_opcode, 0 },
{"fcvt.lu.s", 64, INSN_CLASS_F,   "d,S,m",     MATCH_FCVT_LU_S, MASK_FCVT_LU_S, match_opcode, 0 },
{"fcvt.s.l",  64, INSN_CLASS_F,   "D,s",       MATCH_FCVT_S_L|MASK_RM, MASK_FCVT_S_L|MASK_RM, match_opcode, 0 },
{"fcvt.s.l",  64, INSN_CLASS_F,   "D,s,m",     MATCH_FCVT_S_L, MASK_FCVT_S_L, match_opcode, 0 },
{"fcvt.s.lu", 64, INSN_CLASS_F,   "D,s",       MATCH_FCVT_S_LU|MASK_RM, MASK_FCVT_S_L|MASK_RM, match_opcode, 0 },
{"fcvt.s.lu", 64, INSN_CLASS_F,   "D,s,m",     MATCH_FCVT_S_LU, MASK_FCVT_S_LU, match_opcode, 0 },

/* Double-precision floating-point instruction subset.  */
{"fld",        0, INSN_CLASS_D_AND_C, "D,Cn(Cc)",  MATCH_C_FLDSP, MASK_C_FLDSP, match_opcode, INSN_ALIAS|INSN_DREF|INSN_8_BYTE },
{"fld",        0, INSN_CLASS_D_AND_C, "CD,Cl(Cs)", MATCH_C_FLD, MASK_C_FLD, match_opcode, INSN_ALIAS|INSN_DREF|INSN_8_BYTE },
{"fld",        0, INSN_CLASS_D,   "D,o(s)",    MATCH_FLD, MASK_FLD, match_opcode, INSN_DREF|INSN_8_BYTE },
{"fld",        0, INSN_CLASS_D,   "D,A,s",     0, (int) M_FLD, match_never, INSN_MACRO },
{"fsd",        0, INSN_CLASS_D_AND_C, "CT,CN(Cc)", MATCH_C_FSDSP, MASK_C_FSDSP, match_opcode, INSN_ALIAS|INSN_DREF|INSN_8_BYTE },
{"fsd",        0, INSN_CLASS_D_AND_C, "CD,Cl(Cs)", MATCH_C_FSD, MASK_C_FSD, match_opcode, INSN_ALIAS|INSN_DREF|INSN_8_BYTE },
{"fsd",        0, INSN_CLASS_D,   "T,q(s)",    MATCH_FSD, MASK_FSD, match_opcode, INSN_DREF|INSN_8_BYTE },
{"fsd",        0, INSN_CLASS_D,   "T,A,s",     0, (int) M_FSD, match_never, INSN_MACRO },
{"fmv.d",      0, INSN_CLASS_D,   "D,U",       MATCH_FSGNJ_D, MASK_FSGNJ_D, match_rs1_eq_rs2, INSN_ALIAS },
{"fneg.d",     0, INSN_CLASS_D,   "D,U",       MATCH_FSGNJN_D, MASK_FSGNJN_D, match_rs1_eq_rs2, INSN_ALIAS },
{"fabs.d",     0, INSN_CLASS_D,   "D,U",       MATCH_FSGNJX_D, MASK_FSGNJX_D, match_rs1_eq_rs2, INSN_ALIAS },
{"fsgnj.d",    0, INSN_CLASS_D,   "D,S,T",     MATCH_FSGNJ_D, MASK_FSGNJ_D, match_opcode, 0 },
{"fsgnjn.d",   0, INSN_CLASS_D,   "D,S,T",     MATCH_FSGNJN_D, MASK_FSGNJN_D, match_opcode, 0 },
{"fsgnjx.d",   0, INSN_CLASS_D,   "D,S,T",     MATCH_FSGNJX_D, MASK_FSGNJX_D, match_opcode, 0 },
{"fadd.d",     0, INSN_CLASS_D,   "D,S,T",     MATCH_FADD_D|MASK_RM, MASK_FADD_D|MASK_RM, match_opcode, 0 },
{"fadd.d",     0, INSN_CLASS_D,   "D,S,T,m",   MATCH_FADD_D, MASK_FADD_D, match_opcode, 0 },
{"fsub.d",     0, INSN_CLASS_D,   "D,S,T",     MATCH_FSUB_D|MASK_RM, MASK_FSUB_D|MASK_RM, match_opcode, 0 },
{"fsub.d",     0, INSN_CLASS_D,   "D,S,T,m",   MATCH_FSUB_D, MASK_FSUB_D, match_opcode, 0 },
{"fmul.d",     0, INSN_CLASS_D,   "D,S,T",     MATCH_FMUL_D|MASK_RM, MASK_FMUL_D|MASK_RM, match_opcode, 0 },
{"fmul.d",     0, INSN_CLASS_D,   "D,S,T,m",   MATCH_FMUL_D, MASK_FMUL_D, match_opcode, 0 },
{"fdiv.d",     0, INSN_CLASS_D,   "D,S,T",     MATCH_FDIV_D|MASK_RM, MASK_FDIV_D|MASK_RM, match_opcode, 0 },
{"fdiv.d",     0, INSN_CLASS_D,   "D,S,T,m",   MATCH_FDIV_D, MASK_FDIV_D, match_opcode, 0 },
{"fsqrt.d",    0, INSN_CLASS_D,   "D,S",       MATCH_FSQRT_D|MASK_RM, MASK_FSQRT_D|MASK_RM, match_opcode, 0 },
{"fsqrt.d",    0, INSN_CLASS_D,   "D,S,m",     MATCH_FSQRT_D, MASK_FSQRT_D, match_opcode, 0 },
{"fmin.d",     0, INSN_CLASS_D,   "D,S,T",     MATCH_FMIN_D, MASK_FMIN_D, match_opcode, 0 },
{"fmax.d",     0, INSN_CLASS_D,   "D,S,T",     MATCH_FMAX_D, MASK_FMAX_D, match_opcode, 0 },
{"fmadd.d",    0, INSN_CLASS_D,   "D,S,T,R",   MATCH_FMADD_D|MASK_RM, MASK_FMADD_D|MASK_RM, match_opcode, 0 },
{"fmadd.d",    0, INSN_CLASS_D,   "D,S,T,R,m", MATCH_FMADD_D, MASK_FMADD_D, match_opcode, 0 },
{"fnmadd.d",   0, INSN_CLASS_D,   "D,S,T,R",   MATCH_FNMADD_D|MASK_RM, MASK_FNMADD_D|MASK_RM, match_opcode, 0 },
{"fnmadd.d",   0, INSN_CLASS_D,   "D,S,T,R,m", MATCH_FNMADD_D, MASK_FNMADD_D, match_opcode, 0 },
{"fmsub.d",    0, INSN_CLASS_D,   "D,S,T,R",   MATCH_FMSUB_D|MASK_RM, MASK_FMSUB_D|MASK_RM, match_opcode, 0 },
{"fmsub.d",    0, INSN_CLASS_D,   "D,S,T,R,m", MATCH_FMSUB_D, MASK_FMSUB_D, match_opcode, 0 },
{"fnmsub.d",   0, INSN_CLASS_D,   "D,S,T,R",   MATCH_FNMSUB_D|MASK_RM, MASK_FNMSUB_D|MASK_RM, match_opcode, 0 },
{"fnmsub.d",   0, INSN_CLASS_D,   "D,S,T,R,m", MATCH_FNMSUB_D, MASK_FNMSUB_D, match_opcode, 0 },
{"fcvt.w.d",   0, INSN_CLASS_D,   "d,S",       MATCH_FCVT_W_D|MASK_RM, MASK_FCVT_W_D|MASK_RM, match_opcode, 0 },
{"fcvt.w.d",   0, INSN_CLASS_D,   "d,S,m",     MATCH_FCVT_W_D, MASK_FCVT_W_D, match_opcode, 0 },
{"fcvt.wu.d",  0, INSN_CLASS_D,   "d,S",       MATCH_FCVT_WU_D|MASK_RM, MASK_FCVT_WU_D|MASK_RM, match_opcode, 0 },
{"fcvt.wu.d",  0, INSN_CLASS_D,   "d,S,m",     MATCH_FCVT_WU_D, MASK_FCVT_WU_D, match_opcode, 0 },
{"fcvt.d.w",   0, INSN_CLASS_D,   "D,s",       MATCH_FCVT_D_W, MASK_FCVT_D_W|MASK_RM, match_opcode, 0 },
{"fcvt.d.wu",  0, INSN_CLASS_D,   "D,s",       MATCH_FCVT_D_WU, MASK_FCVT_D_WU|MASK_RM, match_opcode, 0 },
{"fcvt.d.s",   0, INSN_CLASS_D,   "D,S",       MATCH_FCVT_D_S, MASK_FCVT_D_S|MASK_RM, match_opcode, 0 },
{"fcvt.s.d",   0, INSN_CLASS_D,   "D,S",       MATCH_FCVT_S_D|MASK_RM, MASK_FCVT_S_D|MASK_RM, match_opcode, 0 },
{"fcvt.s.d",   0, INSN_CLASS_D,   "D,S,m",     MATCH_FCVT_S_D, MASK_FCVT_S_D, match_opcode, 0 },
{"fclass.d",   0, INSN_CLASS_D,   "d,S",       MATCH_FCLASS_D, MASK_FCLASS_D, match_opcode, 0 },
{"feq.d",      0, INSN_CLASS_D,   "d,S,T",     MATCH_FEQ_D, MASK_FEQ_D, match_opcode, 0 },
{"flt.d",      0, INSN_CLASS_D,   "d,S,T",     MATCH_FLT_D, MASK_FLT_D, match_opcode, 0 },
{"fle.d",      0, INSN_CLASS_D,   "d,S,T",     MATCH_FLE_D, MASK_FLE_D, match_opcode, 0 },
{"fgt.d",      0, INSN_CLASS_D,   "d,T,S",     MATCH_FLT_D, MASK_FLT_D, match_opcode, 0 },
{"fge.d",      0, INSN_CLASS_D,   "d,T,S",     MATCH_FLE_D, MASK_FLE_D, match_opcode, 0 },
{"fmv.x.d",   64, INSN_CLASS_D,   "d,S",       MATCH_FMV_X_D, MASK_FMV_X_D, match_opcode, 0 },
{"fmv.d.x",   64, INSN_CLASS_D,   "D,s",       MATCH_FMV_D_X, MASK_FMV_D_X, match_opcode, 0 },
{"fcvt.l.d",  64, INSN_CLASS_D,   "d,S",       MATCH_FCVT_L_D|MASK_RM, MASK_FCVT_L_D|MASK_RM, match_opcode, 0 },
{"fcvt.l.d",  64, INSN_CLASS_D,   "d,S,m",     MATCH_FCVT_L_D, MASK_FCVT_L_D, match_opcode, 0 },
{"fcvt.lu.d", 64, INSN_CLASS_D,   "d,S",       MATCH_FCVT_LU_D|MASK_RM, MASK_FCVT_LU_D|MASK_RM, match_opcode, 0 },
{"fcvt.lu.d", 64, INSN_CLASS_D,   "d,S,m",     MATCH_FCVT_LU_D, MASK_FCVT_LU_D, match_opcode, 0 },
{"fcvt.d.l",  64, INSN_CLASS_D,   "D,s",       MATCH_FCVT_D_L|MASK_RM, MASK_FCVT_D_L|MASK_RM, match_opcode, 0 },
{"fcvt.d.l",  64, INSN_CLASS_D,   "D,s,m",     MATCH_FCVT_D_L, MASK_FCVT_D_L, match_opcode, 0 },
{"fcvt.d.lu", 64, INSN_CLASS_D,   "D,s",       MATCH_FCVT_D_LU|MASK_RM, MASK_FCVT_D_L|MASK_RM, match_opcode, 0 },
{"fcvt.d.lu", 64, INSN_CLASS_D,   "D,s,m",     MATCH_FCVT_D_LU, MASK_FCVT_D_LU, match_opcode, 0 },

/* Quad-precision floating-point instruction subset.  */
{"flq",        0, INSN_CLASS_Q,   "D,o(s)",    MATCH_FLQ, MASK_FLQ, match_opcode, INSN_DREF|INSN_16_BYTE },
{"flq",        0, INSN_CLASS_Q,   "D,A,s",     0, (int) M_FLQ, match_never, INSN_MACRO },
{"fsq",        0, INSN_CLASS_Q,   "T,q(s)",    MATCH_FSQ, MASK_FSQ, match_opcode, INSN_DREF|INSN_16_BYTE },
{"fsq",        0, INSN_CLASS_Q,   "T,A,s",     0, (int) M_FSQ, match_never, INSN_MACRO },
{"fmv.q",      0, INSN_CLASS_Q,   "D,U",       MATCH_FSGNJ_Q, MASK_FSGNJ_Q, match_rs1_eq_rs2, INSN_ALIAS },
{"fneg.q",     0, INSN_CLASS_Q,   "D,U",       MATCH_FSGNJN_Q, MASK_FSGNJN_Q, match_rs1_eq_rs2, INSN_ALIAS },
{"fabs.q",     0, INSN_CLASS_Q,   "D,U",       MATCH_FSGNJX_Q, MASK_FSGNJX_Q, match_rs1_eq_rs2, INSN_ALIAS },
{"fsgnj.q",    0, INSN_CLASS_Q,   "D,S,T",     MATCH_FSGNJ_Q, MASK_FSGNJ_Q, match_opcode, 0 },
{"fsgnjn.q",   0, INSN_CLASS_Q,   "D,S,T",     MATCH_FSGNJN_Q, MASK_FSGNJN_Q, match_opcode, 0 },
{"fsgnjx.q",   0, INSN_CLASS_Q,   "D,S,T",     MATCH_FSGNJX_Q, MASK_FSGNJX_Q, match_opcode, 0 },
{"fadd.q",     0, INSN_CLASS_Q,   "D,S,T",     MATCH_FADD_Q|MASK_RM, MASK_FADD_Q|MASK_RM, match_opcode, 0 },
{"fadd.q",     0, INSN_CLASS_Q,   "D,S,T,m",   MATCH_FADD_Q, MASK_FADD_Q, match_opcode, 0 },
{"fsub.q",     0, INSN_CLASS_Q,   "D,S,T",     MATCH_FSUB_Q|MASK_RM, MASK_FSUB_Q|MASK_RM, match_opcode, 0 },
{"fsub.q",     0, INSN_CLASS_Q,   "D,S,T,m",   MATCH_FSUB_Q, MASK_FSUB_Q, match_opcode, 0 },
{"fmul.q",     0, INSN_CLASS_Q,   "D,S,T",     MATCH_FMUL_Q|MASK_RM, MASK_FMUL_Q|MASK_RM, match_opcode, 0 },
{"fmul.q",     0, INSN_CLASS_Q,   "D,S,T,m",   MATCH_FMUL_Q, MASK_FMUL_Q, match_opcode, 0 },
{"fdiv.q",     0, INSN_CLASS_Q,   "D,S,T",     MATCH_FDIV_Q|MASK_RM, MASK_FDIV_Q|MASK_RM, match_opcode, 0 },
{"fdiv.q",     0, INSN_CLASS_Q,   "D,S,T,m",   MATCH_FDIV_Q, MASK_FDIV_Q, match_opcode, 0 },
{"fsqrt.q",    0, INSN_CLASS_Q,   "D,S",       MATCH_FSQRT_Q|MASK_RM, MASK_FSQRT_Q|MASK_RM, match_opcode, 0 },
{"fsqrt.q",    0, INSN_CLASS_Q,   "D,S,m",     MATCH_FSQRT_Q, MASK_FSQRT_Q, match_opcode, 0 },
{"fmin.q",     0, INSN_CLASS_Q,   "D,S,T",     MATCH_FMIN_Q, MASK_FMIN_Q, match_opcode, 0 },
{"fmax.q",     0, INSN_CLASS_Q,   "D,S,T",     MATCH_FMAX_Q, MASK_FMAX_Q, match_opcode, 0 },
{"fmadd.q",    0, INSN_CLASS_Q,   "D,S,T,R",   MATCH_FMADD_Q|MASK_RM, MASK_FMADD_Q|MASK_RM, match_opcode, 0 },
{"fmadd.q",    0, INSN_CLASS_Q,   "D,S,T,R,m", MATCH_FMADD_Q, MASK_FMADD_Q, match_opcode, 0 },
{"fnmadd.q",   0, INSN_CLASS_Q,   "D,S,T,R",   MATCH_FNMADD_Q|MASK_RM, MASK_FNMADD_Q|MASK_RM, match_opcode, 0 },
{"fnmadd.q",   0, INSN_CLASS_Q,   "D,S,T,R,m", MATCH_FNMADD_Q, MASK_FNMADD_Q, match_opcode, 0 },
{"fmsub.q",    0, INSN_CLASS_Q,   "D,S,T,R",   MATCH_FMSUB_Q|MASK_RM, MASK_FMSUB_Q|MASK_RM, match_opcode, 0 },
{"fmsub.q",    0, INSN_CLASS_Q,   "D,S,T,R,m", MATCH_FMSUB_Q, MASK_FMSUB_Q, match_opcode, 0 },
{"fnmsub.q",   0, INSN_CLASS_Q,   "D,S,T,R",   MATCH_FNMSUB_Q|MASK_RM, MASK_FNMSUB_Q|MASK_RM, match_opcode, 0 },
{"fnmsub.q",   0, INSN_CLASS_Q,   "D,S,T,R,m", MATCH_FNMSUB_Q, MASK_FNMSUB_Q, match_opcode, 0 },
{"fcvt.w.q",   0, INSN_CLASS_Q,   "d,S",       MATCH_FCVT_W_Q|MASK_RM, MASK_FCVT_W_Q|MASK_RM, match_opcode, 0 },
{"fcvt.w.q",   0, INSN_CLASS_Q,   "d,S,m",     MATCH_FCVT_W_Q, MASK_FCVT_W_Q, match_opcode, 0 },
{"fcvt.wu.q",  0, INSN_CLASS_Q,   "d,S",       MATCH_FCVT_WU_Q|MASK_RM, MASK_FCVT_WU_Q|MASK_RM, match_opcode, 0 },
{"fcvt.wu.q",  0, INSN_CLASS_Q,   "d,S,m",     MATCH_FCVT_WU_Q, MASK_FCVT_WU_Q, match_opcode, 0 },
{"fcvt.q.w",   0, INSN_CLASS_Q,   "D,s",       MATCH_FCVT_Q_W, MASK_FCVT_Q_W|MASK_RM, match_opcode, 0 },
{"fcvt.q.wu",  0, INSN_CLASS_Q,   "D,s",       MATCH_FCVT_Q_WU, MASK_FCVT_Q_WU|MASK_RM, match_opcode, 0 },
{"fcvt.q.s",   0, INSN_CLASS_Q,   "D,S",       MATCH_FCVT_Q_S, MASK_FCVT_Q_S|MASK_RM, match_opcode, 0 },
{"fcvt.q.d",   0, INSN_CLASS_Q,   "D,S",       MATCH_FCVT_Q_D, MASK_FCVT_Q_D|MASK_RM, match_opcode, 0 },
{"fcvt.s.q",   0, INSN_CLASS_Q,   "D,S",       MATCH_FCVT_S_Q|MASK_RM, MASK_FCVT_S_Q|MASK_RM, match_opcode, 0 },
{"fcvt.s.q",   0, INSN_CLASS_Q,   "D,S,m",     MATCH_FCVT_S_Q, MASK_FCVT_S_Q, match_opcode, 0 },
{"fcvt.d.q",   0, INSN_CLASS_Q,   "D,S",       MATCH_FCVT_D_Q|MASK_RM, MASK_FCVT_D_Q|MASK_RM, match_opcode, 0 },
{"fcvt.d.q",   0, INSN_CLASS_Q,   "D,S,m",     MATCH_FCVT_D_Q, MASK_FCVT_D_Q, match_opcode, 0 },
{"fclass.q",   0, INSN_CLASS_Q,   "d,S",       MATCH_FCLASS_Q, MASK_FCLASS_Q, match_opcode, 0 },
{"feq.q",      0, INSN_CLASS_Q,   "d,S,T",     MATCH_FEQ_Q, MASK_FEQ_Q, match_opcode, 0 },
{"flt.q",      0, INSN_CLASS_Q,   "d,S,T",     MATCH_FLT_Q, MASK_FLT_Q, match_opcode, 0 },
{"fle.q",      0, INSN_CLASS_Q,   "d,S,T",     MATCH_FLE_Q, MASK_FLE_Q, match_opcode, 0 },
{"fgt.q",      0, INSN_CLASS_Q,   "d,T,S",     MATCH_FLT_Q, MASK_FLT_Q, match_opcode, 0 },
{"fge.q",      0, INSN_CLASS_Q,   "d,T,S",     MATCH_FLE_Q, MASK_FLE_Q, match_opcode, 0 },
{"fmv.x.q",   64, INSN_CLASS_Q,   "d,S",       MATCH_FMV_X_Q, MASK_FMV_X_Q, match_opcode, 0 },
{"fmv.q.x",   64, INSN_CLASS_Q,   "D,s",       MATCH_FMV_Q_X, MASK_FMV_Q_X, match_opcode, 0 },
{"fcvt.l.q",  64, INSN_CLASS_Q,   "d,S",       MATCH_FCVT_L_Q|MASK_RM, MASK_FCVT_L_Q|MASK_RM, match_opcode, 0 },
{"fcvt.l.q",  64, INSN_CLASS_Q,   "d,S,m",     MATCH_FCVT_L_Q, MASK_FCVT_L_Q, match_opcode, 0 },
{"fcvt.lu.q", 64, INSN_CLASS_Q,   "d,S",       MATCH_FCVT_LU_Q|MASK_RM, MASK_FCVT_LU_Q|MASK_RM, match_opcode, 0 },
{"fcvt.lu.q", 64, INSN_CLASS_Q,   "d,S,m",     MATCH_FCVT_LU_Q, MASK_FCVT_LU_Q, match_opcode, 0 },
{"fcvt.q.l",  64, INSN_CLASS_Q,   "D,s",       MATCH_FCVT_Q_L|MASK_RM, MASK_FCVT_Q_L|MASK_RM, match_opcode, 0 },
{"fcvt.q.l",  64, INSN_CLASS_Q,   "D,s,m",     MATCH_FCVT_Q_L, MASK_FCVT_Q_L, match_opcode, 0 },
{"fcvt.q.lu", 64, INSN_CLASS_Q,   "D,s",       MATCH_FCVT_Q_LU|MASK_RM, MASK_FCVT_Q_L|MASK_RM, match_opcode, 0 },
{"fcvt.q.lu", 64, INSN_CLASS_Q,   "D,s,m",     MATCH_FCVT_Q_LU, MASK_FCVT_Q_LU, match_opcode, 0 },

/* Compressed instructions.  */
{"c.unimp",    0, INSN_CLASS_C,   "",          0, 0xffffU,  match_opcode, 0 },
{"c.ebreak",   0, INSN_CLASS_C,   "",          MATCH_C_EBREAK, MASK_C_EBREAK, match_opcode, 0 },
{"c.jr",       0, INSN_CLASS_C,   "d",         MATCH_C_JR, MASK_C_JR, match_rd_nonzero, INSN_BRANCH },
{"c.jalr",     0, INSN_CLASS_C,   "d",         MATCH_C_JALR, MASK_C_JALR, match_rd_nonzero, INSN_JSR },
{"c.j",        0, INSN_CLASS_C,   "Ca",        MATCH_C_J, MASK_C_J, match_opcode, INSN_BRANCH },
{"c.jal",     32, INSN_CLASS_C,   "Ca",        MATCH_C_JAL, MASK_C_JAL, match_opcode, INSN_JSR },
{"c.beqz",     0, INSN_CLASS_C,   "Cs,Cp",     MATCH_C_BEQZ, MASK_C_BEQZ, match_opcode, INSN_CONDBRANCH },
{"c.bnez",     0, INSN_CLASS_C,   "Cs,Cp",     MATCH_C_BNEZ, MASK_C_BNEZ, match_opcode, INSN_CONDBRANCH },
{"c.lwsp",     0, INSN_CLASS_C,   "d,Cm(Cc)",  MATCH_C_LWSP, MASK_C_LWSP, match_rd_nonzero, 0 },
{"c.lw",       0, INSN_CLASS_C,   "Ct,Ck(Cs)", MATCH_C_LW, MASK_C_LW, match_opcode, INSN_DREF|INSN_4_BYTE },
{"c.swsp",     0, INSN_CLASS_C,   "CV,CM(Cc)", MATCH_C_SWSP, MASK_C_SWSP, match_opcode, INSN_DREF|INSN_4_BYTE },
{"c.sw",       0, INSN_CLASS_C,   "Ct,Ck(Cs)", MATCH_C_SW, MASK_C_SW, match_opcode, INSN_DREF|INSN_4_BYTE },
{"c.nop",      0, INSN_CLASS_C,   "",          MATCH_C_ADDI, 0xffff, match_opcode, INSN_ALIAS },
{"c.nop",      0, INSN_CLASS_C,   "Cj",        MATCH_C_ADDI, MASK_C_ADDI|MASK_RD, match_opcode, INSN_ALIAS },
{"c.mv",       0, INSN_CLASS_C,   "d,CV",      MATCH_C_MV, MASK_C_MV, match_c_add_with_hint, 0 },
{"c.lui",      0, INSN_CLASS_C,   "d,Cu",      MATCH_C_LUI, MASK_C_LUI, match_c_lui_with_hint, 0 },
{"c.li",       0, INSN_CLASS_C,   "d,Co",      MATCH_C_LI, MASK_C_LI, match_opcode, 0 },
{"c.addi4spn", 0, INSN_CLASS_C,   "Ct,Cc,CK",  MATCH_C_ADDI4SPN, MASK_C_ADDI4SPN, match_c_addi4spn, 0 },
{"c.addi16sp", 0, INSN_CLASS_C,   "Cc,CL",     MATCH_C_ADDI16SP, MASK_C_ADDI16SP, match_c_addi16sp, 0 },
{"c.addi",     0, INSN_CLASS_C,   "d,Co",      MATCH_C_ADDI, MASK_C_ADDI, match_opcode, 0 },
{"c.add",      0, INSN_CLASS_C,   "d,CV",      MATCH_C_ADD, MASK_C_ADD, match_c_add_with_hint, 0 },
{"c.sub",      0, INSN_CLASS_C,   "Cs,Ct",     MATCH_C_SUB, MASK_C_SUB, match_opcode, 0 },
{"c.and",      0, INSN_CLASS_C,   "Cs,Ct",     MATCH_C_AND, MASK_C_AND, match_opcode, 0 },
{"c.or",       0, INSN_CLASS_C,   "Cs,Ct",     MATCH_C_OR, MASK_C_OR, match_opcode, 0 },
{"c.xor",      0, INSN_CLASS_C,   "Cs,Ct",     MATCH_C_XOR, MASK_C_XOR, match_opcode, 0 },
{"c.slli",     0, INSN_CLASS_C,   "d,C>",      MATCH_C_SLLI, MASK_C_SLLI, match_c_slli, 0 },
{"c.srli",     0, INSN_CLASS_C,   "Cs,C>",     MATCH_C_SRLI, MASK_C_SRLI, match_c_slli, 0 },
{"c.srai",     0, INSN_CLASS_C,   "Cs,C>",     MATCH_C_SRAI, MASK_C_SRAI, match_c_slli, 0 },
{"c.slli64",   0, INSN_CLASS_C,   "d",         MATCH_C_SLLI64, MASK_C_SLLI64, match_c_slli64, 0 },
{"c.srli64",   0, INSN_CLASS_C,   "Cs",        MATCH_C_SRLI64, MASK_C_SRLI64, match_c_slli64, 0 },
{"c.srai64",   0, INSN_CLASS_C,   "Cs",        MATCH_C_SRAI64, MASK_C_SRAI64, match_c_slli64, 0 },
{"c.andi",     0, INSN_CLASS_C,   "Cs,Co",     MATCH_C_ANDI, MASK_C_ANDI, match_opcode, 0 },
{"c.addiw",   64, INSN_CLASS_C,   "d,Co",      MATCH_C_ADDIW, MASK_C_ADDIW, match_rd_nonzero, 0 },
{"c.addw",    64, INSN_CLASS_C,   "Cs,Ct",     MATCH_C_ADDW, MASK_C_ADDW, match_opcode, 0 },
{"c.subw",    64, INSN_CLASS_C,   "Cs,Ct",     MATCH_C_SUBW, MASK_C_SUBW, match_opcode, 0 },
{"c.ldsp",    64, INSN_CLASS_C,   "d,Cn(Cc)",  MATCH_C_LDSP, MASK_C_LDSP, match_rd_nonzero, INSN_DREF|INSN_8_BYTE },
{"c.ld",      64, INSN_CLASS_C,   "Ct,Cl(Cs)", MATCH_C_LD, MASK_C_LD, match_opcode, INSN_DREF|INSN_8_BYTE },
{"c.sdsp",    64, INSN_CLASS_C,   "CV,CN(Cc)", MATCH_C_SDSP, MASK_C_SDSP, match_opcode, INSN_DREF|INSN_8_BYTE },
{"c.sd",      64, INSN_CLASS_C,   "Ct,Cl(Cs)", MATCH_C_SD, MASK_C_SD, match_opcode, INSN_DREF|INSN_8_BYTE },
{"c.fldsp",    0, INSN_CLASS_D_AND_C, "D,Cn(Cc)",  MATCH_C_FLDSP, MASK_C_FLDSP, match_opcode, INSN_DREF|INSN_8_BYTE },
{"c.fld",      0, INSN_CLASS_D_AND_C, "CD,Cl(Cs)", MATCH_C_FLD, MASK_C_FLD, match_opcode, INSN_DREF|INSN_8_BYTE },
{"c.fsdsp",    0, INSN_CLASS_D_AND_C, "CT,CN(Cc)", MATCH_C_FSDSP, MASK_C_FSDSP, match_opcode, INSN_DREF|INSN_8_BYTE },
{"c.fsd",      0, INSN_CLASS_D_AND_C, "CD,Cl(Cs)", MATCH_C_FSD, MASK_C_FSD, match_opcode, INSN_DREF|INSN_8_BYTE },
{"c.flwsp",   32, INSN_CLASS_F_AND_C, "D,Cm(Cc)",  MATCH_C_FLWSP, MASK_C_FLWSP, match_opcode, INSN_DREF|INSN_4_BYTE },
{"c.flw",     32, INSN_CLASS_F_AND_C, "CD,Ck(Cs)", MATCH_C_FLW, MASK_C_FLW, match_opcode, INSN_DREF|INSN_4_BYTE },
{"c.fswsp",   32, INSN_CLASS_F_AND_C, "CT,CM(Cc)", MATCH_C_FSWSP, MASK_C_FSWSP, match_opcode, INSN_DREF|INSN_4_BYTE },
{"c.fsw",     32, INSN_CLASS_F_AND_C, "CD,Ck(Cs)", MATCH_C_FSW, MASK_C_FSW, match_opcode, INSN_DREF|INSN_4_BYTE },

/* Supervisor instructions.  */
{"csrr",       0, INSN_CLASS_ZICSR,"d,E",      MATCH_CSRRS, MASK_CSRRS|MASK_RS1, match_opcode, INSN_ALIAS },
{"csrwi",      0, INSN_CLASS_ZICSR,"E,Z",      MATCH_CSRRWI, MASK_CSRRWI|MASK_RD, match_opcode, INSN_ALIAS },
{"csrsi",      0, INSN_CLASS_ZICSR,"E,Z",      MATCH_CSRRSI, MASK_CSRRSI|MASK_RD, match_opcode, INSN_ALIAS },
{"csrci",      0, INSN_CLASS_ZICSR,"E,Z",      MATCH_CSRRCI, MASK_CSRRCI|MASK_RD, match_opcode, INSN_ALIAS },
{"csrw",       0, INSN_CLASS_ZICSR,"E,s",      MATCH_CSRRW, MASK_CSRRW|MASK_RD, match_opcode, INSN_ALIAS },
{"csrw",       0, INSN_CLASS_ZICSR,"E,Z",      MATCH_CSRRWI, MASK_CSRRWI|MASK_RD, match_opcode, INSN_ALIAS },
{"csrs",       0, INSN_CLASS_ZICSR,"E,s",      MATCH_CSRRS, MASK_CSRRS|MASK_RD, match_opcode, INSN_ALIAS },
{"csrs",       0, INSN_CLASS_ZICSR,"E,Z",      MATCH_CSRRSI, MASK_CSRRSI|MASK_RD, match_opcode, INSN_ALIAS },
{"csrc",       0, INSN_CLASS_ZICSR,"E,s",      MATCH_CSRRC, MASK_CSRRC|MASK_RD, match_opcode, INSN_ALIAS },
{"csrc",       0, INSN_CLASS_ZICSR,"E,Z",      MATCH_CSRRCI, MASK_CSRRCI|MASK_RD, match_opcode, INSN_ALIAS },
{"csrrwi",     0, INSN_CLASS_ZICSR,"d,E,Z",    MATCH_CSRRWI, MASK_CSRRWI, match_opcode, 0 },
{"csrrsi",     0, INSN_CLASS_ZICSR,"d,E,Z",    MATCH_CSRRSI, MASK_CSRRSI, match_opcode, 0 },
{"csrrci",     0, INSN_CLASS_ZICSR,"d,E,Z",    MATCH_CSRRCI, MASK_CSRRCI, match_opcode, 0 },
{"csrrw",      0, INSN_CLASS_ZICSR,"d,E,s",    MATCH_CSRRW, MASK_CSRRW, match_opcode, 0 },
{"csrrw",      0, INSN_CLASS_ZICSR,"d,E,Z",    MATCH_CSRRWI, MASK_CSRRWI, match_opcode, INSN_ALIAS },
{"csrrs",      0, INSN_CLASS_ZICSR,"d,E,s",    MATCH_CSRRS, MASK_CSRRS, match_opcode, 0 },
{"csrrs",      0, INSN_CLASS_ZICSR,"d,E,Z",    MATCH_CSRRSI, MASK_CSRRSI, match_opcode, INSN_ALIAS },
{"csrrc",      0, INSN_CLASS_ZICSR,"d,E,s",    MATCH_CSRRC, MASK_CSRRC, match_opcode, 0 },
{"csrrc",      0, INSN_CLASS_ZICSR,"d,E,Z",    MATCH_CSRRCI, MASK_CSRRCI, match_opcode, INSN_ALIAS },
{"uret",       0, INSN_CLASS_I,    "",         MATCH_URET, MASK_URET, match_opcode, 0 },
{"sret",       0, INSN_CLASS_I,    "",         MATCH_SRET, MASK_SRET, match_opcode, 0 },
{"hret",       0, INSN_CLASS_I,    "",         MATCH_HRET, MASK_HRET, match_opcode, 0 },
{"mret",       0, INSN_CLASS_I,    "",         MATCH_MRET, MASK_MRET, match_opcode, 0 },
{"dret",       0, INSN_CLASS_I,    "",         MATCH_DRET, MASK_DRET, match_opcode, 0 },
{"sfence.vm",  0, INSN_CLASS_I,    "",         MATCH_SFENCE_VM, MASK_SFENCE_VM | MASK_RS1, match_opcode, 0 },
{"sfence.vm",  0, INSN_CLASS_I,    "s",        MATCH_SFENCE_VM, MASK_SFENCE_VM, match_opcode, 0 },
{"sfence.vma", 0, INSN_CLASS_I,    "",         MATCH_SFENCE_VMA, MASK_SFENCE_VMA|MASK_RS1|MASK_RS2, match_opcode, INSN_ALIAS },
{"sfence.vma", 0, INSN_CLASS_I,    "s",        MATCH_SFENCE_VMA, MASK_SFENCE_VMA|MASK_RS2, match_opcode, INSN_ALIAS },
{"sfence.vma", 0, INSN_CLASS_I,    "s,t",      MATCH_SFENCE_VMA, MASK_SFENCE_VMA, match_opcode, 0 },
{"wfi",        0, INSN_CLASS_I,    "",         MATCH_WFI, MASK_WFI, match_opcode, 0 },

/* Zbb instructions */
{"clz",        0, INSN_CLASS_ZBB,  "d,s",   MATCH_CLZ, MASK_CLZ, match_opcode, 0 },
{"ctz",        0, INSN_CLASS_ZBB,  "d,s",   MATCH_CTZ, MASK_CTZ, match_opcode, 0 },
{"cpop",       0, INSN_CLASS_ZBB,  "d,s",   MATCH_CPOP, MASK_CPOP, match_opcode, 0 },
{"min",        0, INSN_CLASS_ZBB,  "d,s,t", MATCH_MIN, MASK_MIN, match_opcode, 0 },
{"max",        0, INSN_CLASS_ZBB,  "d,s,t", MATCH_MAX, MASK_MAX, match_opcode, 0 },
{"minu",       0, INSN_CLASS_ZBB,  "d,s,t", MATCH_MINU, MASK_MINU, match_opcode, 0 },
{"maxu",       0, INSN_CLASS_ZBB,  "d,s,t", MATCH_MAXU, MASK_MAXU, match_opcode, 0 },
{"sext.b",     0, INSN_CLASS_ZBB,  "d,s",   MATCH_SEXT_B, MASK_SEXT_B, match_opcode, 0 },
{"sext.b",     0, INSN_CLASS_I,         "d,s",   0, (int) M_SEXTB, match_never, INSN_MACRO },
{"sext.h",     0, INSN_CLASS_ZBB,  "d,s",   MATCH_SEXT_H, MASK_SEXT_H, match_opcode, 0 },
{"sext.h",     0, INSN_CLASS_I,         "d,s",   0, (int) M_SEXTH, match_never, INSN_MACRO },
{"zext.h",    32, INSN_CLASS_ZBB,  "d,s",   MATCH_PACK, MASK_PACK | MASK_RS2, match_opcode, 0 },
{"zext.h",    64, INSN_CLASS_ZBB,  "d,s",   MATCH_PACKW, MASK_PACKW | MASK_RS2, match_opcode, 0 },
{"zext.h",     0, INSN_CLASS_I,         "d,s",   0, (int) M_ZEXTH, match_never, INSN_MACRO },
{"andn",       0, INSN_CLASS_ZBB,  "d,s,t", MATCH_ANDN, MASK_ANDN, match_opcode, 0 },
{"orn",        0, INSN_CLASS_ZBB,  "d,s,t", MATCH_ORN, MASK_ORN, match_opcode, 0 },
{"xnor",       0, INSN_CLASS_ZBB,  "d,s,t", MATCH_XNOR, MASK_XNOR, match_opcode, 0 },
{"rol",        0, INSN_CLASS_ZBB,  "d,s,t", MATCH_ROL, MASK_ROL, match_opcode, 0 },
{"rori",       0, INSN_CLASS_ZBB,  "d,s,>", MATCH_RORI, MASK_RORI, match_opcode, 0 },
{"ror",        0, INSN_CLASS_ZBB,  "d,s,t", MATCH_ROR, MASK_ROR, match_opcode, 0 },
{"ror",        0, INSN_CLASS_ZBB,  "d,s,>", MATCH_RORI, MASK_RORI, match_opcode, INSN_ALIAS },
{"rev8",      32, INSN_CLASS_ZBB,  "d,s",   MATCH_GREVI | MATCH_SHAMT_REV8_32 , MASK_GREVI | MASK_SHAMT, match_opcode, 0 },
{"rev8",      64, INSN_CLASS_ZBB,  "d,s",   MATCH_GREVI | MATCH_SHAMT_REV8_64 , MASK_GREVI | MASK_SHAMT, match_opcode, 0 },
{"orc.b",      0, INSN_CLASS_ZBB,  "d,s",   MATCH_GORCI | MATCH_SHAMT_ORC_B, MASK_GORCI | MASK_SHAMT, match_opcode, 0 },
{"clzw",      64, INSN_CLASS_ZBB,  "d,s",   MATCH_CLZW, MASK_CLZW, match_opcode, 0 },
{"ctzw",      64, INSN_CLASS_ZBB,  "d,s",   MATCH_CTZW, MASK_CTZW, match_opcode, 0 },
{"cpopw",     64, INSN_CLASS_ZBB,  "d,s",   MATCH_CPOPW, MASK_CPOPW, match_opcode, 0 },
{"rolw",      64, INSN_CLASS_ZBB,  "d,s,t", MATCH_ROLW, MASK_ROLW, match_opcode, 0 },
{"roriw",     64, INSN_CLASS_ZBB,  "d,s,<", MATCH_RORIW, MASK_RORIW, match_opcode, 0 },
{"rorw",      64, INSN_CLASS_ZBB,  "d,s,t", MATCH_RORW, MASK_RORW, match_opcode, 0 },
{"rorw",      64, INSN_CLASS_ZBB,  "d,s,<", MATCH_RORIW, MASK_RORIW, match_opcode, 0 },

/* Zba instructions */
{"sh1add",     0, INSN_CLASS_ZBA,  "d,s,t", MATCH_SH1ADD, MASK_SH1ADD, match_opcode, 0 },
{"sh2add",     0, INSN_CLASS_ZBA,  "d,s,t", MATCH_SH2ADD, MASK_SH2ADD, match_opcode, 0 },
{"sh3add",     0, INSN_CLASS_ZBA,  "d,s,t", MATCH_SH3ADD, MASK_SH3ADD, match_opcode, 0 },
{"sh1add.uw", 64, INSN_CLASS_ZBA,  "d,s,t", MATCH_SH1ADD_UW, MASK_SH1ADD_UW, match_opcode, 0 },
{"sh2add.uw", 64, INSN_CLASS_ZBA,  "d,s,t", MATCH_SH2ADD_UW, MASK_SH2ADD_UW, match_opcode, 0 },
{"sh3add.uw", 64, INSN_CLASS_ZBA,  "d,s,t", MATCH_SH3ADD_UW, MASK_SH3ADD_UW, match_opcode, 0 },
{"zext.w",    64, INSN_CLASS_ZBA,  "d,s",   MATCH_ADD_UW, MASK_ADD_UW | MASK_RS2, match_opcode, INSN_ALIAS },
{"zext.w",    64, INSN_CLASS_I, "d,s",       0, (int) M_ZEXTW, match_never, INSN_MACRO },
{"add.uw",    64, INSN_CLASS_ZBA,  "d,s,t", MATCH_ADD_UW, MASK_ADD_UW, match_opcode, 0 },
{"slli.uw",   64, INSN_CLASS_ZBA,  "d,s,>", MATCH_SLLI_UW, MASK_SLLI_UW, match_opcode, 0 },

/* Zbc instructions */
{"clmul",      0, INSN_CLASS_ZBC,  "d,s,t", MATCH_CLMUL, MASK_CLMUL, match_opcode, 0 },
{"clmulh",     0, INSN_CLASS_ZBC,  "d,s,t", MATCH_CLMULH, MASK_CLMULH, match_opcode, 0 },
{"clmulr",     0, INSN_CLASS_ZBC,  "d,s,t", MATCH_CLMULR, MASK_CLMULR, match_opcode, 0 },

/* Zbs instructions */
{"bclri",     0, INSN_CLASS_ZBS,   "d,s,>",  MATCH_BCLRI, MASK_BCLRI, match_opcode, 0 },
{"bseti",     0, INSN_CLASS_ZBS,   "d,s,>",  MATCH_BSETI, MASK_BSETI, match_opcode, 0 },
{"binvi",     0, INSN_CLASS_ZBS,   "d,s,>",  MATCH_BINVI, MASK_BINVI, match_opcode, 0 },
{"bexti",     0, INSN_CLASS_ZBS,   "d,s,>",  MATCH_BEXTI, MASK_BEXTI, match_opcode, 0 },
{"bclr",      0, INSN_CLASS_ZBS,   "d,s,t",  MATCH_BCLR, MASK_BCLR, match_opcode, 0 },
{"bclr",      0, INSN_CLASS_ZBS,   "d,s,>",  MATCH_BCLRI, MASK_BCLRI, match_opcode, INSN_ALIAS },
{"bset",      0, INSN_CLASS_ZBS,   "d,s,t",  MATCH_BSET, MASK_BSET, match_opcode, 0 },
{"bset",      0, INSN_CLASS_ZBS,   "d,s,>",  MATCH_BSETI, MASK_BSETI, match_opcode, INSN_ALIAS },
{"binv",      0, INSN_CLASS_ZBS,   "d,s,t",  MATCH_BINV, MASK_BINV, match_opcode, 0 },
{"binv",      0, INSN_CLASS_ZBS,   "d,s,>",  MATCH_BINVI, MASK_BINVI, match_opcode, INSN_ALIAS },
{"bext",      0, INSN_CLASS_ZBS,   "d,s,t",  MATCH_BEXT, MASK_BEXT, match_opcode, 0 },
{"bext",      0, INSN_CLASS_ZBS,   "d,s,>",  MATCH_BEXTI, MASK_BEXTI, match_opcode, INSN_ALIAS },

/* Terminate the list.  */
{0, 0, INSN_CLASS_NONE, 0, 0, 0, 0, 0}
};

/* Instruction format for .insn directive.  */
const struct riscv_opcode riscv_insn_types[] =
{
/* name, xlen, isa, operands, match, mask, match_func, pinfo.  */
{"r",       0, INSN_CLASS_I,       "O4,F3,F7,d,s,t",    0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F7,D,s,t",    0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F7,d,S,t",    0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F7,D,S,t",    0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F7,d,s,T",    0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F7,D,s,T",    0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F7,d,S,T",    0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F7,D,S,T",    0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_I,       "O4,F3,F2,d,s,t,r",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,D,s,t,r",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,d,S,t,r",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,D,S,t,r",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,d,s,T,r",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,D,s,T,r",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,d,S,T,r",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,D,S,T,r",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,d,s,t,R",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,D,s,t,R",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,d,S,t,R",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,D,S,t,R",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,d,s,T,R",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,D,s,T,R",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,d,S,T,R",  0, 0, match_opcode, 0 },
{"r",       0, INSN_CLASS_F,       "O4,F3,F2,D,S,T,R",  0, 0, match_opcode, 0 },

{"r4",      0, INSN_CLASS_I,       "O4,F3,F2,d,s,t,r",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,D,s,t,r",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,d,S,t,r",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,D,S,t,r",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,d,s,T,r",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,D,s,T,r",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,d,S,T,r",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,D,S,T,r",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,d,s,t,R",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,D,s,t,R",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,d,S,t,R",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,D,S,t,R",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,d,s,T,R",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,D,s,T,R",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,d,S,T,R",  0, 0, match_opcode, 0 },
{"r4",      0, INSN_CLASS_F,       "O4,F3,F2,D,S,T,R",  0, 0, match_opcode, 0 },

{"i",       0, INSN_CLASS_I,       "O4,F3,d,s,j",       0, 0, match_opcode, 0 },
{"i",       0, INSN_CLASS_F,       "O4,F3,D,s,j",       0, 0, match_opcode, 0 },
{"i",       0, INSN_CLASS_F,       "O4,F3,d,S,j",       0, 0, match_opcode, 0 },
{"i",       0, INSN_CLASS_F,       "O4,F3,D,S,j",       0, 0, match_opcode, 0 },
{"i",       0, INSN_CLASS_I,       "O4,F3,d,o(s)",      0, 0, match_opcode, 0 },
{"i",       0, INSN_CLASS_F,       "O4,F3,D,o(s)",      0, 0, match_opcode, 0 },

{"s",       0, INSN_CLASS_I,       "O4,F3,t,q(s)",      0, 0, match_opcode, 0 },
{"s",       0, INSN_CLASS_F,       "O4,F3,T,q(s)",      0, 0, match_opcode, 0 },

{"sb",      0, INSN_CLASS_I,       "O4,F3,s,t,p",       0, 0, match_opcode, 0 },
{"sb",      0, INSN_CLASS_F,       "O4,F3,S,t,p",       0, 0, match_opcode, 0 },
{"sb",      0, INSN_CLASS_F,       "O4,F3,s,T,p",       0, 0, match_opcode, 0 },
{"sb",      0, INSN_CLASS_F,       "O4,F3,S,T,p",       0, 0, match_opcode, 0 },
{"b",       0, INSN_CLASS_I,       "O4,F3,s,t,p",       0, 0, match_opcode, 0 },
{"b",       0, INSN_CLASS_F,       "O4,F3,S,t,p",       0, 0, match_opcode, 0 },
{"b",       0, INSN_CLASS_F,       "O4,F3,s,T,p",       0, 0, match_opcode, 0 },
{"b",       0, INSN_CLASS_F,       "O4,F3,S,T,p",       0, 0, match_opcode, 0 },

{"u",       0, INSN_CLASS_I,       "O4,d,u",            0, 0, match_opcode, 0 },
{"u",       0, INSN_CLASS_F,       "O4,D,u",            0, 0, match_opcode, 0 },

{"uj",      0, INSN_CLASS_I,       "O4,d,a",            0, 0, match_opcode, 0 },
{"uj",      0, INSN_CLASS_F,       "O4,D,a",            0, 0, match_opcode, 0 },
{"j",       0, INSN_CLASS_I,       "O4,d,a",            0, 0, match_opcode, 0 },
{"j",       0, INSN_CLASS_F,       "O4,D,a",            0, 0, match_opcode, 0 },

{"cr",      0, INSN_CLASS_C,       "O2,CF4,d,CV",       0, 0, match_opcode, 0 },
{"cr",      0, INSN_CLASS_F_AND_C, "O2,CF4,D,CV",       0, 0, match_opcode, 0 },
{"cr",      0, INSN_CLASS_F_AND_C, "O2,CF4,d,CT",       0, 0, match_opcode, 0 },
{"cr",      0, INSN_CLASS_F_AND_C, "O2,CF4,D,CT",       0, 0, match_opcode, 0 },

{"ci",      0, INSN_CLASS_C,       "O2,CF3,d,Co",       0, 0, match_opcode, 0 },
{"ci",      0, INSN_CLASS_F_AND_C, "O2,CF3,D,Co",       0, 0, match_opcode, 0 },

{"ciw",     0, INSN_CLASS_C,       "O2,CF3,Ct,C8",      0, 0, match_opcode, 0 },
{"ciw",     0, INSN_CLASS_F_AND_C, "O2,CF3,CD,C8",      0, 0, match_opcode, 0 },

{"css",     0, INSN_CLASS_C,       "O2,CF3,CV,C6",      0, 0, match_opcode, 0 },
{"css",     0, INSN_CLASS_F_AND_C, "O2,CF3,CT,C6",      0, 0, match_opcode, 0 },

{"cl",      0, INSN_CLASS_C,       "O2,CF3,Ct,C5(Cs)",  0, 0, match_opcode, 0 },
{"cl",      0, INSN_CLASS_F_AND_C, "O2,CF3,CD,C5(Cs)",  0, 0, match_opcode, 0 },
{"cl",      0, INSN_CLASS_F_AND_C, "O2,CF3,Ct,C5(CS)",  0, 0, match_opcode, 0 },
{"cl",      0, INSN_CLASS_F_AND_C, "O2,CF3,CD,C5(CS)",  0, 0, match_opcode, 0 },

{"cs",      0, INSN_CLASS_C,       "O2,CF3,Ct,C5(Cs)",  0, 0, match_opcode, 0 },
{"cs",      0, INSN_CLASS_F_AND_C, "O2,CF3,CD,C5(Cs)",  0, 0, match_opcode, 0 },
{"cs",      0, INSN_CLASS_F_AND_C, "O2,CF3,Ct,C5(CS)",  0, 0, match_opcode, 0 },
{"cs",      0, INSN_CLASS_F_AND_C, "O2,CF3,CD,C5(CS)",  0, 0, match_opcode, 0 },

{"ca",      0, INSN_CLASS_C,       "O2,CF6,CF2,Cs,Ct",  0, 0, match_opcode, 0 },
{"ca",      0, INSN_CLASS_F_AND_C, "O2,CF6,CF2,CS,Ct",  0, 0, match_opcode, 0 },
{"ca",      0, INSN_CLASS_F_AND_C, "O2,CF6,CF2,Cs,CD",  0, 0, match_opcode, 0 },
{"ca",      0, INSN_CLASS_F_AND_C, "O2,CF6,CF2,CS,CD",  0, 0, match_opcode, 0 },

{"cb",      0, INSN_CLASS_C,       "O2,CF3,Cs,Cp",      0, 0, match_opcode, 0 },
{"cb",      0, INSN_CLASS_F_AND_C, "O2,CF3,CS,Cp",      0, 0, match_opcode, 0 },

{"cj",      0, INSN_CLASS_C,       "O2,CF3,Ca",         0, 0, match_opcode, 0 },

/* Terminate the list.  */
{0, 0, INSN_CLASS_NONE, 0, 0, 0, 0, 0}
};

/* Extended extensions.  */

/* RVV registers.  */
const char * const riscv_vecr_names_numeric[NVECR] =
{
  "v0",   "v1",   "v2",   "v3",   "v4",   "v5",   "v6",   "v7",
  "v8",   "v9",   "v10",  "v11",  "v12",  "v13",  "v14",  "v15",
  "v16",  "v17",  "v18",  "v19",  "v20",  "v21",  "v22",  "v23",
  "v24",  "v25",  "v26",  "v27",  "v28",  "v29",  "v30",  "v31"
};

/* RVV mask registers.  */
const char * const riscv_vecm_names_numeric[NVECM] =
{
  "v0.t"
};

/* The vsetvli vsew constants.  */
const char * const riscv_vsew[8] =
{
  "e8", "e16", "e32", "e64", "e128", "e256", "e512", "e1024"
};

/* The vsetvli vlmul constants.  */
const char * const riscv_vlmul[8] =
{
  "m1", "m2", "m4", "m8", 0, "mf8", "mf4", "mf2"
};

/* The vsetvli vta constants.  */
const char * const riscv_vta[2] =
{
  "tu", "ta"
};

/* The vsetvli vma constants.  */
const char * const riscv_vma[2] =
{
  "mu", "ma"
};

#define MASK_VD  (OP_MASK_VD << OP_SH_VD)
#define MASK_VS1 (OP_MASK_VS1 << OP_SH_VS1)
#define MASK_VS2 (OP_MASK_VS2 << OP_SH_VS2)
#define MASK_VMASK (OP_MASK_VMASK << OP_SH_VMASK)

static int
match_vs1_eq_vs2 (const struct riscv_opcode *op,
		  insn_t insn,
		  int constraints ATTRIBUTE_UNUSED,
		  const char **error ATTRIBUTE_UNUSED)
{
  int vs1 = (insn & MASK_VS1) >> OP_SH_VS1;
  int vs2 = (insn & MASK_VS2) >> OP_SH_VS2;

  return match_opcode (op, insn, 0, NULL) && vs1 == vs2;
}

static int
match_vs1_eq_vs2_neq_vm (const struct riscv_opcode *op,
			 insn_t insn,
			 int constraints,
			 const char **error)
{
  int vd = (insn & MASK_VD) >> OP_SH_VD;
  int vs1 = (insn & MASK_VS1) >> OP_SH_VS1;
  int vs2 = (insn & MASK_VS2) >> OP_SH_VS2;
  int vm = (insn & MASK_VMASK) >> OP_SH_VMASK;

  if (!constraints || error == NULL)
    return match_opcode (op, insn, 0, NULL) && vs1 == vs2;

  if (!vm && vm == vd)
    *error = "illegal operands vd cannot overlap vm";
  else
    return match_opcode (op, insn, 0, NULL) && vs1 == vs2;
  return 0;
}

static int
match_vd_eq_vs1_eq_vs2 (const struct riscv_opcode *op,
			insn_t insn,
			int constraints ATTRIBUTE_UNUSED,
			const char **error ATTRIBUTE_UNUSED)
{
  int vd =  (insn & MASK_VD) >> OP_SH_VD;
  int vs1 = (insn & MASK_VS1) >> OP_SH_VS1;
  int vs2 = (insn & MASK_VS2) >> OP_SH_VS2;

  return match_opcode (op, insn, 0, NULL) && vd == vs1 && vs1 == vs2;
}

/* These are used to check the vector constraints.  */

static int
match_vd_neq_vs1_neq_vs2 (const struct riscv_opcode *op,
			  insn_t insn,
			  int constraints,
			  const char **error)
{
  int vd = (insn & MASK_VD) >> OP_SH_VD;
  int vs1 = (insn & MASK_VS1) >> OP_SH_VS1;
  int vs2 = (insn & MASK_VS2) >> OP_SH_VS2;

  if (!constraints || error == NULL)
    return match_opcode (op, insn, 0, NULL);

  if (vs1 == vd)
    *error = "illegal operands vd cannot overlap vs1";
  else if (vs2 == vd)
    *error = "illegal operands vd cannot overlap vs2";
  else
    return match_opcode (op, insn, 0, NULL);
  return 0;
}

static int
match_vd_neq_vs1_neq_vs2_neq_vm (const struct riscv_opcode *op,
				 insn_t insn,
				 int constraints,
				 const char **error)
{
  int vd = (insn & MASK_VD) >> OP_SH_VD;
  int vs1 = (insn & MASK_VS1) >> OP_SH_VS1;
  int vs2 = (insn & MASK_VS2) >> OP_SH_VS2;
  int vm = (insn & MASK_VMASK) >> OP_SH_VMASK;

  if (!constraints || error == NULL)
    return match_opcode (op, insn, 0, NULL);

  if (vs1 == vd)
    *error = "illegal operands vd cannot overlap vs1";
  else if (vs2 == vd)
    *error = "illegal operands vd cannot overlap vs2";
  else if (!vm && vm == vd)
    *error = "illegal operands vd cannot overlap vm";
  else
    return match_opcode (op, insn, 0, NULL);
  return 0;
}

static int
match_vd_neq_vs1_neq_vm (const struct riscv_opcode *op,
			 insn_t insn,
			 int constraints,
			 const char **error)
{
  int vd = (insn & MASK_VD) >> OP_SH_VD;
  int vs1 = (insn & MASK_VS1) >> OP_SH_VS1;
  int vm = (insn & MASK_VMASK) >> OP_SH_VMASK;

  if (!constraints || error == NULL)
    return match_opcode (op, insn, 0, NULL);

  if (vs1 == vd)
    *error = "illegal operands vd cannot overlap vs1";
  else if (!vm && vm == vd)
    *error = "illegal operands vd cannot overlap vm";
  else
    return match_opcode (op, insn, 0, NULL);
  return 0;
}

static int
match_vd_neq_vs2_neq_vm (const struct riscv_opcode *op,
			 insn_t insn,
			 int constraints,
			 const char **error)
{
  int vd = (insn & MASK_VD) >> OP_SH_VD;
  int vs2 = (insn & MASK_VS2) >> OP_SH_VS2;
  int vm = (insn & MASK_VMASK) >> OP_SH_VMASK;

  if (!constraints || error == NULL)
    return match_opcode (op, insn, 0, NULL);

  if (vs2 == vd)
    *error = "illegal operands vd cannot overlap vs2";
  else if (!vm && vm == vd)
    *error = "illegal operands vd cannot overlap vm";
  else
    return match_opcode (op, insn, 0, NULL);
  return 0;
}

/* v[m]adc and v[m]sbc use the vm encoding to encode the
   carry-in v0 register.  The carry-in v0 register can not
   overlap with the vd, too.  Therefore, we use the same
   match_vd_neq_vm to check the overlap constraints.  */

static int
match_vd_neq_vm (const struct riscv_opcode *op,
		 insn_t insn,
		 int constraints,
		 const char **error)
{
  int vd = (insn & MASK_VD) >> OP_SH_VD;
  int vm = (insn & MASK_VMASK) >> OP_SH_VMASK;

  if (!constraints || error == NULL)
    return match_opcode (op, insn, 0, NULL);

  if (!vm && vm == vd)
    *error = "illegal operands vd cannot overlap vm";
  else
    return match_opcode (op, insn, 0, NULL);
  return 0;
}

static int
match_vls_nf_rv (const struct riscv_opcode *op,
		 insn_t insn,
		 int constraints,
		 const char **error)
{
  int vd = (insn & MASK_VD) >> OP_SH_VD;
  int nf = ((insn & (0x7 << 29) ) >> 29) + 1;

  if (!constraints || error == NULL)
    return match_opcode (op, insn, 0, NULL);

  if ((vd % nf) != 0)
    *error = "illegal operands vd must be multiple of nf";
  else
    return match_opcode (op, insn, 0, NULL);
  return 0;
}

static int
match_vmv_nf_rv (const struct riscv_opcode *op,
		 insn_t insn,
		 int constraints,
		 const char **error)
{
  int vd = (insn & MASK_VD) >> OP_SH_VD;
  int vs2 = (insn & MASK_VS2) >> OP_SH_VS2;
  int nf = ((insn & (0x7 << 15) ) >> 15) + 1;

  if (!constraints || error == NULL)
    return match_opcode (op, insn, 0, NULL);

  if ((vd % nf) != 0)
    *error = "illegal operands vd must be multiple of nf";
  else if ((vs2 % nf) != 0)
    *error = "illegal operands vs2 must be multiple of nf";
  else
    return match_opcode (op, insn, 0, NULL);
  return 0;
}

/* Draft extensions.  */
const struct riscv_opcode riscv_draft_opcodes[] =
{
/* name, xlen, isa, operands, match, mask, match_func, pinfo.  */
/* Half-precision floating-point instruction subset.  */
{"flh",        0, INSN_CLASS_ZFH,  "D,o(s)",    MATCH_FLH, MASK_FLH, match_opcode, INSN_DREF|INSN_2_BYTE },
{"flh",        0, INSN_CLASS_ZFH,  "D,A,s",     0, (int) M_FLH, match_never, INSN_MACRO },
{"fsh",        0, INSN_CLASS_ZFH,  "T,q(s)",    MATCH_FSH, MASK_FSH, match_opcode, INSN_DREF|INSN_2_BYTE },
{"fsh",        0, INSN_CLASS_ZFH,  "T,A,s",     0, (int) M_FSH, match_never, INSN_MACRO },
{"fmv.h",      0, INSN_CLASS_ZFH,  "D,U",       MATCH_FSGNJ_H, MASK_FSGNJ_H, match_rs1_eq_rs2, INSN_ALIAS },
{"fneg.h",     0, INSN_CLASS_ZFH,  "D,U",       MATCH_FSGNJN_H, MASK_FSGNJN_H, match_rs1_eq_rs2, INSN_ALIAS },
{"fabs.h",     0, INSN_CLASS_ZFH,  "D,U",       MATCH_FSGNJX_H, MASK_FSGNJX_H, match_rs1_eq_rs2, INSN_ALIAS },
{"fsgnj.h",    0, INSN_CLASS_ZFH,  "D,S,T",     MATCH_FSGNJ_H, MASK_FSGNJ_H, match_opcode, 0 },
{"fsgnjn.h",   0, INSN_CLASS_ZFH,  "D,S,T",     MATCH_FSGNJN_H, MASK_FSGNJN_H, match_opcode, 0 },
{"fsgnjx.h",   0, INSN_CLASS_ZFH,  "D,S,T",     MATCH_FSGNJX_H, MASK_FSGNJX_H, match_opcode, 0 },
{"fadd.h",     0, INSN_CLASS_ZFH,  "D,S,T",     MATCH_FADD_H|MASK_RM, MASK_FADD_H|MASK_RM, match_opcode, 0 },
{"fadd.h",     0, INSN_CLASS_ZFH,  "D,S,T,m",   MATCH_FADD_H, MASK_FADD_H, match_opcode, 0 },
{"fsub.h",     0, INSN_CLASS_ZFH,  "D,S,T",     MATCH_FSUB_H|MASK_RM, MASK_FSUB_H|MASK_RM, match_opcode, 0 },
{"fsub.h",     0, INSN_CLASS_ZFH,  "D,S,T,m",   MATCH_FSUB_H, MASK_FSUB_H, match_opcode, 0 },
{"fmul.h",     0, INSN_CLASS_ZFH,  "D,S,T",     MATCH_FMUL_H|MASK_RM, MASK_FMUL_H|MASK_RM, match_opcode, 0 },
{"fmul.h",     0, INSN_CLASS_ZFH,  "D,S,T,m",   MATCH_FMUL_H, MASK_FMUL_H, match_opcode, 0 },
{"fdiv.h",     0, INSN_CLASS_ZFH,  "D,S,T",     MATCH_FDIV_H|MASK_RM, MASK_FDIV_H|MASK_RM, match_opcode, 0 },
{"fdiv.h",     0, INSN_CLASS_ZFH,  "D,S,T,m",   MATCH_FDIV_H, MASK_FDIV_H, match_opcode, 0 },
{"fsqrt.h",    0, INSN_CLASS_ZFH,  "D,S",       MATCH_FSQRT_H|MASK_RM, MASK_FSQRT_H|MASK_RM, match_opcode, 0 },
{"fsqrt.h",    0, INSN_CLASS_ZFH,  "D,S,m",     MATCH_FSQRT_H, MASK_FSQRT_H, match_opcode, 0 },
{"fmin.h",     0, INSN_CLASS_ZFH,  "D,S,T",     MATCH_FMIN_H, MASK_FMIN_H, match_opcode, 0 },
{"fmax.h",     0, INSN_CLASS_ZFH,  "D,S,T",     MATCH_FMAX_H, MASK_FMAX_H, match_opcode, 0 },
{"fmadd.h",    0, INSN_CLASS_ZFH,  "D,S,T,R",   MATCH_FMADD_H|MASK_RM, MASK_FMADD_H|MASK_RM, match_opcode, 0 },
{"fmadd.h",    0, INSN_CLASS_ZFH,  "D,S,T,R,m", MATCH_FMADD_H, MASK_FMADD_H, match_opcode, 0 },
{"fnmadd.h",   0, INSN_CLASS_ZFH,  "D,S,T,R",   MATCH_FNMADD_H|MASK_RM, MASK_FNMADD_H|MASK_RM, match_opcode, 0 },
{"fnmadd.h",   0, INSN_CLASS_ZFH,  "D,S,T,R,m", MATCH_FNMADD_H, MASK_FNMADD_H, match_opcode, 0 },
{"fmsub.h",    0, INSN_CLASS_ZFH,  "D,S,T,R",   MATCH_FMSUB_H|MASK_RM, MASK_FMSUB_H|MASK_RM, match_opcode, 0 },
{"fmsub.h",    0, INSN_CLASS_ZFH,  "D,S,T,R,m", MATCH_FMSUB_H, MASK_FMSUB_H, match_opcode, 0 },
{"fnmsub.h",   0, INSN_CLASS_ZFH,  "D,S,T,R",   MATCH_FNMSUB_H|MASK_RM, MASK_FNMSUB_H|MASK_RM, match_opcode, 0 },
{"fnmsub.h",   0, INSN_CLASS_ZFH,  "D,S,T,R,m", MATCH_FNMSUB_H, MASK_FNMSUB_H, match_opcode, 0 },
{"fcvt.w.h",   0, INSN_CLASS_ZFH,  "d,S",       MATCH_FCVT_W_H|MASK_RM, MASK_FCVT_W_H|MASK_RM, match_opcode, 0 },
{"fcvt.w.h",   0, INSN_CLASS_ZFH,  "d,S,m",     MATCH_FCVT_W_H, MASK_FCVT_W_H, match_opcode, 0 },
{"fcvt.wu.h",  0, INSN_CLASS_ZFH,  "d,S",       MATCH_FCVT_WU_H|MASK_RM, MASK_FCVT_WU_H|MASK_RM, match_opcode, 0 },
{"fcvt.wu.h",  0, INSN_CLASS_ZFH,  "d,S,m",     MATCH_FCVT_WU_H, MASK_FCVT_WU_H, match_opcode, 0 },
{"fcvt.h.w",   0, INSN_CLASS_ZFH,  "D,s",       MATCH_FCVT_H_W|MASK_RM, MASK_FCVT_H_W|MASK_RM, match_opcode, 0 },
{"fcvt.h.w",   0, INSN_CLASS_ZFH,  "D,s,m",     MATCH_FCVT_H_W, MASK_FCVT_H_W, match_opcode, 0 },
{"fcvt.h.wu",  0, INSN_CLASS_ZFH,  "D,s",       MATCH_FCVT_H_WU|MASK_RM, MASK_FCVT_H_WU|MASK_RM, match_opcode, 0 },
{"fcvt.h.wu",  0, INSN_CLASS_ZFH,  "D,s,m",     MATCH_FCVT_H_WU, MASK_FCVT_H_WU, match_opcode, 0 },
{"fcvt.s.h",   0, INSN_CLASS_ZFH,  "D,S",       MATCH_FCVT_S_H, MASK_FCVT_S_H|MASK_RM, match_opcode, 0 },
{"fcvt.d.h",   0, INSN_CLASS_D_AND_ZFH,  "D,S",       MATCH_FCVT_D_H, MASK_FCVT_D_H|MASK_RM, match_opcode, 0 },
{"fcvt.q.h",   0, INSN_CLASS_Q_AND_ZFH,  "D,S",       MATCH_FCVT_Q_H, MASK_FCVT_Q_H|MASK_RM, match_opcode, 0 },
{"fcvt.h.s",   0, INSN_CLASS_ZFH,  "D,S",       MATCH_FCVT_H_S|MASK_RM, MASK_FCVT_H_S|MASK_RM, match_opcode, 0 },
{"fcvt.h.s",   0, INSN_CLASS_ZFH,  "D,S,m",     MATCH_FCVT_H_S, MASK_FCVT_H_S, match_opcode, 0 },
{"fcvt.h.d",   0, INSN_CLASS_D_AND_ZFH,  "D,S",       MATCH_FCVT_H_D|MASK_RM, MASK_FCVT_H_D|MASK_RM, match_opcode, 0 },
{"fcvt.h.d",   0, INSN_CLASS_D_AND_ZFH,  "D,S,m",     MATCH_FCVT_H_D, MASK_FCVT_H_D, match_opcode, 0 },
{"fcvt.h.q",   0, INSN_CLASS_Q_AND_ZFH,  "D,S",       MATCH_FCVT_H_Q|MASK_RM, MASK_FCVT_H_Q|MASK_RM, match_opcode, 0 },
{"fcvt.h.q",   0, INSN_CLASS_Q_AND_ZFH,  "D,S,m",     MATCH_FCVT_H_Q, MASK_FCVT_H_Q, match_opcode, 0 },
{"fclass.h",   0, INSN_CLASS_ZFH,  "d,S",       MATCH_FCLASS_H, MASK_FCLASS_H, match_opcode, 0 },
{"feq.h",      0, INSN_CLASS_ZFH,  "d,S,T",     MATCH_FEQ_H, MASK_FEQ_H, match_opcode, 0 },
{"flt.h",      0, INSN_CLASS_ZFH,  "d,S,T",     MATCH_FLT_H, MASK_FLT_H, match_opcode, 0 },
{"fle.h",      0, INSN_CLASS_ZFH,  "d,S,T",     MATCH_FLE_H, MASK_FLE_H, match_opcode, 0 },
{"fgt.h",      0, INSN_CLASS_ZFH,  "d,T,S",     MATCH_FLT_H, MASK_FLT_H, match_opcode, 0 },
{"fge.h",      0, INSN_CLASS_ZFH,  "d,T,S",     MATCH_FLE_H, MASK_FLE_H, match_opcode, 0 },
{"fmv.x.h",    0, INSN_CLASS_ZFH,  "d,S",       MATCH_FMV_X_H, MASK_FMV_X_H, match_opcode, 0 },
{"fmv.h.x",    0, INSN_CLASS_ZFH,  "D,s",       MATCH_FMV_H_X, MASK_FMV_H_X, match_opcode, 0 },
{"fcvt.l.h",  64, INSN_CLASS_ZFH,  "d,S",       MATCH_FCVT_L_H|MASK_RM, MASK_FCVT_L_H|MASK_RM, match_opcode, 0 },
{"fcvt.l.h",  64, INSN_CLASS_ZFH,  "d,S,m",     MATCH_FCVT_L_H, MASK_FCVT_L_H, match_opcode, 0 },
{"fcvt.lu.h", 64, INSN_CLASS_ZFH,  "d,S",       MATCH_FCVT_LU_H|MASK_RM, MASK_FCVT_LU_H|MASK_RM, match_opcode, 0 },
{"fcvt.lu.h", 64, INSN_CLASS_ZFH,  "d,S,m",     MATCH_FCVT_LU_H, MASK_FCVT_LU_H, match_opcode, 0 },
{"fcvt.h.l",  64, INSN_CLASS_ZFH,  "D,s",       MATCH_FCVT_H_L|MASK_RM, MASK_FCVT_H_L|MASK_RM, match_opcode, 0 },
{"fcvt.h.l",  64, INSN_CLASS_ZFH,  "D,s,m",     MATCH_FCVT_H_L, MASK_FCVT_H_L, match_opcode, 0 },
{"fcvt.h.lu", 64, INSN_CLASS_ZFH,  "D,s",       MATCH_FCVT_H_LU|MASK_RM, MASK_FCVT_H_L|MASK_RM, match_opcode, 0 },
{"fcvt.h.lu", 64, INSN_CLASS_ZFH,  "D,s,m",     MATCH_FCVT_H_LU, MASK_FCVT_H_LU, match_opcode, 0 },

/* RVV */
{"vsetvl",     0, INSN_CLASS_V,  "d,s,t",  MATCH_VSETVL, MASK_VSETVL, match_opcode, 0},
{"vsetvli",    0, INSN_CLASS_V,  "d,s,Vc", MATCH_VSETVLI, MASK_VSETVLI, match_opcode, 0},
{"vsetivli",   0, INSN_CLASS_V,  "d,Z,Vb", MATCH_VSETIVLI, MASK_VSETIVLI, match_opcode, 0},

{"vlm.v",      0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VLMV, MASK_VLMV, match_opcode, INSN_DREF },
{"vsm.v",      0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VSMV, MASK_VSMV, match_opcode, INSN_DREF },
{"vle1.v",     0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VLMV, MASK_VLMV, match_opcode, INSN_DREF|INSN_ALIAS },
{"vse1.v",     0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VSMV, MASK_VSMV, match_opcode, INSN_DREF|INSN_ALIAS },

{"vle8.v",     0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VLE8V, MASK_VLE8V, match_vd_neq_vm, INSN_DREF },
{"vle16.v",    0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VLE16V, MASK_VLE16V, match_vd_neq_vm, INSN_DREF },
{"vle32.v",    0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VLE32V, MASK_VLE32V, match_vd_neq_vm, INSN_DREF },
{"vle64.v",    0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VLE64V, MASK_VLE64V, match_vd_neq_vm, INSN_DREF },

{"vse8.v",     0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VSE8V, MASK_VSE8V, match_vd_neq_vm, INSN_DREF },
{"vse16.v",    0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VSE16V, MASK_VSE16V, match_vd_neq_vm, INSN_DREF },
{"vse32.v",    0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VSE32V, MASK_VSE32V, match_vd_neq_vm, INSN_DREF },
{"vse64.v",    0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VSE64V, MASK_VSE64V, match_vd_neq_vm, INSN_DREF },

{"vlse8.v",    0, INSN_CLASS_V,  "Vd,0(s),tVm", MATCH_VLSE8V, MASK_VLSE8V, match_vd_neq_vm, INSN_DREF },
{"vlse16.v",   0, INSN_CLASS_V,  "Vd,0(s),tVm", MATCH_VLSE16V, MASK_VLSE16V, match_vd_neq_vm, INSN_DREF },
{"vlse32.v",   0, INSN_CLASS_V,  "Vd,0(s),tVm", MATCH_VLSE32V, MASK_VLSE32V, match_vd_neq_vm, INSN_DREF },
{"vlse64.v",   0, INSN_CLASS_V,  "Vd,0(s),tVm", MATCH_VLSE64V, MASK_VLSE64V, match_vd_neq_vm, INSN_DREF },

{"vsse8.v",    0, INSN_CLASS_V,  "Vd,0(s),tVm", MATCH_VSSE8V, MASK_VSSE8V, match_vd_neq_vm, INSN_DREF },
{"vsse16.v",   0, INSN_CLASS_V,  "Vd,0(s),tVm", MATCH_VSSE16V, MASK_VSSE16V, match_vd_neq_vm, INSN_DREF },
{"vsse32.v",   0, INSN_CLASS_V,  "Vd,0(s),tVm", MATCH_VSSE32V, MASK_VSSE32V, match_vd_neq_vm, INSN_DREF },
{"vsse64.v",   0, INSN_CLASS_V,  "Vd,0(s),tVm", MATCH_VSSE64V, MASK_VSSE64V, match_vd_neq_vm, INSN_DREF },

{"vloxei8.v",   0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VLOXEI8V, MASK_VLOXEI8V, match_vd_neq_vm, INSN_DREF },
{"vloxei16.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VLOXEI16V, MASK_VLOXEI16V, match_vd_neq_vm, INSN_DREF },
{"vloxei32.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VLOXEI32V, MASK_VLOXEI32V, match_vd_neq_vm, INSN_DREF },
{"vloxei64.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VLOXEI64V, MASK_VLOXEI64V, match_vd_neq_vm, INSN_DREF },

{"vsoxei8.v",   0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VSOXEI8V, MASK_VSOXEI8V, match_vd_neq_vm, INSN_DREF },
{"vsoxei16.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VSOXEI16V, MASK_VSOXEI16V, match_vd_neq_vm, INSN_DREF },
{"vsoxei32.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VSOXEI32V, MASK_VSOXEI32V, match_vd_neq_vm, INSN_DREF },
{"vsoxei64.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VSOXEI64V, MASK_VSOXEI64V, match_vd_neq_vm, INSN_DREF },

{"vluxei8.v",   0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VLUXEI8V, MASK_VLUXEI8V, match_vd_neq_vm, INSN_DREF },
{"vluxei16.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VLUXEI16V, MASK_VLUXEI16V, match_vd_neq_vm, INSN_DREF },
{"vluxei32.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VLUXEI32V, MASK_VLUXEI32V, match_vd_neq_vm, INSN_DREF },
{"vluxei64.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VLUXEI64V, MASK_VLUXEI64V, match_vd_neq_vm, INSN_DREF },

{"vsuxei8.v",   0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VSUXEI8V, MASK_VSUXEI8V, match_vd_neq_vm, INSN_DREF },
{"vsuxei16.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VSUXEI16V, MASK_VSUXEI16V, match_vd_neq_vm, INSN_DREF },
{"vsuxei32.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VSUXEI32V, MASK_VSUXEI32V, match_vd_neq_vm, INSN_DREF },
{"vsuxei64.v",  0, INSN_CLASS_V,  "Vd,0(s),VtVm", MATCH_VSUXEI64V, MASK_VSUXEI64V, match_vd_neq_vm, INSN_DREF },

{"vle8ff.v",    0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VLE8FFV, MASK_VLE8FFV, match_vd_neq_vm, INSN_DREF },
{"vle16ff.v",   0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VLE16FFV, MASK_VLE16FFV, match_vd_neq_vm, INSN_DREF },
{"vle32ff.v",   0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VLE32FFV, MASK_VLE32FFV, match_vd_neq_vm, INSN_DREF },
{"vle64ff.v",   0, INSN_CLASS_V,  "Vd,0(s)Vm", MATCH_VLE64FFV, MASK_VLE64FFV, match_vd_neq_vm, INSN_DREF },

{"vlseg2e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG2E8V, MASK_VLSEG2E8V, match_vd_neq_vm, INSN_DREF },
{"vsseg2e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG2E8V, MASK_VSSEG2E8V, match_vd_neq_vm, INSN_DREF },
{"vlseg3e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG3E8V, MASK_VLSEG3E8V, match_vd_neq_vm, INSN_DREF },
{"vsseg3e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG3E8V, MASK_VSSEG3E8V, match_vd_neq_vm, INSN_DREF },
{"vlseg4e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG4E8V, MASK_VLSEG4E8V, match_vd_neq_vm, INSN_DREF },
{"vsseg4e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG4E8V, MASK_VSSEG4E8V, match_vd_neq_vm, INSN_DREF },
{"vlseg5e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG5E8V, MASK_VLSEG5E8V, match_vd_neq_vm, INSN_DREF },
{"vsseg5e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG5E8V, MASK_VSSEG5E8V, match_vd_neq_vm, INSN_DREF },
{"vlseg6e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG6E8V, MASK_VLSEG6E8V, match_vd_neq_vm, INSN_DREF },
{"vsseg6e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG6E8V, MASK_VSSEG6E8V, match_vd_neq_vm, INSN_DREF },
{"vlseg7e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG7E8V, MASK_VLSEG7E8V, match_vd_neq_vm, INSN_DREF },
{"vsseg7e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG7E8V, MASK_VSSEG7E8V, match_vd_neq_vm, INSN_DREF },
{"vlseg8e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG8E8V, MASK_VLSEG8E8V, match_vd_neq_vm, INSN_DREF },
{"vsseg8e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG8E8V, MASK_VSSEG8E8V, match_vd_neq_vm, INSN_DREF },

{"vlseg2e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG2E16V, MASK_VLSEG2E16V, match_vd_neq_vm, INSN_DREF },
{"vsseg2e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG2E16V, MASK_VSSEG2E16V, match_vd_neq_vm, INSN_DREF },
{"vlseg3e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG3E16V, MASK_VLSEG3E16V, match_vd_neq_vm, INSN_DREF },
{"vsseg3e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG3E16V, MASK_VSSEG3E16V, match_vd_neq_vm, INSN_DREF },
{"vlseg4e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG4E16V, MASK_VLSEG4E16V, match_vd_neq_vm, INSN_DREF },
{"vsseg4e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG4E16V, MASK_VSSEG4E16V, match_vd_neq_vm, INSN_DREF },
{"vlseg5e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG5E16V, MASK_VLSEG5E16V, match_vd_neq_vm, INSN_DREF },
{"vsseg5e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG5E16V, MASK_VSSEG5E16V, match_vd_neq_vm, INSN_DREF },
{"vlseg6e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG6E16V, MASK_VLSEG6E16V, match_vd_neq_vm, INSN_DREF },
{"vsseg6e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG6E16V, MASK_VSSEG6E16V, match_vd_neq_vm, INSN_DREF },
{"vlseg7e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG7E16V, MASK_VLSEG7E16V, match_vd_neq_vm, INSN_DREF },
{"vsseg7e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG7E16V, MASK_VSSEG7E16V, match_vd_neq_vm, INSN_DREF },
{"vlseg8e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG8E16V, MASK_VLSEG8E16V, match_vd_neq_vm, INSN_DREF },
{"vsseg8e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG8E16V, MASK_VSSEG8E16V, match_vd_neq_vm, INSN_DREF },

{"vlseg2e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG2E32V, MASK_VLSEG2E32V, match_vd_neq_vm, INSN_DREF },
{"vsseg2e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG2E32V, MASK_VSSEG2E32V, match_vd_neq_vm, INSN_DREF },
{"vlseg3e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG3E32V, MASK_VLSEG3E32V, match_vd_neq_vm, INSN_DREF },
{"vsseg3e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG3E32V, MASK_VSSEG3E32V, match_vd_neq_vm, INSN_DREF },
{"vlseg4e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG4E32V, MASK_VLSEG4E32V, match_vd_neq_vm, INSN_DREF },
{"vsseg4e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG4E32V, MASK_VSSEG4E32V, match_vd_neq_vm, INSN_DREF },
{"vlseg5e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG5E32V, MASK_VLSEG5E32V, match_vd_neq_vm, INSN_DREF },
{"vsseg5e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG5E32V, MASK_VSSEG5E32V, match_vd_neq_vm, INSN_DREF },
{"vlseg6e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG6E32V, MASK_VLSEG6E32V, match_vd_neq_vm, INSN_DREF },
{"vsseg6e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG6E32V, MASK_VSSEG6E32V, match_vd_neq_vm, INSN_DREF },
{"vlseg7e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG7E32V, MASK_VLSEG7E32V, match_vd_neq_vm, INSN_DREF },
{"vsseg7e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG7E32V, MASK_VSSEG7E32V, match_vd_neq_vm, INSN_DREF },
{"vlseg8e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG8E32V, MASK_VLSEG8E32V, match_vd_neq_vm, INSN_DREF },
{"vsseg8e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG8E32V, MASK_VSSEG8E32V, match_vd_neq_vm, INSN_DREF },

{"vlseg2e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG2E64V, MASK_VLSEG2E64V, match_vd_neq_vm, INSN_DREF },
{"vsseg2e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG2E64V, MASK_VSSEG2E64V, match_vd_neq_vm, INSN_DREF },
{"vlseg3e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG3E64V, MASK_VLSEG3E64V, match_vd_neq_vm, INSN_DREF },
{"vsseg3e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG3E64V, MASK_VSSEG3E64V, match_vd_neq_vm, INSN_DREF },
{"vlseg4e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG4E64V, MASK_VLSEG4E64V, match_vd_neq_vm, INSN_DREF },
{"vsseg4e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG4E64V, MASK_VSSEG4E64V, match_vd_neq_vm, INSN_DREF },
{"vlseg5e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG5E64V, MASK_VLSEG5E64V, match_vd_neq_vm, INSN_DREF },
{"vsseg5e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG5E64V, MASK_VSSEG5E64V, match_vd_neq_vm, INSN_DREF },
{"vlseg6e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG6E64V, MASK_VLSEG6E64V, match_vd_neq_vm, INSN_DREF },
{"vsseg6e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG6E64V, MASK_VSSEG6E64V, match_vd_neq_vm, INSN_DREF },
{"vlseg7e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG7E64V, MASK_VLSEG7E64V, match_vd_neq_vm, INSN_DREF },
{"vsseg7e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG7E64V, MASK_VSSEG7E64V, match_vd_neq_vm, INSN_DREF },
{"vlseg8e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG8E64V, MASK_VLSEG8E64V, match_vd_neq_vm, INSN_DREF },
{"vsseg8e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VSSEG8E64V, MASK_VSSEG8E64V, match_vd_neq_vm, INSN_DREF },

{"vlsseg2e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG2E8V, MASK_VLSSEG2E8V, match_vd_neq_vm, INSN_DREF },
{"vssseg2e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG2E8V, MASK_VSSSEG2E8V, match_vd_neq_vm, INSN_DREF },
{"vlsseg3e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG3E8V, MASK_VLSSEG3E8V, match_vd_neq_vm, INSN_DREF },
{"vssseg3e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG3E8V, MASK_VSSSEG3E8V, match_vd_neq_vm, INSN_DREF },
{"vlsseg4e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG4E8V, MASK_VLSSEG4E8V, match_vd_neq_vm, INSN_DREF },
{"vssseg4e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG4E8V, MASK_VSSSEG4E8V, match_vd_neq_vm, INSN_DREF },
{"vlsseg5e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG5E8V, MASK_VLSSEG5E8V, match_vd_neq_vm, INSN_DREF },
{"vssseg5e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG5E8V, MASK_VSSSEG5E8V, match_vd_neq_vm, INSN_DREF },
{"vlsseg6e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG6E8V, MASK_VLSSEG6E8V, match_vd_neq_vm, INSN_DREF },
{"vssseg6e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG6E8V, MASK_VSSSEG6E8V, match_vd_neq_vm, INSN_DREF },
{"vlsseg7e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG7E8V, MASK_VLSSEG7E8V, match_vd_neq_vm, INSN_DREF },
{"vssseg7e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG7E8V, MASK_VSSSEG7E8V, match_vd_neq_vm, INSN_DREF },
{"vlsseg8e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG8E8V, MASK_VLSSEG8E8V, match_vd_neq_vm, INSN_DREF },
{"vssseg8e8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG8E8V, MASK_VSSSEG8E8V, match_vd_neq_vm, INSN_DREF },

{"vlsseg2e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG2E16V, MASK_VLSSEG2E16V, match_vd_neq_vm, INSN_DREF },
{"vssseg2e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG2E16V, MASK_VSSSEG2E16V, match_vd_neq_vm, INSN_DREF },
{"vlsseg3e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG3E16V, MASK_VLSSEG3E16V, match_vd_neq_vm, INSN_DREF },
{"vssseg3e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG3E16V, MASK_VSSSEG3E16V, match_vd_neq_vm, INSN_DREF },
{"vlsseg4e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG4E16V, MASK_VLSSEG4E16V, match_vd_neq_vm, INSN_DREF },
{"vssseg4e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG4E16V, MASK_VSSSEG4E16V, match_vd_neq_vm, INSN_DREF },
{"vlsseg5e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG5E16V, MASK_VLSSEG5E16V, match_vd_neq_vm, INSN_DREF },
{"vssseg5e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG5E16V, MASK_VSSSEG5E16V, match_vd_neq_vm, INSN_DREF },
{"vlsseg6e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG6E16V, MASK_VLSSEG6E16V, match_vd_neq_vm, INSN_DREF },
{"vssseg6e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG6E16V, MASK_VSSSEG6E16V, match_vd_neq_vm, INSN_DREF },
{"vlsseg7e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG7E16V, MASK_VLSSEG7E16V, match_vd_neq_vm, INSN_DREF },
{"vssseg7e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG7E16V, MASK_VSSSEG7E16V, match_vd_neq_vm, INSN_DREF },
{"vlsseg8e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG8E16V, MASK_VLSSEG8E16V, match_vd_neq_vm, INSN_DREF },
{"vssseg8e16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG8E16V, MASK_VSSSEG8E16V, match_vd_neq_vm, INSN_DREF },

{"vlsseg2e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG2E32V, MASK_VLSSEG2E32V, match_vd_neq_vm, INSN_DREF },
{"vssseg2e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG2E32V, MASK_VSSSEG2E32V, match_vd_neq_vm, INSN_DREF },
{"vlsseg3e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG3E32V, MASK_VLSSEG3E32V, match_vd_neq_vm, INSN_DREF },
{"vssseg3e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG3E32V, MASK_VSSSEG3E32V, match_vd_neq_vm, INSN_DREF },
{"vlsseg4e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG4E32V, MASK_VLSSEG4E32V, match_vd_neq_vm, INSN_DREF },
{"vssseg4e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG4E32V, MASK_VSSSEG4E32V, match_vd_neq_vm, INSN_DREF },
{"vlsseg5e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG5E32V, MASK_VLSSEG5E32V, match_vd_neq_vm, INSN_DREF },
{"vssseg5e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG5E32V, MASK_VSSSEG5E32V, match_vd_neq_vm, INSN_DREF },
{"vlsseg6e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG6E32V, MASK_VLSSEG6E32V, match_vd_neq_vm, INSN_DREF },
{"vssseg6e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG6E32V, MASK_VSSSEG6E32V, match_vd_neq_vm, INSN_DREF },
{"vlsseg7e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG7E32V, MASK_VLSSEG7E32V, match_vd_neq_vm, INSN_DREF },
{"vssseg7e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG7E32V, MASK_VSSSEG7E32V, match_vd_neq_vm, INSN_DREF },
{"vlsseg8e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG8E32V, MASK_VLSSEG8E32V, match_vd_neq_vm, INSN_DREF },
{"vssseg8e32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG8E32V, MASK_VSSSEG8E32V, match_vd_neq_vm, INSN_DREF },

{"vlsseg2e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG2E64V, MASK_VLSSEG2E64V, match_vd_neq_vm, INSN_DREF },
{"vssseg2e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG2E64V, MASK_VSSSEG2E64V, match_vd_neq_vm, INSN_DREF },
{"vlsseg3e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG3E64V, MASK_VLSSEG3E64V, match_vd_neq_vm, INSN_DREF },
{"vssseg3e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG3E64V, MASK_VSSSEG3E64V, match_vd_neq_vm, INSN_DREF },
{"vlsseg4e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG4E64V, MASK_VLSSEG4E64V, match_vd_neq_vm, INSN_DREF },
{"vssseg4e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG4E64V, MASK_VSSSEG4E64V, match_vd_neq_vm, INSN_DREF },
{"vlsseg5e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG5E64V, MASK_VLSSEG5E64V, match_vd_neq_vm, INSN_DREF },
{"vssseg5e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG5E64V, MASK_VSSSEG5E64V, match_vd_neq_vm, INSN_DREF },
{"vlsseg6e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG6E64V, MASK_VLSSEG6E64V, match_vd_neq_vm, INSN_DREF },
{"vssseg6e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG6E64V, MASK_VSSSEG6E64V, match_vd_neq_vm, INSN_DREF },
{"vlsseg7e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG7E64V, MASK_VLSSEG7E64V, match_vd_neq_vm, INSN_DREF },
{"vssseg7e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG7E64V, MASK_VSSSEG7E64V, match_vd_neq_vm, INSN_DREF },
{"vlsseg8e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VLSSEG8E64V, MASK_VLSSEG8E64V, match_vd_neq_vm, INSN_DREF },
{"vssseg8e64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),tVm", MATCH_VSSSEG8E64V, MASK_VSSSEG8E64V, match_vd_neq_vm, INSN_DREF },

{"vloxseg2ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG2EI8V, MASK_VLOXSEG2EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg2ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG2EI8V, MASK_VSOXSEG2EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg3ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG3EI8V, MASK_VLOXSEG3EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg3ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG3EI8V, MASK_VSOXSEG3EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg4ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG4EI8V, MASK_VLOXSEG4EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg4ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG4EI8V, MASK_VSOXSEG4EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg5ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG5EI8V, MASK_VLOXSEG5EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg5ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG5EI8V, MASK_VSOXSEG5EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg6ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG6EI8V, MASK_VLOXSEG6EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg6ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG6EI8V, MASK_VSOXSEG6EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg7ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG7EI8V, MASK_VLOXSEG7EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg7ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG7EI8V, MASK_VSOXSEG7EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg8ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG8EI8V, MASK_VLOXSEG8EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg8ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG8EI8V, MASK_VSOXSEG8EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },

{"vloxseg2ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG2EI16V, MASK_VLOXSEG2EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg2ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG2EI16V, MASK_VSOXSEG2EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg3ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG3EI16V, MASK_VLOXSEG3EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg3ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG3EI16V, MASK_VSOXSEG3EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg4ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG4EI16V, MASK_VLOXSEG4EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg4ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG4EI16V, MASK_VSOXSEG4EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg5ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG5EI16V, MASK_VLOXSEG5EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg5ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG5EI16V, MASK_VSOXSEG5EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg6ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG6EI16V, MASK_VLOXSEG6EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg6ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG6EI16V, MASK_VSOXSEG6EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg7ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG7EI16V, MASK_VLOXSEG7EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg7ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG7EI16V, MASK_VSOXSEG7EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg8ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG8EI16V, MASK_VLOXSEG8EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg8ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG8EI16V, MASK_VSOXSEG8EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },

{"vloxseg2ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG2EI32V, MASK_VLOXSEG2EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg2ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG2EI32V, MASK_VSOXSEG2EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg3ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG3EI32V, MASK_VLOXSEG3EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg3ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG3EI32V, MASK_VSOXSEG3EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg4ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG4EI32V, MASK_VLOXSEG4EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg4ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG4EI32V, MASK_VSOXSEG4EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg5ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG5EI32V, MASK_VLOXSEG5EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg5ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG5EI32V, MASK_VSOXSEG5EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg6ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG6EI32V, MASK_VLOXSEG6EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg6ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG6EI32V, MASK_VSOXSEG6EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg7ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG7EI32V, MASK_VLOXSEG7EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg7ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG7EI32V, MASK_VSOXSEG7EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg8ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG8EI32V, MASK_VLOXSEG8EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg8ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG8EI32V, MASK_VSOXSEG8EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },

{"vloxseg2ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG2EI64V, MASK_VLOXSEG2EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg2ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG2EI64V, MASK_VSOXSEG2EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg3ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG3EI64V, MASK_VLOXSEG3EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg3ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG3EI64V, MASK_VSOXSEG3EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg4ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG4EI64V, MASK_VLOXSEG4EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg4ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG4EI64V, MASK_VSOXSEG4EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg5ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG5EI64V, MASK_VLOXSEG5EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg5ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG5EI64V, MASK_VSOXSEG5EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg6ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG6EI64V, MASK_VLOXSEG6EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg6ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG6EI64V, MASK_VSOXSEG6EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg7ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG7EI64V, MASK_VLOXSEG7EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg7ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG7EI64V, MASK_VSOXSEG7EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vloxseg8ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLOXSEG8EI64V, MASK_VLOXSEG8EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsoxseg8ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSOXSEG8EI64V, MASK_VSOXSEG8EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },

{"vluxseg2ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG2EI8V, MASK_VLUXSEG2EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg2ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG2EI8V, MASK_VSUXSEG2EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg3ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG3EI8V, MASK_VLUXSEG3EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg3ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG3EI8V, MASK_VSUXSEG3EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg4ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG4EI8V, MASK_VLUXSEG4EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg4ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG4EI8V, MASK_VSUXSEG4EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg5ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG5EI8V, MASK_VLUXSEG5EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg5ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG5EI8V, MASK_VSUXSEG5EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg6ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG6EI8V, MASK_VLUXSEG6EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg6ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG6EI8V, MASK_VSUXSEG6EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg7ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG7EI8V, MASK_VLUXSEG7EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg7ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG7EI8V, MASK_VSUXSEG7EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg8ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG8EI8V, MASK_VLUXSEG8EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg8ei8.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG8EI8V, MASK_VSUXSEG8EI8V, match_vd_neq_vs2_neq_vm, INSN_DREF },

{"vluxseg2ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG2EI16V, MASK_VLUXSEG2EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg2ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG2EI16V, MASK_VSUXSEG2EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg3ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG3EI16V, MASK_VLUXSEG3EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg3ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG3EI16V, MASK_VSUXSEG3EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg4ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG4EI16V, MASK_VLUXSEG4EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg4ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG4EI16V, MASK_VSUXSEG4EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg5ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG5EI16V, MASK_VLUXSEG5EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg5ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG5EI16V, MASK_VSUXSEG5EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg6ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG6EI16V, MASK_VLUXSEG6EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg6ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG6EI16V, MASK_VSUXSEG6EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg7ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG7EI16V, MASK_VLUXSEG7EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg7ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG7EI16V, MASK_VSUXSEG7EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg8ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG8EI16V, MASK_VLUXSEG8EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg8ei16.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG8EI16V, MASK_VSUXSEG8EI16V, match_vd_neq_vs2_neq_vm, INSN_DREF },

{"vluxseg2ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG2EI32V, MASK_VLUXSEG2EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg2ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG2EI32V, MASK_VSUXSEG2EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg3ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG3EI32V, MASK_VLUXSEG3EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg3ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG3EI32V, MASK_VSUXSEG3EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg4ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG4EI32V, MASK_VLUXSEG4EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg4ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG4EI32V, MASK_VSUXSEG4EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg5ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG5EI32V, MASK_VLUXSEG5EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg5ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG5EI32V, MASK_VSUXSEG5EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg6ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG6EI32V, MASK_VLUXSEG6EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg6ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG6EI32V, MASK_VSUXSEG6EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg7ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG7EI32V, MASK_VLUXSEG7EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg7ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG7EI32V, MASK_VSUXSEG7EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg8ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG8EI32V, MASK_VLUXSEG8EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg8ei32.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG8EI32V, MASK_VSUXSEG8EI32V, match_vd_neq_vs2_neq_vm, INSN_DREF },

{"vluxseg2ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG2EI64V, MASK_VLUXSEG2EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg2ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG2EI64V, MASK_VSUXSEG2EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg3ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG3EI64V, MASK_VLUXSEG3EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg3ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG3EI64V, MASK_VSUXSEG3EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg4ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG4EI64V, MASK_VLUXSEG4EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg4ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG4EI64V, MASK_VSUXSEG4EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg5ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG5EI64V, MASK_VLUXSEG5EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg5ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG5EI64V, MASK_VSUXSEG5EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg6ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG6EI64V, MASK_VLUXSEG6EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg6ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG6EI64V, MASK_VSUXSEG6EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg7ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG7EI64V, MASK_VLUXSEG7EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg7ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG7EI64V, MASK_VSUXSEG7EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vluxseg8ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VLUXSEG8EI64V, MASK_VLUXSEG8EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },
{"vsuxseg8ei64.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s),VtVm", MATCH_VSUXSEG8EI64V, MASK_VSUXSEG8EI64V, match_vd_neq_vs2_neq_vm, INSN_DREF },

{"vlseg2e8ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG2E8FFV, MASK_VLSEG2E8FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg3e8ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG3E8FFV, MASK_VLSEG3E8FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg4e8ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG4E8FFV, MASK_VLSEG4E8FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg5e8ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG5E8FFV, MASK_VLSEG5E8FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg6e8ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG6E8FFV, MASK_VLSEG6E8FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg7e8ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG7E8FFV, MASK_VLSEG7E8FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg8e8ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG8E8FFV, MASK_VLSEG8E8FFV, match_vd_neq_vm, INSN_DREF },

{"vlseg2e16ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG2E16FFV, MASK_VLSEG2E16FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg3e16ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG3E16FFV, MASK_VLSEG3E16FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg4e16ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG4E16FFV, MASK_VLSEG4E16FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg5e16ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG5E16FFV, MASK_VLSEG5E16FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg6e16ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG6E16FFV, MASK_VLSEG6E16FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg7e16ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG7E16FFV, MASK_VLSEG7E16FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg8e16ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG8E16FFV, MASK_VLSEG8E16FFV, match_vd_neq_vm, INSN_DREF },

{"vlseg2e32ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG2E32FFV, MASK_VLSEG2E32FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg3e32ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG3E32FFV, MASK_VLSEG3E32FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg4e32ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG4E32FFV, MASK_VLSEG4E32FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg5e32ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG5E32FFV, MASK_VLSEG5E32FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg6e32ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG6E32FFV, MASK_VLSEG6E32FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg7e32ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG7E32FFV, MASK_VLSEG7E32FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg8e32ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG8E32FFV, MASK_VLSEG8E32FFV, match_vd_neq_vm, INSN_DREF },

{"vlseg2e64ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG2E64FFV, MASK_VLSEG2E64FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg3e64ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG3E64FFV, MASK_VLSEG3E64FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg4e64ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG4E64FFV, MASK_VLSEG4E64FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg5e64ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG5E64FFV, MASK_VLSEG5E64FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg6e64ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG6E64FFV, MASK_VLSEG6E64FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg7e64ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG7E64FFV, MASK_VLSEG7E64FFV, match_vd_neq_vm, INSN_DREF },
{"vlseg8e64ff.v",  0, INSN_CLASS_V_OR_ZVLSSEG,  "Vd,0(s)Vm", MATCH_VLSEG8E64FFV, MASK_VLSEG8E64FFV, match_vd_neq_vm, INSN_DREF },

{"vl1r.v",      0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL1RE8V, MASK_VL1RE8V, match_vls_nf_rv, INSN_DREF|INSN_ALIAS },
{"vl1re8.v",    0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL1RE8V, MASK_VL1RE8V, match_vls_nf_rv, INSN_DREF },
{"vl1re16.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL1RE16V, MASK_VL1RE16V, match_vls_nf_rv, INSN_DREF },
{"vl1re32.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL1RE32V, MASK_VL1RE32V, match_vls_nf_rv, INSN_DREF },
{"vl1re64.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL1RE64V, MASK_VL1RE64V, match_vls_nf_rv, INSN_DREF },

{"vl2r.v",      0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL2RE8V, MASK_VL2RE8V, match_vls_nf_rv, INSN_DREF|INSN_ALIAS },
{"vl2re8.v",    0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL2RE8V, MASK_VL2RE8V, match_vls_nf_rv, INSN_DREF },
{"vl2re16.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL2RE16V, MASK_VL2RE16V, match_vls_nf_rv, INSN_DREF },
{"vl2re32.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL2RE32V, MASK_VL2RE32V, match_vls_nf_rv, INSN_DREF },
{"vl2re64.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL2RE64V, MASK_VL2RE64V, match_vls_nf_rv, INSN_DREF },

{"vl4r.v",      0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL4RE8V, MASK_VL4RE8V, match_vls_nf_rv, INSN_DREF|INSN_ALIAS },
{"vl4re8.v",    0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL4RE8V, MASK_VL4RE8V, match_vls_nf_rv, INSN_DREF },
{"vl4re16.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL4RE16V, MASK_VL4RE16V, match_vls_nf_rv, INSN_DREF },
{"vl4re32.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL4RE32V, MASK_VL4RE32V, match_vls_nf_rv, INSN_DREF },
{"vl4re64.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL4RE64V, MASK_VL4RE64V, match_vls_nf_rv, INSN_DREF },

{"vl8r.v",      0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL8RE8V, MASK_VL8RE8V, match_vls_nf_rv, INSN_DREF|INSN_ALIAS },
{"vl8re8.v",    0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL8RE8V, MASK_VL8RE8V, match_vls_nf_rv, INSN_DREF },
{"vl8re16.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL8RE16V, MASK_VL8RE16V, match_vls_nf_rv, INSN_DREF },
{"vl8re32.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL8RE32V, MASK_VL8RE32V, match_vls_nf_rv, INSN_DREF },
{"vl8re64.v",   0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VL8RE64V, MASK_VL8RE64V, match_vls_nf_rv, INSN_DREF },

{"vs1r.v",  0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VS1RV, MASK_VS1RV, match_vls_nf_rv, INSN_DREF },
{"vs2r.v",  0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VS2RV, MASK_VS2RV, match_vls_nf_rv, INSN_DREF },
{"vs4r.v",  0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VS4RV, MASK_VS4RV, match_vls_nf_rv, INSN_DREF },
{"vs8r.v",  0, INSN_CLASS_V,  "Vd,0(s)", MATCH_VS8RV, MASK_VS8RV, match_vls_nf_rv, INSN_DREF },

{"vamoaddei8.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOADDEI8V, MASK_VAMOADDEI8V, match_vd_neq_vm, INSN_DREF},
{"vamoswapei8.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOSWAPEI8V, MASK_VAMOSWAPEI8V, match_vd_neq_vm, INSN_DREF},
{"vamoxorei8.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOXOREI8V, MASK_VAMOXOREI8V, match_vd_neq_vm, INSN_DREF},
{"vamoandei8.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOANDEI8V, MASK_VAMOANDEI8V, match_vd_neq_vm, INSN_DREF},
{"vamoorei8.v",    0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOOREI8V, MASK_VAMOOREI8V, match_vd_neq_vm, INSN_DREF},
{"vamominei8.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMINEI8V, MASK_VAMOMINEI8V, match_vd_neq_vm, INSN_DREF},
{"vamomaxei8.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMAXEI8V, MASK_VAMOMAXEI8V, match_vd_neq_vm, INSN_DREF},
{"vamominuei8.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMINUEI8V, MASK_VAMOMINUEI8V, match_vd_neq_vm, INSN_DREF},
{"vamomaxuei8.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMAXUEI8V, MASK_VAMOMAXUEI8V, match_vd_neq_vm, INSN_DREF},

{"vamoaddei16.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOADDEI16V, MASK_VAMOADDEI16V, match_vd_neq_vm, INSN_DREF},
{"vamoswapei16.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOSWAPEI16V, MASK_VAMOSWAPEI16V, match_vd_neq_vm, INSN_DREF},
{"vamoxorei16.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOXOREI16V, MASK_VAMOXOREI16V, match_vd_neq_vm, INSN_DREF},
{"vamoandei16.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOANDEI16V, MASK_VAMOANDEI16V, match_vd_neq_vm, INSN_DREF},
{"vamoorei16.v",    0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOOREI16V, MASK_VAMOOREI16V, match_vd_neq_vm, INSN_DREF},
{"vamominei16.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMINEI16V, MASK_VAMOMINEI16V, match_vd_neq_vm, INSN_DREF},
{"vamomaxei16.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMAXEI16V, MASK_VAMOMAXEI16V, match_vd_neq_vm, INSN_DREF},
{"vamominuei16.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMINUEI16V, MASK_VAMOMINUEI16V, match_vd_neq_vm, INSN_DREF},
{"vamomaxuei16.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMAXUEI16V, MASK_VAMOMAXUEI16V, match_vd_neq_vm, INSN_DREF},

{"vamoaddei32.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOADDEI32V, MASK_VAMOADDEI32V, match_vd_neq_vm, INSN_DREF},
{"vamoswapei32.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOSWAPEI32V, MASK_VAMOSWAPEI32V, match_vd_neq_vm, INSN_DREF},
{"vamoxorei32.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOXOREI32V, MASK_VAMOXOREI32V, match_vd_neq_vm, INSN_DREF},
{"vamoandei32.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOANDEI32V, MASK_VAMOANDEI32V, match_vd_neq_vm, INSN_DREF},
{"vamoorei32.v",    0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOOREI32V, MASK_VAMOOREI32V, match_vd_neq_vm, INSN_DREF},
{"vamominei32.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMINEI32V, MASK_VAMOMINEI32V, match_vd_neq_vm, INSN_DREF},
{"vamomaxei32.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMAXEI32V, MASK_VAMOMAXEI32V, match_vd_neq_vm, INSN_DREF},
{"vamominuei32.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMINUEI32V, MASK_VAMOMINUEI32V, match_vd_neq_vm, INSN_DREF},
{"vamomaxuei32.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMAXUEI32V, MASK_VAMOMAXUEI32V, match_vd_neq_vm, INSN_DREF},

{"vamoaddei64.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOADDEI64V, MASK_VAMOADDEI64V, match_vd_neq_vm, INSN_DREF},
{"vamoswapei64.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOSWAPEI64V, MASK_VAMOSWAPEI64V, match_vd_neq_vm, INSN_DREF},
{"vamoxorei64.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOXOREI64V, MASK_VAMOXOREI64V, match_vd_neq_vm, INSN_DREF},
{"vamoandei64.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOANDEI64V, MASK_VAMOANDEI64V, match_vd_neq_vm, INSN_DREF},
{"vamoorei64.v",    0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOOREI64V, MASK_VAMOOREI64V, match_vd_neq_vm, INSN_DREF},
{"vamominei64.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMINEI64V, MASK_VAMOMINEI64V, match_vd_neq_vm, INSN_DREF},
{"vamomaxei64.v",   0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMAXEI64V, MASK_VAMOMAXEI64V, match_vd_neq_vm, INSN_DREF},
{"vamominuei64.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMINUEI64V, MASK_VAMOMINUEI64V, match_vd_neq_vm, INSN_DREF},
{"vamomaxuei64.v",  0, INSN_CLASS_V_OR_ZVAMO,  "Ve,0(s),Vt,VfVm", MATCH_VAMOMAXUEI64V, MASK_VAMOMAXUEI64V, match_vd_neq_vm, INSN_DREF},

{"vneg.v",     0, INSN_CLASS_V,  "Vd,VtVm",  MATCH_VRSUBVX, MASK_VRSUBVX | MASK_RS1, match_vd_neq_vm, INSN_ALIAS },

{"vadd.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VADDVV, MASK_VADDVV, match_vd_neq_vm, 0 },
{"vadd.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VADDVX, MASK_VADDVX, match_vd_neq_vm, 0 },
{"vadd.vi",    0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VADDVI, MASK_VADDVI, match_vd_neq_vm, 0 },
{"vsub.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSUBVV, MASK_VSUBVV, match_vd_neq_vm, 0 },
{"vsub.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSUBVX, MASK_VSUBVX, match_vd_neq_vm, 0 },
{"vrsub.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VRSUBVX, MASK_VRSUBVX, match_vd_neq_vm, 0 },
{"vrsub.vi",   0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VRSUBVI, MASK_VRSUBVI, match_vd_neq_vm, 0 },

{"vwcvt.x.x.v",  0, INSN_CLASS_V,  "Vd,VtVm", MATCH_VWCVTXXV, MASK_VWCVTXXV, match_vd_neq_vs2_neq_vm, INSN_ALIAS },
{"vwcvtu.x.x.v", 0, INSN_CLASS_V,  "Vd,VtVm", MATCH_VWCVTUXXV, MASK_VWCVTUXXV, match_vd_neq_vs2_neq_vm, INSN_ALIAS },

{"vwaddu.vv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWADDUVV, MASK_VWADDUVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0 },
{"vwaddu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWADDUVX, MASK_VWADDUVX, match_vd_neq_vs2_neq_vm, 0 },
{"vwsubu.vv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWSUBUVV, MASK_VWSUBUVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0 },
{"vwsubu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWSUBUVX, MASK_VWSUBUVX, match_vd_neq_vs2_neq_vm, 0 },
{"vwadd.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWADDVV, MASK_VWADDVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0 },
{"vwadd.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWADDVX, MASK_VWADDVX, match_vd_neq_vs2_neq_vm, 0 },
{"vwsub.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWSUBVV, MASK_VWSUBVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0 },
{"vwsub.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWSUBVX, MASK_VWSUBVX, match_vd_neq_vs2_neq_vm, 0 },
{"vwaddu.wv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWADDUWV, MASK_VWADDUWV, match_vd_neq_vs1_neq_vm, 0 },
{"vwaddu.wx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWADDUWX, MASK_VWADDUWX, match_vd_neq_vm, 0 },
{"vwsubu.wv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWSUBUWV, MASK_VWSUBUWV, match_vd_neq_vs1_neq_vm, 0 },
{"vwsubu.wx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWSUBUWX, MASK_VWSUBUWX, match_vd_neq_vm, 0 },
{"vwadd.wv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWADDWV, MASK_VWADDWV, match_vd_neq_vs1_neq_vm, 0 },
{"vwadd.wx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWADDWX, MASK_VWADDWX, match_vd_neq_vm, 0 },
{"vwsub.wv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWSUBWV, MASK_VWSUBWV, match_vd_neq_vs1_neq_vm, 0 },
{"vwsub.wx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWSUBWX, MASK_VWSUBWX, match_vd_neq_vm, 0 },

{"vzext.vf2",  0, INSN_CLASS_V,  "Vd,VtVm", MATCH_VZEXT_VF2, MASK_VZEXT_VF2, match_vd_neq_vm, 0 },
{"vsext.vf2",  0, INSN_CLASS_V,  "Vd,VtVm", MATCH_VSEXT_VF2, MASK_VSEXT_VF2, match_vd_neq_vm, 0 },
{"vzext.vf4",  0, INSN_CLASS_V,  "Vd,VtVm", MATCH_VZEXT_VF4, MASK_VZEXT_VF4, match_vd_neq_vm, 0 },
{"vsext.vf4",  0, INSN_CLASS_V,  "Vd,VtVm", MATCH_VSEXT_VF4, MASK_VSEXT_VF4, match_vd_neq_vm, 0 },
{"vzext.vf8",  0, INSN_CLASS_V,  "Vd,VtVm", MATCH_VZEXT_VF8, MASK_VZEXT_VF8, match_vd_neq_vm, 0 },
{"vsext.vf8",  0, INSN_CLASS_V,  "Vd,VtVm", MATCH_VSEXT_VF8, MASK_VSEXT_VF8, match_vd_neq_vm, 0 },

{"vadc.vvm",   0, INSN_CLASS_V,  "Vd,Vt,Vs,V0", MATCH_VADCVVM, MASK_VADCVVM, match_vd_neq_vm, 0 },
{"vadc.vxm",   0, INSN_CLASS_V,  "Vd,Vt,s,V0", MATCH_VADCVXM, MASK_VADCVXM, match_vd_neq_vm, 0 },
{"vadc.vim",   0, INSN_CLASS_V,  "Vd,Vt,Vi,V0", MATCH_VADCVIM, MASK_VADCVIM, match_vd_neq_vm, 0 },
{"vmadc.vvm",  0, INSN_CLASS_V,  "Vd,Vt,Vs,V0", MATCH_VMADCVVM, MASK_VMADCVVM, match_opcode, 0 },
{"vmadc.vxm",  0, INSN_CLASS_V,  "Vd,Vt,s,V0", MATCH_VMADCVXM, MASK_VMADCVXM, match_opcode, 0 },
{"vmadc.vim",  0, INSN_CLASS_V,  "Vd,Vt,Vi,V0", MATCH_VMADCVIM, MASK_VMADCVIM, match_opcode, 0 },
{"vmadc.vv",   0, INSN_CLASS_V,  "Vd,Vt,Vs", MATCH_VMADCVV, MASK_VMADCVV, match_opcode, 0 },
{"vmadc.vx",   0, INSN_CLASS_V,  "Vd,Vt,s", MATCH_VMADCVX, MASK_VMADCVX, match_opcode, 0 },
{"vmadc.vi",   0, INSN_CLASS_V,  "Vd,Vt,Vi", MATCH_VMADCVI, MASK_VMADCVI, match_opcode, 0 },
{"vsbc.vvm",   0, INSN_CLASS_V,  "Vd,Vt,Vs,V0", MATCH_VSBCVVM, MASK_VSBCVVM, match_vd_neq_vm, 0 },
{"vsbc.vxm",   0, INSN_CLASS_V,  "Vd,Vt,s,V0", MATCH_VSBCVXM, MASK_VSBCVXM, match_vd_neq_vm, 0 },
{"vmsbc.vvm",  0, INSN_CLASS_V,  "Vd,Vt,Vs,V0", MATCH_VMSBCVVM, MASK_VMSBCVVM, match_opcode, 0 },
{"vmsbc.vxm",  0, INSN_CLASS_V,  "Vd,Vt,s,V0", MATCH_VMSBCVXM, MASK_VMSBCVXM, match_opcode, 0 },
{"vmsbc.vv",   0, INSN_CLASS_V,  "Vd,Vt,Vs", MATCH_VMSBCVV, MASK_VMSBCVV, match_opcode, 0 },
{"vmsbc.vx",   0, INSN_CLASS_V,  "Vd,Vt,s", MATCH_VMSBCVX, MASK_VMSBCVX, match_opcode, 0 },

{"vnot.v",     0, INSN_CLASS_V,  "Vd,VtVm", MATCH_VNOTV, MASK_VNOTV, match_vd_neq_vm, INSN_ALIAS },

{"vand.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VANDVV, MASK_VANDVV, match_vd_neq_vm, 0 },
{"vand.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VANDVX, MASK_VANDVX, match_vd_neq_vm, 0 },
{"vand.vi",    0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VANDVI, MASK_VANDVI, match_vd_neq_vm, 0 },
{"vor.vv",     0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VORVV, MASK_VORVV, match_vd_neq_vm, 0 },
{"vor.vx",     0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VORVX, MASK_VORVX, match_vd_neq_vm, 0 },
{"vor.vi",     0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VORVI, MASK_VORVI, match_vd_neq_vm, 0 },
{"vxor.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VXORVV, MASK_VXORVV, match_vd_neq_vm, 0 },
{"vxor.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VXORVX, MASK_VXORVX, match_vd_neq_vm, 0 },
{"vxor.vi",    0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VXORVI, MASK_VXORVI, match_vd_neq_vm, 0 },

{"vsll.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSLLVV, MASK_VSLLVV, match_vd_neq_vm, 0 },
{"vsll.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSLLVX, MASK_VSLLVX, match_vd_neq_vm, 0 },
{"vsll.vi",    0, INSN_CLASS_V,  "Vd,Vt,VjVm", MATCH_VSLLVI, MASK_VSLLVI, match_vd_neq_vm, 0 },
{"vsrl.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSRLVV, MASK_VSRLVV, match_vd_neq_vm, 0 },
{"vsrl.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSRLVX, MASK_VSRLVX, match_vd_neq_vm, 0 },
{"vsrl.vi",    0, INSN_CLASS_V,  "Vd,Vt,VjVm", MATCH_VSRLVI, MASK_VSRLVI, match_vd_neq_vm, 0 },
{"vsra.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSRAVV, MASK_VSRAVV, match_vd_neq_vm, 0 },
{"vsra.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSRAVX, MASK_VSRAVX, match_vd_neq_vm, 0 },
{"vsra.vi",    0, INSN_CLASS_V,  "Vd,Vt,VjVm", MATCH_VSRAVI, MASK_VSRAVI, match_vd_neq_vm, 0 },

{"vncvt.x.x.w",0, INSN_CLASS_V,  "Vd,VtVm", MATCH_VNCVTXXW, MASK_VNCVTXXW, match_opcode, INSN_ALIAS },

{"vnsrl.wv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VNSRLWV, MASK_VNSRLWV, match_opcode, 0 },
{"vnsrl.wx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VNSRLWX, MASK_VNSRLWX, match_opcode, 0 },
{"vnsrl.wi",   0, INSN_CLASS_V,  "Vd,Vt,VjVm", MATCH_VNSRLWI, MASK_VNSRLWI, match_opcode, 0 },
{"vnsra.wv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VNSRAWV, MASK_VNSRAWV, match_opcode, 0 },
{"vnsra.wx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VNSRAWX, MASK_VNSRAWX, match_opcode, 0 },
{"vnsra.wi",   0, INSN_CLASS_V,  "Vd,Vt,VjVm", MATCH_VNSRAWI, MASK_VNSRAWI, match_opcode, 0 },

{"vmseq.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMSEQVV, MASK_VMSEQVV, match_opcode, 0 },
{"vmseq.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMSEQVX, MASK_VMSEQVX, match_opcode, 0 },
{"vmseq.vi",   0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VMSEQVI, MASK_VMSEQVI, match_opcode, 0 },
{"vmsne.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMSNEVV, MASK_VMSNEVV, match_opcode, 0 },
{"vmsne.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMSNEVX, MASK_VMSNEVX, match_opcode, 0 },
{"vmsne.vi",   0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VMSNEVI, MASK_VMSNEVI, match_opcode, 0 },
{"vmsltu.vv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMSLTUVV, MASK_VMSLTUVV, match_opcode, 0 },
{"vmsltu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMSLTUVX, MASK_VMSLTUVX, match_opcode, 0 },
{"vmslt.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMSLTVV, MASK_VMSLTVV, match_opcode, 0 },
{"vmslt.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMSLTVX, MASK_VMSLTVX, match_opcode, 0 },
{"vmsleu.vv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMSLEUVV, MASK_VMSLEUVV, match_opcode, 0 },
{"vmsleu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMSLEUVX, MASK_VMSLEUVX, match_opcode, 0 },
{"vmsleu.vi",  0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VMSLEUVI, MASK_VMSLEUVI, match_opcode, 0 },
{"vmsle.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMSLEVV, MASK_VMSLEVV, match_opcode, 0 },
{"vmsle.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMSLEVX, MASK_VMSLEVX, match_opcode, 0 },
{"vmsle.vi",   0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VMSLEVI, MASK_VMSLEVI, match_opcode, 0 },
{"vmsgtu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMSGTUVX, MASK_VMSGTUVX, match_opcode, 0 },
{"vmsgtu.vi",  0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VMSGTUVI, MASK_VMSGTUVI, match_opcode, 0 },
{"vmsgt.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMSGTVX, MASK_VMSGTVX, match_opcode, 0 },
{"vmsgt.vi",   0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VMSGTVI, MASK_VMSGTVI, match_opcode, 0 },

/* These aliases are for assembly but not disassembly.  */
{"vmsgt.vv",   0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VMSLTVV, MASK_VMSLTVV, match_opcode, INSN_ALIAS },
{"vmsgtu.vv",  0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VMSLTUVV, MASK_VMSLTUVV, match_opcode, INSN_ALIAS },
{"vmsge.vv",   0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VMSLEVV, MASK_VMSLEVV, match_opcode, INSN_ALIAS },
{"vmsgeu.vv",  0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VMSLEUVV, MASK_VMSLEUVV, match_opcode, INSN_ALIAS },
{"vmslt.vi",   0, INSN_CLASS_V,  "Vd,Vt,VkVm", MATCH_VMSLEVI, MASK_VMSLEVI, match_opcode, INSN_ALIAS },
{"vmsltu.vi",  0, INSN_CLASS_V,  "Vd,Vu,0Vm", MATCH_VMSNEVV, MASK_VMSNEVV, match_opcode, INSN_ALIAS },
{"vmsltu.vi",  0, INSN_CLASS_V,  "Vd,Vt,VkVm", MATCH_VMSLEUVI, MASK_VMSLEUVI, match_opcode, INSN_ALIAS },
{"vmsge.vi",   0, INSN_CLASS_V,  "Vd,Vt,VkVm", MATCH_VMSGTVI, MASK_VMSGTVI, match_opcode, INSN_ALIAS },
{"vmsgeu.vi",  0, INSN_CLASS_V,  "Vd,Vu,0Vm", MATCH_VMSEQVV, MASK_VMSEQVV, match_opcode, INSN_ALIAS },
{"vmsgeu.vi",  0, INSN_CLASS_V,  "Vd,Vt,VkVm", MATCH_VMSGTUVI, MASK_VMSGTUVI, match_opcode, INSN_ALIAS },

{"vmsge.vx",   0, INSN_CLASS_V, "Vd,Vt,sVm", 0, (int) M_VMSGE, match_never, INSN_MACRO },
{"vmsge.vx",   0, INSN_CLASS_V, "Vd,Vt,s,VM,VT", 0, (int) M_VMSGE, match_never, INSN_MACRO },
{"vmsgeu.vx",  0, INSN_CLASS_V, "Vd,Vt,sVm", 0, (int) M_VMSGEU, match_never, INSN_MACRO },
{"vmsgeu.vx",  0, INSN_CLASS_V, "Vd,Vt,s,VM,VT", 0, (int) M_VMSGEU, match_never, INSN_MACRO },

{"vminu.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMINUVV, MASK_VMINUVV, match_vd_neq_vm, 0},
{"vminu.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMINUVX, MASK_VMINUVX, match_vd_neq_vm, 0},
{"vmin.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMINVV, MASK_VMINVV, match_vd_neq_vm, 0},
{"vmin.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMINVX, MASK_VMINVX, match_vd_neq_vm, 0},
{"vmaxu.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMAXUVV, MASK_VMAXUVV, match_vd_neq_vm, 0},
{"vmaxu.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMAXUVX, MASK_VMAXUVX, match_vd_neq_vm, 0},
{"vmax.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMAXVV, MASK_VMAXVV, match_vd_neq_vm, 0},
{"vmax.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMAXVX, MASK_VMAXVX, match_vd_neq_vm, 0},

{"vmul.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMULVV, MASK_VMULVV, match_vd_neq_vm, 0 },
{"vmul.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMULVX, MASK_VMULVX, match_vd_neq_vm, 0 },
{"vmulh.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMULHVV, MASK_VMULHVV, match_vd_neq_vm, 0 },
{"vmulh.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMULHVX, MASK_VMULHVX, match_vd_neq_vm, 0 },
{"vmulhu.vv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMULHUVV, MASK_VMULHUVV, match_vd_neq_vm, 0 },
{"vmulhu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMULHUVX, MASK_VMULHUVX, match_vd_neq_vm, 0 },
{"vmulhsu.vv", 0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VMULHSUVV, MASK_VMULHSUVV, match_vd_neq_vm, 0 },
{"vmulhsu.vx", 0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VMULHSUVX, MASK_VMULHSUVX, match_vd_neq_vm, 0 },

{"vwmul.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWMULVV, MASK_VWMULVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0 },
{"vwmul.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWMULVX, MASK_VWMULVX, match_vd_neq_vs2_neq_vm, 0 },
{"vwmulu.vv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWMULUVV, MASK_VWMULUVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0 },
{"vwmulu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWMULUVX, MASK_VWMULUVX, match_vd_neq_vs2_neq_vm, 0 },
{"vwmulsu.vv", 0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VWMULSUVV, MASK_VWMULSUVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0 },
{"vwmulsu.vx", 0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VWMULSUVX, MASK_VWMULSUVX, match_vd_neq_vs2_neq_vm, 0 },

{"vmacc.vv",   0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VMACCVV, MASK_VMACCVV, match_vd_neq_vm, 0},
{"vmacc.vx",   0, INSN_CLASS_V,  "Vd,s,VtVm", MATCH_VMACCVX, MASK_VMACCVX, match_vd_neq_vm, 0},
{"vnmsac.vv",  0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VNMSACVV, MASK_VNMSACVV, match_vd_neq_vm, 0},
{"vnmsac.vx",  0, INSN_CLASS_V,  "Vd,s,VtVm", MATCH_VNMSACVX, MASK_VNMSACVX, match_vd_neq_vm, 0},
{"vmadd.vv",   0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VMADDVV, MASK_VMADDVV, match_vd_neq_vm, 0},
{"vmadd.vx",   0, INSN_CLASS_V,  "Vd,s,VtVm", MATCH_VMADDVX, MASK_VMADDVX, match_vd_neq_vm, 0},
{"vnmsub.vv",  0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VNMSUBVV, MASK_VNMSUBVV, match_vd_neq_vm, 0},
{"vnmsub.vx",  0, INSN_CLASS_V,  "Vd,s,VtVm", MATCH_VNMSUBVX, MASK_VNMSUBVX, match_vd_neq_vm, 0},

{"vwmaccu.vv",  0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VWMACCUVV, MASK_VWMACCUVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vwmaccu.vx",  0, INSN_CLASS_V,  "Vd,s,VtVm", MATCH_VWMACCUVX, MASK_VWMACCUVX, match_vd_neq_vs2_neq_vm, 0},
{"vwmacc.vv",   0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VWMACCVV, MASK_VWMACCVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vwmacc.vx",   0, INSN_CLASS_V,  "Vd,s,VtVm", MATCH_VWMACCVX, MASK_VWMACCVX, match_vd_neq_vs2_neq_vm, 0},
{"vwmaccsu.vv", 0, INSN_CLASS_V,  "Vd,Vs,VtVm", MATCH_VWMACCSUVV, MASK_VWMACCSUVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vwmaccsu.vx", 0, INSN_CLASS_V,  "Vd,s,VtVm", MATCH_VWMACCSUVX, MASK_VWMACCSUVX, match_vd_neq_vs2_neq_vm, 0},
{"vwmaccus.vx", 0, INSN_CLASS_V,  "Vd,s,VtVm", MATCH_VWMACCUSVX, MASK_VWMACCUSVX, match_vd_neq_vs2_neq_vm, 0},

{"vdivu.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VDIVUVV, MASK_VDIVUVV, match_vd_neq_vm, 0 },
{"vdivu.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VDIVUVX, MASK_VDIVUVX, match_vd_neq_vm, 0 },
{"vdiv.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VDIVVV, MASK_VDIVVV, match_vd_neq_vm, 0 },
{"vdiv.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VDIVVX, MASK_VDIVVX, match_vd_neq_vm, 0 },
{"vremu.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VREMUVV, MASK_VREMUVV, match_vd_neq_vm, 0 },
{"vremu.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VREMUVX, MASK_VREMUVX, match_vd_neq_vm, 0 },
{"vrem.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VREMVV, MASK_VREMVV, match_vd_neq_vm, 0 },
{"vrem.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VREMVX, MASK_VREMVX, match_vd_neq_vm, 0 },

{"vmerge.vvm", 0, INSN_CLASS_V,  "Vd,Vt,Vs,V0", MATCH_VMERGEVVM, MASK_VMERGEVVM, match_opcode, 0 },
{"vmerge.vxm", 0, INSN_CLASS_V,  "Vd,Vt,s,V0", MATCH_VMERGEVXM, MASK_VMERGEVXM, match_opcode, 0 },
{"vmerge.vim", 0, INSN_CLASS_V,  "Vd,Vt,Vi,V0", MATCH_VMERGEVIM, MASK_VMERGEVIM, match_opcode, 0 },

{"vmv.v.v",    0, INSN_CLASS_V,  "Vd,Vs", MATCH_VMVVV, MASK_VMVVV, match_opcode, 0 },
{"vmv.v.x",    0, INSN_CLASS_V,  "Vd,s", MATCH_VMVVX, MASK_VMVVX, match_opcode, 0 },
{"vmv.v.i",    0, INSN_CLASS_V,  "Vd,Vi", MATCH_VMVVI, MASK_VMVVI, match_opcode, 0 },

{"vsaddu.vv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSADDUVV, MASK_VSADDUVV, match_vd_neq_vm, 0 },
{"vsaddu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSADDUVX, MASK_VSADDUVX, match_vd_neq_vm, 0 },
{"vsaddu.vi",  0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VSADDUVI, MASK_VSADDUVI, match_vd_neq_vm, 0 },
{"vsadd.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSADDVV, MASK_VSADDVV, match_vd_neq_vm, 0 },
{"vsadd.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSADDVX, MASK_VSADDVX, match_vd_neq_vm, 0 },
{"vsadd.vi",   0, INSN_CLASS_V,  "Vd,Vt,ViVm", MATCH_VSADDVI, MASK_VSADDVI, match_vd_neq_vm, 0 },
{"vssubu.vv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSSUBUVV, MASK_VSSUBUVV, match_vd_neq_vm, 0 },
{"vssubu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSSUBUVX, MASK_VSSUBUVX, match_vd_neq_vm, 0 },
{"vssub.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSSUBVV, MASK_VSSUBVV, match_vd_neq_vm, 0 },
{"vssub.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSSUBVX, MASK_VSSUBVX, match_vd_neq_vm, 0 },

{"vaaddu.vv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VAADDUVV, MASK_VAADDUVV, match_vd_neq_vm, 0 },
{"vaaddu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VAADDUVX, MASK_VAADDUVX, match_vd_neq_vm, 0 },
{"vaadd.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VAADDVV, MASK_VAADDVV, match_vd_neq_vm, 0 },
{"vaadd.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VAADDVX, MASK_VAADDVX, match_vd_neq_vm, 0 },
{"vasubu.vv",  0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VASUBUVV, MASK_VASUBUVV, match_vd_neq_vm, 0 },
{"vasubu.vx",  0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VASUBUVX, MASK_VASUBUVX, match_vd_neq_vm, 0 },
{"vasub.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VASUBVV, MASK_VASUBVV, match_vd_neq_vm, 0 },
{"vasub.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VASUBVX, MASK_VASUBVX, match_vd_neq_vm, 0 },

{"vsmul.vv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSMULVV, MASK_VSMULVV, match_vd_neq_vm, 0 },
{"vsmul.vx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSMULVX, MASK_VSMULVX, match_vd_neq_vm, 0 },

{"vssrl.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSSRLVV, MASK_VSSRLVV, match_vd_neq_vm, 0 },
{"vssrl.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSSRLVX, MASK_VSSRLVX, match_vd_neq_vm, 0 },
{"vssrl.vi",    0, INSN_CLASS_V,  "Vd,Vt,VjVm", MATCH_VSSRLVI, MASK_VSSRLVI, match_vd_neq_vm, 0 },
{"vssra.vv",    0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VSSRAVV, MASK_VSSRAVV, match_vd_neq_vm, 0 },
{"vssra.vx",    0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VSSRAVX, MASK_VSSRAVX, match_vd_neq_vm, 0 },
{"vssra.vi",    0, INSN_CLASS_V,  "Vd,Vt,VjVm", MATCH_VSSRAVI, MASK_VSSRAVI, match_vd_neq_vm, 0 },

{"vnclipu.wv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VNCLIPUWV, MASK_VNCLIPUWV, match_opcode, 0 },
{"vnclipu.wx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VNCLIPUWX, MASK_VNCLIPUWX, match_opcode, 0 },
{"vnclipu.wi",   0, INSN_CLASS_V,  "Vd,Vt,VjVm", MATCH_VNCLIPUWI, MASK_VNCLIPUWI, match_opcode, 0 },
{"vnclip.wv",   0, INSN_CLASS_V,  "Vd,Vt,VsVm", MATCH_VNCLIPWV, MASK_VNCLIPWV, match_opcode, 0 },
{"vnclip.wx",   0, INSN_CLASS_V,  "Vd,Vt,sVm", MATCH_VNCLIPWX, MASK_VNCLIPWX, match_opcode, 0 },
{"vnclip.wi",   0, INSN_CLASS_V,  "Vd,Vt,VjVm", MATCH_VNCLIPWI, MASK_VNCLIPWI, match_opcode, 0 },

{"vfadd.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFADDVV, MASK_VFADDVV, match_vd_neq_vm, 0},
{"vfadd.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFADDVF, MASK_VFADDVF, match_vd_neq_vm, 0},
{"vfsub.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFSUBVV, MASK_VFSUBVV, match_vd_neq_vm, 0},
{"vfsub.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFSUBVF, MASK_VFSUBVF, match_vd_neq_vm, 0},
{"vfrsub.vf",  0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFRSUBVF, MASK_VFRSUBVF, match_vd_neq_vm, 0},

{"vfwadd.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFWADDVV, MASK_VFWADDVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vfwadd.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFWADDVF, MASK_VFWADDVF, match_vd_neq_vs2_neq_vm, 0},
{"vfwsub.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFWSUBVV, MASK_VFWSUBVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vfwsub.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFWSUBVF, MASK_VFWSUBVF, match_vd_neq_vs2_neq_vm, 0},
{"vfwadd.wv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFWADDWV, MASK_VFWADDWV, match_vd_neq_vs1_neq_vm, 0},
{"vfwadd.wf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFWADDWF, MASK_VFWADDWF, match_vd_neq_vm, 0},
{"vfwsub.wv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFWSUBWV, MASK_VFWSUBWV, match_vd_neq_vs1_neq_vm, 0},
{"vfwsub.wf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFWSUBWF, MASK_VFWSUBWF, match_vd_neq_vm, 0},

{"vfmul.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFMULVV, MASK_VFMULVV, match_vd_neq_vm, 0},
{"vfmul.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFMULVF, MASK_VFMULVF, match_vd_neq_vm, 0},
{"vfdiv.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFDIVVV, MASK_VFDIVVV, match_vd_neq_vm, 0},
{"vfdiv.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFDIVVF, MASK_VFDIVVF, match_vd_neq_vm, 0},
{"vfrdiv.vf",  0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFRDIVVF, MASK_VFRDIVVF, match_vd_neq_vm, 0},

{"vfwmul.vv",  0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFWMULVV, MASK_VFWMULVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vfwmul.vf",  0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFWMULVF, MASK_VFWMULVF, match_vd_neq_vs2_neq_vm, 0},

{"vfmadd.vv",  0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFMADDVV, MASK_VFMADDVV, match_vd_neq_vm, 0},
{"vfmadd.vf",  0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFMADDVF, MASK_VFMADDVF, match_vd_neq_vm, 0},
{"vfnmadd.vv", 0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFNMADDVV, MASK_VFNMADDVV, match_vd_neq_vm, 0},
{"vfnmadd.vf", 0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFNMADDVF, MASK_VFNMADDVF, match_vd_neq_vm, 0},
{"vfmsub.vv",  0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFMSUBVV, MASK_VFMSUBVV, match_vd_neq_vm, 0},
{"vfmsub.vf",  0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFMSUBVF, MASK_VFMSUBVF, match_vd_neq_vm, 0},
{"vfnmsub.vv", 0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFNMSUBVV, MASK_VFNMSUBVV, match_vd_neq_vm, 0},
{"vfnmsub.vf", 0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFNMSUBVF, MASK_VFNMSUBVF, match_vd_neq_vm, 0},
{"vfmacc.vv",  0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFMACCVV, MASK_VFMACCVV, match_vd_neq_vm, 0},
{"vfmacc.vf",  0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFMACCVF, MASK_VFMACCVF, match_vd_neq_vm, 0},
{"vfnmacc.vv", 0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFNMACCVV, MASK_VFNMACCVV, match_vd_neq_vm, 0},
{"vfnmacc.vf", 0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFNMACCVF, MASK_VFNMACCVF, match_vd_neq_vm, 0},
{"vfmsac.vv",  0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFMSACVV, MASK_VFMSACVV, match_vd_neq_vm, 0},
{"vfmsac.vf",  0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFMSACVF, MASK_VFMSACVF, match_vd_neq_vm, 0},
{"vfnmsac.vv", 0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFNMSACVV, MASK_VFNMSACVV, match_vd_neq_vm, 0},
{"vfnmsac.vf", 0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFNMSACVF, MASK_VFNMSACVF, match_vd_neq_vm, 0},

{"vfwmacc.vv",  0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFWMACCVV, MASK_VFWMACCVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vfwmacc.vf",  0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFWMACCVF, MASK_VFWMACCVF, match_vd_neq_vs2_neq_vm, 0},
{"vfwnmacc.vv", 0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFWNMACCVV, MASK_VFWNMACCVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vfwnmacc.vf", 0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFWNMACCVF, MASK_VFWNMACCVF, match_vd_neq_vs2_neq_vm, 0},
{"vfwmsac.vv",  0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFWMSACVV, MASK_VFWMSACVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vfwmsac.vf",  0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFWMSACVF, MASK_VFWMSACVF, match_vd_neq_vs2_neq_vm, 0},
{"vfwnmsac.vv", 0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VFWNMSACVV, MASK_VFWNMSACVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vfwnmsac.vf", 0, INSN_CLASS_V_AND_F, "Vd,S,VtVm", MATCH_VFWNMSACVF, MASK_VFWNMSACVF, match_vd_neq_vs2_neq_vm, 0},

{"vfsqrt.v",   0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFSQRTV, MASK_VFSQRTV, match_vd_neq_vm, 0},
{"vfrsqrt7.v", 0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFRSQRT7V, MASK_VFRSQRT7V, match_vd_neq_vm, 0},
{"vfrsqrte7.v",0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFRSQRT7V, MASK_VFRSQRT7V, match_vd_neq_vm, 0},
{"vfrec7.v",   0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFREC7V, MASK_VFREC7V, match_vd_neq_vm, 0},
{"vfrece7.v",  0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFREC7V, MASK_VFREC7V, match_vd_neq_vm, 0},
{"vfclass.v",  0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFCLASSV, MASK_VFCLASSV, match_vd_neq_vm, 0},

{"vfmin.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFMINVV, MASK_VFMINVV, match_vd_neq_vm, 0},
{"vfmin.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFMINVF, MASK_VFMINVF, match_vd_neq_vm, 0},
{"vfmax.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFMAXVV, MASK_VFMAXVV, match_vd_neq_vm, 0},
{"vfmax.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFMAXVF, MASK_VFMAXVF, match_vd_neq_vm, 0},

{"vfneg.v",    0, INSN_CLASS_V_AND_F, "Vd,VuVm", MATCH_VFSGNJNVV, MASK_VFSGNJNVV, match_vs1_eq_vs2_neq_vm, INSN_ALIAS },
{"vfabs.v",    0, INSN_CLASS_V_AND_F, "Vd,VuVm", MATCH_VFSGNJXVV, MASK_VFSGNJXVV, match_vs1_eq_vs2_neq_vm, INSN_ALIAS },

{"vfsgnj.vv",  0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFSGNJVV, MASK_VFSGNJVV, match_vd_neq_vm, 0},
{"vfsgnj.vf",  0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFSGNJVF, MASK_VFSGNJVF, match_vd_neq_vm, 0},
{"vfsgnjn.vv", 0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFSGNJNVV, MASK_VFSGNJNVV, match_vd_neq_vm, 0},
{"vfsgnjn.vf", 0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFSGNJNVF, MASK_VFSGNJNVF, match_vd_neq_vm, 0},
{"vfsgnjx.vv", 0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFSGNJXVV, MASK_VFSGNJXVV, match_vd_neq_vm, 0},
{"vfsgnjx.vf", 0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFSGNJXVF, MASK_VFSGNJXVF, match_vd_neq_vm, 0},

{"vmfeq.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VMFEQVV, MASK_VMFEQVV, match_opcode, 0},
{"vmfeq.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VMFEQVF, MASK_VMFEQVF, match_opcode, 0},
{"vmfne.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VMFNEVV, MASK_VMFNEVV, match_opcode, 0},
{"vmfne.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VMFNEVF, MASK_VMFNEVF, match_opcode, 0},
{"vmflt.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VMFLTVV, MASK_VMFLTVV, match_opcode, 0},
{"vmflt.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VMFLTVF, MASK_VMFLTVF, match_opcode, 0},
{"vmfle.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VMFLEVV, MASK_VMFLEVV, match_opcode, 0},
{"vmfle.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VMFLEVF, MASK_VMFLEVF, match_opcode, 0},
{"vmfgt.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VMFGTVF, MASK_VMFGTVF, match_opcode, 0},
{"vmfge.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VMFGEVF, MASK_VMFGEVF, match_opcode, 0},

/* These aliases are for assembly but not disassembly.  */
{"vmfgt.vv",    0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VMFLTVV, MASK_VMFLTVV, match_opcode, INSN_ALIAS},
{"vmfge.vv",   0, INSN_CLASS_V_AND_F, "Vd,Vs,VtVm", MATCH_VMFLEVV, MASK_VMFLEVV, match_opcode, INSN_ALIAS},

{"vfmerge.vfm",0, INSN_CLASS_V_AND_F, "Vd,Vt,S,V0", MATCH_VFMERGEVFM, MASK_VFMERGEVFM, match_opcode, 0},
{"vfmv.v.f",   0, INSN_CLASS_V_AND_F, "Vd,S", MATCH_VFMVVF, MASK_VFMVVF, match_opcode, 0 },

{"vfcvt.xu.f.v",     0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFCVTXUFV, MASK_VFCVTXUFV, match_vd_neq_vm, 0},
{"vfcvt.x.f.v",      0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFCVTXFV, MASK_VFCVTXFV, match_vd_neq_vm, 0},
{"vfcvt.rtz.xu.f.v", 0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFCVTRTZXUFV, MASK_VFCVTRTZXUFV, match_vd_neq_vm, 0},
{"vfcvt.rtz.x.f.v",  0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFCVTRTZXFV, MASK_VFCVTRTZXFV, match_vd_neq_vm, 0},
{"vfcvt.f.xu.v",     0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFCVTFXUV, MASK_VFCVTFXUV, match_vd_neq_vm, 0},
{"vfcvt.f.x.v",      0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFCVTFXV, MASK_VFCVTFXV, match_vd_neq_vm, 0},

{"vfwcvt.xu.f.v",     0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFWCVTXUFV, MASK_VFWCVTXUFV, match_vd_neq_vs2_neq_vm, 0},
{"vfwcvt.x.f.v",      0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFWCVTXFV, MASK_VFWCVTXFV, match_vd_neq_vs2_neq_vm, 0},
{"vfwcvt.rtz.xu.f.v", 0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFWCVTRTZXUFV, MASK_VFWCVTRTZXUFV, match_vd_neq_vs2_neq_vm, 0},
{"vfwcvt.rtz.x.f.v",  0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFWCVTRTZXFV, MASK_VFWCVTRTZXFV, match_vd_neq_vs2_neq_vm, 0},
{"vfwcvt.f.xu.v",     0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFWCVTFXUV, MASK_VFWCVTFXUV, match_vd_neq_vs2_neq_vm, 0},
{"vfwcvt.f.x.v",      0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFWCVTFXV, MASK_VFWCVTFXV, match_vd_neq_vs2_neq_vm, 0},
{"vfwcvt.f.f.v",      0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFWCVTFFV, MASK_VFWCVTFFV, match_vd_neq_vs2_neq_vm, 0},

{"vfncvt.xu.f.w",     0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFNCVTXUFW, MASK_VFNCVTXUFW, match_opcode, 0},
{"vfncvt.x.f.w",      0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFNCVTXFW, MASK_VFNCVTXFW, match_opcode, 0},
{"vfncvt.rtz.xu.f.w", 0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFNCVTRTZXUFW, MASK_VFNCVTRTZXUFW, match_opcode, 0},
{"vfncvt.rtz.x.f.w",  0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFNCVTRTZXFW, MASK_VFNCVTRTZXFW, match_opcode, 0},
{"vfncvt.f.xu.w",     0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFNCVTFXUW, MASK_VFNCVTFXUW, match_opcode, 0},
{"vfncvt.f.x.w",      0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFNCVTFXW, MASK_VFNCVTFXW, match_opcode, 0},
{"vfncvt.f.f.w",      0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFNCVTFFW, MASK_VFNCVTFFW, match_opcode, 0},
{"vfncvt.rod.f.f.w",  0, INSN_CLASS_V_AND_F, "Vd,VtVm", MATCH_VFNCVTRODFFW, MASK_VFNCVTRODFFW, match_opcode, 0},

{"vredsum.vs", 0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VREDSUMVS, MASK_VREDSUMVS, match_opcode, 0},
{"vredmaxu.vs",0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VREDMAXUVS, MASK_VREDMAXUVS, match_opcode, 0},
{"vredmax.vs", 0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VREDMAXVS, MASK_VREDMAXVS, match_opcode, 0},
{"vredminu.vs",0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VREDMINUVS, MASK_VREDMINUVS, match_opcode, 0},
{"vredmin.vs", 0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VREDMINVS, MASK_VREDMINVS, match_opcode, 0},
{"vredand.vs", 0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VREDANDVS, MASK_VREDANDVS, match_opcode, 0},
{"vredor.vs",  0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VREDORVS, MASK_VREDORVS, match_opcode, 0},
{"vredxor.vs", 0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VREDXORVS, MASK_VREDXORVS, match_opcode, 0},

{"vwredsumu.vs",0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VWREDSUMUVS, MASK_VWREDSUMUVS, match_opcode, 0},
{"vwredsum.vs",0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VWREDSUMVS, MASK_VWREDSUMVS, match_opcode, 0},

{"vfredosum.vs",0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFREDOSUMVS, MASK_VFREDOSUMVS, match_opcode, 0},
{"vfredusum.vs",0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFREDUSUMVS, MASK_VFREDUSUMVS, match_opcode, 0},
{"vfredsum.vs", 0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFREDUSUMVS, MASK_VFREDUSUMVS, match_opcode, INSN_ALIAS},
{"vfredmax.vs", 0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFREDMAXVS, MASK_VFREDMAXVS, match_opcode, 0},
{"vfredmin.vs", 0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFREDMINVS, MASK_VFREDMINVS, match_opcode, 0},

{"vfwredosum.vs",0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFWREDOSUMVS, MASK_VFWREDOSUMVS, match_opcode, 0},
{"vfwredusum.vs",0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFWREDUSUMVS, MASK_VFWREDUSUMVS, match_opcode, 0},
{"vfwredsum.vs", 0, INSN_CLASS_V_AND_F, "Vd,Vt,VsVm", MATCH_VFWREDUSUMVS, MASK_VFWREDUSUMVS, match_opcode, INSN_ALIAS},

{"vmmv.m",     0, INSN_CLASS_V, "Vd,Vu", MATCH_VMANDMM, MASK_VMANDMM, match_vs1_eq_vs2, INSN_ALIAS},
{"vmcpy.m",    0, INSN_CLASS_V, "Vd,Vu", MATCH_VMANDMM, MASK_VMANDMM, match_vs1_eq_vs2, INSN_ALIAS},
{"vmclr.m",    0, INSN_CLASS_V, "Vv", MATCH_VMXORMM, MASK_VMXORMM, match_vd_eq_vs1_eq_vs2, INSN_ALIAS},
{"vmset.m",    0, INSN_CLASS_V, "Vv", MATCH_VMXNORMM, MASK_VMXNORMM, match_vd_eq_vs1_eq_vs2, INSN_ALIAS},
{"vmnot.m",    0, INSN_CLASS_V, "Vd,Vu", MATCH_VMNANDMM, MASK_VMNANDMM, match_vs1_eq_vs2, INSN_ALIAS},

{"vmand.mm",   0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VMANDMM, MASK_VMANDMM, match_opcode, 0},
{"vmnand.mm",  0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VMNANDMM, MASK_VMNANDMM, match_opcode, 0},

{"vmandn.mm",  0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VMANDNMM, MASK_VMANDNMM, match_opcode, 0},
{"vmandnot.mm",0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VMANDNMM, MASK_VMANDNMM, match_opcode, INSN_ALIAS},
{"vmxor.mm",   0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VMXORMM, MASK_VMXORMM, match_opcode, 0},
{"vmor.mm",    0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VMORMM, MASK_VMORMM, match_opcode, 0},
{"vmnor.mm",   0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VMNORMM, MASK_VMNORMM, match_opcode, 0},
{"vmorn.mm",   0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VMORNMM, MASK_VMORNMM, match_opcode, 0},
{"vmornot.mm", 0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VMORNMM, MASK_VMORNMM, match_opcode, INSN_ALIAS},
{"vmxnor.mm",  0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VMXNORMM, MASK_VMXNORMM, match_opcode, 0},

{"vcpop.m",    0, INSN_CLASS_V, "d,VtVm", MATCH_VCPOPM, MASK_VCPOPM, match_opcode, 0},
{"vpopc.m",    0, INSN_CLASS_V, "d,VtVm", MATCH_VCPOPM, MASK_VCPOPM, match_opcode, INSN_ALIAS},
{"vfirst.m",   0, INSN_CLASS_V, "d,VtVm", MATCH_VFIRSTM, MASK_VFIRSTM, match_opcode, 0},
{"vmsbf.m",    0, INSN_CLASS_V, "Vd,VtVm", MATCH_VMSBFM, MASK_VMSBFM, match_vd_neq_vs2_neq_vm, 0},
{"vmsif.m",    0, INSN_CLASS_V, "Vd,VtVm", MATCH_VMSIFM, MASK_VMSIFM, match_vd_neq_vs2_neq_vm, 0},
{"vmsof.m",    0, INSN_CLASS_V, "Vd,VtVm", MATCH_VMSOFM, MASK_VMSOFM, match_vd_neq_vs2_neq_vm, 0},
{"viota.m",    0, INSN_CLASS_V, "Vd,VtVm", MATCH_VIOTAM, MASK_VIOTAM, match_vd_neq_vs2_neq_vm, 0},
{"vid.v",      0, INSN_CLASS_V, "VdVm", MATCH_VIDV, MASK_VIDV, match_vd_neq_vm, 0},

{"vmv.x.s",    0, INSN_CLASS_V, "d,Vt", MATCH_VMVXS, MASK_VMVXS, match_opcode, 0},
{"vmv.s.x",    0, INSN_CLASS_V, "Vd,s", MATCH_VMVSX, MASK_VMVSX, match_opcode, 0},

{"vfmv.f.s",   0, INSN_CLASS_V_AND_F, "D,Vt", MATCH_VFMVFS, MASK_VFMVFS, match_opcode, 0},
{"vfmv.s.f",   0, INSN_CLASS_V_AND_F, "Vd,S", MATCH_VFMVSF, MASK_VFMVSF, match_opcode, 0},

{"vslideup.vx",0, INSN_CLASS_V, "Vd,Vt,sVm", MATCH_VSLIDEUPVX, MASK_VSLIDEUPVX, match_vd_neq_vs2_neq_vm, 0},
{"vslideup.vi",0, INSN_CLASS_V, "Vd,Vt,VjVm", MATCH_VSLIDEUPVI, MASK_VSLIDEUPVI, match_vd_neq_vs2_neq_vm, 0},
{"vslidedown.vx",0,INSN_CLASS_V, "Vd,Vt,sVm", MATCH_VSLIDEDOWNVX, MASK_VSLIDEDOWNVX, match_vd_neq_vm, 0},
{"vslidedown.vi",0,INSN_CLASS_V, "Vd,Vt,VjVm", MATCH_VSLIDEDOWNVI, MASK_VSLIDEDOWNVI, match_vd_neq_vm, 0},

{"vslide1up.vx",    0, INSN_CLASS_V, "Vd,Vt,sVm", MATCH_VSLIDE1UPVX, MASK_VSLIDE1UPVX, match_vd_neq_vs2_neq_vm, 0},
{"vslide1down.vx",  0, INSN_CLASS_V, "Vd,Vt,sVm", MATCH_VSLIDE1DOWNVX, MASK_VSLIDE1DOWNVX, match_vd_neq_vm, 0},
{"vfslide1up.vf",   0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFSLIDE1UPVF, MASK_VFSLIDE1UPVF, match_vd_neq_vs2_neq_vm, 0},
{"vfslide1down.vf", 0, INSN_CLASS_V_AND_F, "Vd,Vt,SVm", MATCH_VFSLIDE1DOWNVF, MASK_VFSLIDE1DOWNVF, match_vd_neq_vm, 0},

{"vrgather.vv",    0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VRGATHERVV, MASK_VRGATHERVV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},
{"vrgather.vx",    0, INSN_CLASS_V, "Vd,Vt,sVm", MATCH_VRGATHERVX, MASK_VRGATHERVX, match_vd_neq_vs2_neq_vm, 0},
{"vrgather.vi",    0, INSN_CLASS_V, "Vd,Vt,VjVm", MATCH_VRGATHERVI, MASK_VRGATHERVI, match_vd_neq_vs2_neq_vm, 0},
{"vrgatherei16.vv",0, INSN_CLASS_V, "Vd,Vt,VsVm", MATCH_VRGATHEREI16VV, MASK_VRGATHEREI16VV, match_vd_neq_vs1_neq_vs2_neq_vm, 0},

{"vcompress.vm",0, INSN_CLASS_V, "Vd,Vt,Vs", MATCH_VCOMPRESSVM, MASK_VCOMPRESSVM, match_vd_neq_vs1_neq_vs2, 0},

{"vmv1r.v",    0, INSN_CLASS_V, "Vd,Vt", MATCH_VMV1RV, MASK_VMV1RV, match_vmv_nf_rv, 0},
{"vmv2r.v",    0, INSN_CLASS_V, "Vd,Vt", MATCH_VMV2RV, MASK_VMV2RV, match_vmv_nf_rv, 0},
{"vmv4r.v",    0, INSN_CLASS_V, "Vd,Vt", MATCH_VMV4RV, MASK_VMV4RV, match_vmv_nf_rv, 0},
{"vmv8r.v",    0, INSN_CLASS_V, "Vd,Vt", MATCH_VMV8RV, MASK_VMV8RV, match_vmv_nf_rv, 0},

/* Svinval instructions.  */
{"sinval.vma",      0, INSN_CLASS_SVINVAL, "s,t", MATCH_SINVAL_VMA, MASK_SINVAL_VMA, match_opcode, 0 },
{"sfence.w.inval",  0, INSN_CLASS_SVINVAL, "", MATCH_SFENCE_W_INVAL, MASK_SFENCE_W_INVAL, match_opcode, 0 },
{"sfence.inval.ir", 0, INSN_CLASS_SVINVAL, "",  MATCH_SFENCE_INVAL_IR, MASK_SFENCE_INVAL_IR, match_opcode, 0 },
{"hinval.vvma",     0, INSN_CLASS_SVINVAL, "s,t", MATCH_HINVAL_VVMA, MASK_HINVAL_VVMA, match_opcode, 0 },
{"hinval.gvma",     0, INSN_CLASS_SVINVAL, "s,t", MATCH_HINVAL_GVMA, MASK_HINVAL_GVMA, match_opcode, 0 },

/* Terminate the list.  */
{0, 0, INSN_CLASS_NONE, 0, 0, 0, 0, 0 },
};


/* Vendor T-HEAD opcodes.  */
/*
   ARG format:

   "Xgm@n": encode GPR with m bit at opcode[m+n-1:n].
    "Xg5@0": encode GPR with 5 bit at opcode[4:0].
    "Xg5@8": encode GPR with 5 bit at opcode[12:8].

   "XIm@n": m bits unsigned immediate at opcode[m+n-1:n].
    "XI5@0": 5 bits unsigned immediate at opcode[4:0].
    "XI4@8": 4 bits unsigned immediate at opcode[11:8].

   "XSm@n": m bits signed immediate at opcode[m+n-1:n].
    "XS5@0": 5 bits signed immediate at opcode[4:0].
    "XS4@8": 4 bits signed immediate at opcode[11:8].

   "XFm@n": m bits FR at opcode[m+n-1:n].
    "XF5@0": 5 bits FR at opcode[4:0].
    "XF5@0": 5 bits FR at opcode[4:0].
*/

static int
match_thead_rd1_rd2_neq_rs1(const struct riscv_opcode *op,
			    insn_t insn,
			    int constraints,
			    const char **error)
{
  int rd1 = (insn & MASK_RD) >> OP_SH_RD;
  int rd2 = (insn & MASK_RS2) >> OP_SH_RS2;
  int rs1 = (insn & MASK_RS1) >> OP_SH_RS1;

  /* FIXME: add xlen check.  */
  return match_opcode (op, insn, constraints, error) && rd1 != rs1 && rd2 != rs1;
}

/* The vendor extension opcodes for T-HEAD.  */
struct riscv_opcode riscv_vendor_thead_opcodes[] =
{
  {"wsc",             0, INSN_CLASS_THEADC_OR_THEADE,   "",  MATCH_WSC, MASK_WSC, match_opcode, 0},
  {"dcache.iall",     0, INSN_CLASS_THEADC_OR_THEADE,   "",  MATCH_DCACHE_IALL, MASK_DCACHE_IALL, match_opcode, 0},
  {"dcache.call",     0, INSN_CLASS_THEADC_OR_THEADE,   "",  MATCH_DCACHE_CALL, MASK_DCACHE_CALL, match_opcode, 0},
  {"dcache.ciall",    0, INSN_CLASS_THEADC_OR_THEADE,   "",  MATCH_DCACHE_CIALL, MASK_DCACHE_CIALL, match_opcode, 0},
  {"dcache.isw",      0, INSN_CLASS_THEADC_OR_THEADE,   "s",     MATCH_DCACHE_ISW, MASK_DCACHE_ISW, match_opcode, 0},
  {"dcache.csw",      0, INSN_CLASS_THEADC_OR_THEADE,   "s",     MATCH_DCACHE_CSW, MASK_DCACHE_CSW, match_opcode, 0},
  {"dcache.cisw",     0, INSN_CLASS_THEADC_OR_THEADE,   "s",     MATCH_DCACHE_CISW, MASK_DCACHE_CISW, match_opcode, 0},
  {"dcache.iva",      0, INSN_CLASS_THEADC,   "s",     MATCH_DCACHE_IVA, MASK_DCACHE_IVA, match_opcode, 0},
  {"dcache.cva",      0, INSN_CLASS_THEADC,   "s",     MATCH_DCACHE_CVA, MASK_DCACHE_CVA, match_opcode, 0},
  {"dcache.cval1",    0, INSN_CLASS_THEADC,   "s",     MATCH_DCACHE_CVAL1, MASK_DCACHE_CVAL1, match_opcode, 0},
  {"dcache.civa",     0, INSN_CLASS_THEADC,   "s",     MATCH_DCACHE_CIVA, MASK_DCACHE_CIVA, match_opcode, 0},
  {"dcache.ipa",      0, INSN_CLASS_THEADC_OR_THEADE,   "s",     MATCH_DCACHE_IPA, MASK_DCACHE_IPA, match_opcode, 0},
  {"dcache.cpa",      0, INSN_CLASS_THEADC_OR_THEADE,   "s",     MATCH_DCACHE_CPA, MASK_DCACHE_CPA, match_opcode, 0},
  {"dcache.cpal1",    0, INSN_CLASS_THEADC,   "s",     MATCH_DCACHE_CPAL1, MASK_DCACHE_CPAL1, match_opcode, 0},
  {"dcache.cipa",     0, INSN_CLASS_THEADC_OR_THEADE,   "s",     MATCH_DCACHE_CIPA, MASK_DCACHE_CIPA, match_opcode, 0},
  {"icache.iall",     0, INSN_CLASS_THEADC_OR_THEADE_OR_THEADSE,   "",      MATCH_ICACHE_IALL, MASK_ICACHE_IALL, match_opcode, 0},
  {"icache.iall",     0, INSN_CLASS_THEADSE,   "",      MATCH_ICACHE_IALL, MASK_ICACHE_IALL, match_opcode, INSN_ALIAS},
  {"icache.ialls",    0, INSN_CLASS_THEADC,   "",      MATCH_ICACHE_IALLS, MASK_ICACHE_IALLS, match_opcode, 0},
  {"icache.iva",      0, INSN_CLASS_THEADC,   "s",     MATCH_ICACHE_IVA, MASK_ICACHE_IVA, match_opcode, 0},
  {"icache.ipa",      0, INSN_CLASS_THEADC_OR_THEADE,   "s",     MATCH_ICACHE_IPA, MASK_ICACHE_IPA, match_opcode, 0},
  {"l2cache.iall",    0, INSN_CLASS_THEADC,   "",      MATCH_L2CACHE_IALL, MASK_L2CACHE_IALL, match_opcode, 0},
  {"l2cache.call",    0, INSN_CLASS_THEADC,   "",      MATCH_L2CACHE_CALL, MASK_L2CACHE_CALL, match_opcode, 0},
  {"l2cache.ciall",   0, INSN_CLASS_THEADC_OR_THEADE,   "",      MATCH_L2CACHE_CIALL, MASK_L2CACHE_CIALL, match_opcode, 0},
  {"sync",            0, INSN_CLASS_THEADC_OR_THEADE,   "",      MATCH_SYNC, MASK_SYNC, match_opcode, 0},
  {"sync.i",          0, INSN_CLASS_THEADC_OR_THEADE,   "",      MATCH_SYNC_I, MASK_SYNC_I, match_opcode, 0},
  {"sync.s",          0, INSN_CLASS_THEADC,   "",      MATCH_SYNC_S, MASK_SYNC_S, match_opcode, 0},
  {"sync.is",         0, INSN_CLASS_THEADC,   "",      MATCH_SYNC_IS, MASK_SYNC_IS, match_opcode, 0},
  {"tstnbz",          0, INSN_CLASS_THEADC_OR_THEADE,   "d,s",     MATCH_TSTNBZ, MASK_TSTNBZ, match_opcode, 0},
  {"mula",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t",   MATCH_MULA, MASK_MULA, match_opcode, 0},
  {"muls",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t",   MATCH_MULS, MASK_MULS, match_opcode, 0},
  {"mulah",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t",   MATCH_MULAH, MASK_MULAH, match_opcode, 0},
  {"mulsh",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t",   MATCH_MULSH, MASK_MULSH, match_opcode, 0},
  {"sfence.vmas",     0, INSN_CLASS_THEADC,   "s,t",      MATCH_SFENCE_VMAS, MASK_SFENCE_VMAS, match_opcode, 0},
  {"mveqz",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t",   MATCH_MVEQZ, MASK_MVEQZ, match_opcode, 0},
  {"mvnez",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t",   MATCH_MVNEZ, MASK_MVNEZ, match_opcode, 0},
  {"mulaw",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t",   MATCH_MULAW, MASK_MULAW, match_opcode, 0},
  {"mulsw",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t",   MATCH_MULSW, MASK_MULSW, match_opcode, 0},
  {"ext",             64, INSN_CLASS_THEADC,  "d,s,XI6@26,XI6@20",   MATCH_EXT, MASK_EXT, match_opcode, 0 },
  {"ext",             32, INSN_CLASS_THEADE,  "d,s,XI5@26,XI5@20",   MATCH_EXT, (MASK_EXT | (1U<<25) | (1U<<31)), match_opcode, 0 },
  {"extu",            64, INSN_CLASS_THEADC,  "d,s,XI6@26,XI6@20",   MATCH_EXTU, MASK_EXTU, match_opcode, 0 },
  {"extu",            32, INSN_CLASS_THEADE,  "d,s,XI5@26,XI5@20",   MATCH_EXTU, (MASK_EXTU | (1U<<25) | (1U<<31)), match_opcode, 0 },
  {"ff1",             0, INSN_CLASS_THEADC_OR_THEADE,   "d,s",   MATCH_FF1, MASK_FF1, match_opcode, 0},
  {"ff0",             0, INSN_CLASS_THEADC_OR_THEADE,   "d,s",   MATCH_FF0, MASK_FF1, match_opcode, 0},
  {"rev",             0, INSN_CLASS_THEADC_OR_THEADE,   "d,s",   MATCH_REV, MASK_REV, match_opcode, 0},
  {"lrb",             0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t,XI2@25",   MATCH_LRB, MASK_LRB, match_opcode, 0 },
  {"lrbu",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t,XI2@25",   MATCH_LRBU, MASK_LRBU, match_opcode, 0 },
  {"lrh",             0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t,XI2@25",   MATCH_LRH, MASK_LRH, match_opcode, 0 },
  {"lrhu",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t,XI2@25",   MATCH_LRHU, MASK_LRHU, match_opcode, 0 },
  {"lrw",             0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t,XI2@25",   MATCH_LRW, MASK_LRW, match_opcode, 0 },
  {"lrwu",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t,XI2@25",   MATCH_LRWU, MASK_LRWU, match_opcode, 0 },
  {"srb",             0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t,XI2@25",   MATCH_SRB, MASK_SRB, match_opcode, 0 },
  {"srh",             0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t,XI2@25",   MATCH_SRH, MASK_SRH, match_opcode, 0 },
  {"srw",             0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t,XI2@25",   MATCH_SRW, MASK_SRW, match_opcode, 0 },
  {"lrd",             0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_LRD, MASK_LRD, match_opcode, 0 },
  {"srd",             0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_SRD, MASK_SRD, match_opcode, 0 },
  {"lurb",            0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_LURB, MASK_LURB, match_opcode, 0 },
  {"lurbu",           0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_LURBU, MASK_LURBU, match_opcode, 0 },
  {"lurh",            0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_LURH, MASK_LURH, match_opcode, 0 },
  {"lurhu",           0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_LURHU, MASK_LURHU, match_opcode, 0 },
  {"lurw",            0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_LURW, MASK_LURW, match_opcode, 0 },
  {"lurwu",           0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_LURWU, MASK_LURWU, match_opcode, 0 },
  {"lurd",            0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_LURD, MASK_LURD, match_opcode, 0 },
  {"surb",            0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_SURB, MASK_SURB, match_opcode, 0 },
  {"surh",            0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_SURH, MASK_SURH, match_opcode, 0 },
  {"surw",            0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_SURW, MASK_SURW, match_opcode, 0 },
  {"surd",            0, INSN_CLASS_THEADC,   "d,s,t,XI2@25",   MATCH_SURD, MASK_SURD, match_opcode, 0 },
  {"tst",             64, INSN_CLASS_THEADC,  "d,s,XI6@20",   MATCH_TST, MASK_TST, match_opcode, 0 },
  {"tst",             32, INSN_CLASS_THEADE,   "d,s,XI5@20",   MATCH_TST, (MASK_TST | (1U << 25)), match_opcode, INSN_ALIAS},
  {"srriw",           0, INSN_CLASS_THEADC,   "d,s,XI5@20",   MATCH_SRRIW, MASK_SRRIW, match_opcode, 0 },
  {"srri",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,XI6@20",   MATCH_SRRI, MASK_SRRI, match_opcode, 0 },
  {"addsl",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,s,t,XI2@25", MATCH_ADDSL, MASK_ADDSL, match_opcode, 0 },
  {"lwd",             0, INSN_CLASS_THEADC,   "d,t,(s),XI2@25", MATCH_LWD, MASK_LWD, match_thead_rd1_rd2_neq_rs1, 0 },
  {"ldd",             0, INSN_CLASS_THEADC,   "d,t,(s),XI2@25", MATCH_LDD, MASK_LDD, match_thead_rd1_rd2_neq_rs1, 0 },
  {"swd",             0, INSN_CLASS_THEADC,   "d,t,(s),XI2@25", MATCH_SWD, MASK_SWD, match_opcode, 0 },
  {"sdd",             0, INSN_CLASS_THEADC,   "d,t,(s),XI2@25", MATCH_SDD, MASK_SDD, match_opcode, 0 },
  {"sdia",            0, INSN_CLASS_THEADC,   "d,(s),XS5@20,XI2@25", MATCH_SDIA, MASK_SDIA, match_opcode, 0 },
  {"sdib",            0, INSN_CLASS_THEADC,   "d,(s),XS5@20,XI2@25", MATCH_SDIB, MASK_SDIB, match_opcode, 0 },
  {"lwud",            0, INSN_CLASS_THEADC,   "d,t,(s),XI2@25", MATCH_LWUD, MASK_LWUD, match_thead_rd1_rd2_neq_rs1, 0 },
  {"swia",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_SWIA, MASK_SWIA, match_opcode, 0},
  {"swib",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_SWIB, MASK_SWIB, match_opcode, 0},
  {"shia",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_SHIA, MASK_SHIA, match_opcode, 0},
  {"shib",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_SHIB, MASK_SHIB, match_opcode, 0},
  {"sbia",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_SBIA, MASK_SBIA, match_opcode, 0},
  {"sbib",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_SBIB, MASK_SBIB, match_opcode, 0},
  {"lwuia",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LWUIA, MASK_LWUIA, match_opcode, 0},
  {"lwuib",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LWUIB, MASK_LWUIB, match_opcode, 0},
  {"lhuia",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LHUIA, MASK_LHUIA, match_opcode, 0},
  {"lhuib",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LHUIB, MASK_LHUIB, match_opcode, 0},
  {"lbuia",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LBUIA, MASK_LBUIA, match_opcode, 0},
  {"lbuib",           0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LBUIB, MASK_LBUIB, match_opcode, 0},
  {"ldia",            0, INSN_CLASS_THEADC,   "d,(s),XS5@20,XI2@25", MATCH_LDIA, MASK_LDIA, match_opcode, 0},
  {"ldib",            0, INSN_CLASS_THEADC,   "d,(s),XS5@20,XI2@25", MATCH_LDIB, MASK_LDIB, match_opcode, 0},
  {"lwia",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LWIA, MASK_LWIA, match_opcode, 0},
  {"lwib",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LWIB, MASK_LWIB, match_opcode, 0},
  {"lhia",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LHIA, MASK_LHIA, match_opcode, 0},
  {"lhib",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LHIB, MASK_LHIB, match_opcode, 0},
  {"lbia",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LBIA, MASK_LBIA, match_opcode, 0},
  {"lbib",            0, INSN_CLASS_THEADC_OR_THEADE,   "d,(s),XS5@20,XI2@25", MATCH_LBIB, MASK_LBIB, match_opcode, 0},
  {"fsurd",           0, INSN_CLASS_THEADC,   "D,s,t,XI2@25", MATCH_FSURD, MASK_FSURD, match_opcode, 0},
  {"revw",            0, INSN_CLASS_THEADC,   "d,s", MATCH_REVW, MASK_REVW, match_opcode, 0},
  {"fsurw",           0, INSN_CLASS_THEADC,   "D,s,t,XI2@25", MATCH_FSURW, MASK_FSURW, match_opcode, 0},
  {"flurd",           0, INSN_CLASS_THEADC,   "D,s,t,XI2@25", MATCH_FLURD, MASK_FLURD, match_opcode, 0},
  {"flurw",           0, INSN_CLASS_THEADC,   "D,s,t,XI2@25", MATCH_FLURW, MASK_FLURW, match_opcode, 0},
  {"fsrd",            0, INSN_CLASS_THEADC,   "D,s,t,XI2@25", MATCH_FSRD, MASK_FSRD, match_opcode, 0},
  {"fsrw",            0, INSN_CLASS_THEADC,   "D,s,t,XI2@25", MATCH_FSRW, MASK_FSRW, match_opcode, 0},
  {"flrd",            0, INSN_CLASS_THEADC,   "D,s,t,XI2@25", MATCH_FLRD, MASK_FLRD, match_opcode, 0},
  {"flrw",            0, INSN_CLASS_THEADC,   "D,s,t,XI2@25", MATCH_FLRW, MASK_FLRW, match_opcode, 0},

  /* THEADE only.  */
  {"ipush",           0, INSN_CLASS_THEADE,   "",  MATCH_IPUSH, MASK_IPUSH, match_opcode, 0},
  {"ipop",            0, INSN_CLASS_THEADE,   "",  MATCH_IPOP, MASK_IPOP, match_opcode, 0},
  /* Float move.  */
  {"fmv.x.hw",        32, INSN_CLASS_THEADE,  "d,S",  MATCH_FMV_X_HW, MASK_FMV_X_HW, match_opcode, 0},
  {"fmv.hw.x",        32, INSN_CLASS_THEADE,  "D,s",  MATCH_FMV_HW_X, MASK_FMV_HW_X, match_opcode, 0},


  {0, 0, INSN_CLASS_NONE, 0, 0, 0, 0, 0}

};

/* The supported extended extensions.  */
const struct riscv_opcode *riscv_extended_opcodes[] =
{
  riscv_draft_opcodes,
  riscv_vendor_thead_opcodes,
  NULL
};
