/* Brew disassemble and simulation helpers.
   Copyright (C) 2009-2021 Free Software Foundation, Inc.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include <stdio.h>
#include <stdint.h>

#include "disassemble.h" // for OPCODES_ASSERT
#include "opcode/brew.h"

#ifndef INLINE
#if defined (__GNUC__) && defined (__OPTIMIZE__)
#define INLINE __inline__
#else
#define INLINE /*inline*/
#endif
#endif

#define DEBUG(...) { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
//#define DEBUG(...)

/* Macros to extract operands from the instruction word.  */
#define FIELD_C ((insn_code >> 12) & 0xf)
#define FIELD_B ((insn_code >> 8) & 0xf)
#define FIELD_A ((insn_code >> 4) & 0xf)
#define FIELD_D ((insn_code >> 0) & 0xf)

#define GET_NIBBLE(i, nibble) ((i >> (nibble*4)) & 0xf)

static int hexdigit(char digit)
{
  if (digit >= '0' && digit <= '9')
    return digit - '0';
  if (digit >= 'a' && digit <= 'f')
    return digit - 'a' + 10;
  if (digit >= 'A' && digit <= 'F')
    return digit - 'A' + 10;
  OPCODES_ASSERT(false);
}

static bool pattern_match(uint16_t insn_code, const char *pattern)
{
  const char *p;
  int nibble;
  for (p=pattern, nibble=strlen(pattern)-1;nibble >=0;--nibble, ++p)
    {
      if (*p == '.')
        {
          if (GET_NIBBLE(insn_code, nibble) == 0xf)
            {
              return false;
            }
          continue;
        }
      if (hexdigit(*p) != GET_NIBBLE(insn_code, nibble))
        {
          return false;
        }
    }
    return true;
}

const char * brew_reg_names[16] =
  { "$sp", "$fp", "$r2",  "$r3",  "$r4",  "$r5",  "$r6",  "$r7",
    "$r8", "$r9", "$r10", "$r11", "$r12", "$r13", "$r14", "<<<INVALID>>>"};

const char * brew_sreg_names[16] =
  { "$sr0", "$sr1", "$sr2",  "$sr3",  "$sr4",  "$sr5",  "$sr6",  "$sr7",
    "$sr8", "$sr9", "$sr10", "$sr11", "$sr12", "$sr13", "$sr14", "<<<INVALID>>>"};

const char * brew_freg_names[16] =
  { "$fr0", "$fr1", "$fr2",  "$fr3",  "$fr4",  "$fr5",  "$fr6",  "$fr7",
    "$fr8", "$fr9", "$fr10", "$fr11", "$fr12", "$fr13", "$fr14", "<<<INVALID>>>"};

#define REG_D (brew_reg_names[FIELD_D])
#define REG_A (brew_reg_names[FIELD_A])
#define REG_B (brew_reg_names[FIELD_B])

#define SREG_D (brew_sreg_names[FIELD_D])
#define SREG_A (brew_sreg_names[FIELD_A])
#define SREG_B (brew_sreg_names[FIELD_B])

#define FREG_D (brew_freg_names[FIELD_D])
#define FREG_A (brew_freg_names[FIELD_A])
#define FREG_B (brew_freg_names[FIELD_B])

#define TYPED_REG(type, idx) (typed_reg_names[type][idx])

#define SIM_REG(idx) ((uint32_t)sim_state->reg[idx])
#define SIM_SREG(idx) ((int32_t)sim_state->reg[idx])
#define SIM_FREG(idx) (as_float(sim_state->reg[idx]))
#define SIM_PC (sim_state->is_task_mode ? sim_state->tpc : sim_state->spc)
#define SIM_TPC sim_state->tpc

#define SIM_REG_T(idx) sim_state->dirty_map |= (1<<idx); sim_state->reg[idx]
#define SIM_SREG_T(idx) sim_state->dirty_map |= (1<<idx); sim_state->reg[idx]
#define SIM_REGD_T SIM_REG_T(FIELD_D)
#define SIM_SREGD_T SIM_SREG_T(FIELD_D)
#define SIM_PC_T *(sim_state->is_task_mode ? &sim_state->ntpc : &sim_state->nspc)
#define SIM_TPC_T sim_state->ntpc

#define SIM(...) { if(sim_state->read_mem != NULL) { __VA_ARGS__; } }

#define UNKNOWN { CLASS(UNKNOWN); SIM(sim_state->insn_exception = BREW_EXCEPTION_SII); if (fpr) print_unknown_insn(fpr, strm_or_buffer, insn_code, field_e, length); return; }


static const int field_c_to_bit_map[] =
  {
    0,  1,  2,  3,  4,  5,  6,  7,
    8,  9, 14, 15, 16, 30, 31, -1
  };

static void
print_unknown_insn(fprintf_ftype fpr, void *strm_or_buffer, uint16_t insn_code, uint32_t field_e, int length)
{
  switch (length)
    {
    case 2:
      fpr(strm_or_buffer, "unknown instruction 0x%04x", insn_code);
      break;
    case 4:
      fpr(strm_or_buffer, "unknown instruction 0x%04x 0x%04x", insn_code, field_e);
      break;
    case 6:
      fpr(strm_or_buffer, "unknown instruction 0x%04x 0x%08x", insn_code, field_e);
      break;
    }
}

int32_t
brew_unmunge_address(uint16_t field_e)
{
  // For now, we're simply shifting
  // FIXME: we should do proper munging.
  return ((int32_t)(field_e)) << 16 >> 15;
}

uint16_t
brew_munge_address(int32_t offset)
{
  // For now, we're simply shifting
  // FIXME: we should do proper munging.
  return (uint16_t)(offset >> 1);
}

typedef union {
  uint32_t as_uint;
  float as_float;
} int_or_float;

static inline float as_float(uint32_t val) { int_or_float iof; iof.as_uint = val; return iof.as_float; }
static inline uint32_t as_uint(float val) { int_or_float iof; iof.as_float = val; return iof.as_uint; }

static char *
format_branch_target(uint16_t field_e, char *buffer)
{
  int32_t offset = brew_unmunge_address(field_e);
  if (offset < 0)
    {
      sprintf(buffer, "<- $pc - %d (0x%x)", -offset, -offset);
    }
  else
    {
      sprintf(buffer, "<- $pc + %d (0x%x)", offset, offset);
    }
  return buffer;
}

static uint32_t
sim_branch_target(uint16_t field_e, uint32_t pc)
{
  int32_t offset = brew_unmunge_address(field_e);
  return pc + (uint32_t)offset;
}

static char *
format_link_offset(uint8_t field_b, char *buffer)
{
  int32_t offset = field_b << 1;
  sprintf(buffer, "<- $pc + %d (0x%x)", offset, offset);
  return buffer;
}

static uint32_t sim_link_offset(uint8_t field_b)
{
  return field_b << 1;
}

static char *
format_mem_ref(int ref_size, bool is_load ATTRIBUTE_UNUSED, int base_reg_idx, uint32_t offset, bool has_offset, char *buffer)
{
  bool has_base = base_reg_idx != 0xf;
  if (has_base && has_offset)
    sprintf(buffer, "MEM%d[%s, %u (0x%x)]", ref_size, brew_reg_names[base_reg_idx], offset, offset);
  else if (has_base)
    sprintf(buffer, "MEM%d[%s]", ref_size, brew_reg_names[base_reg_idx]);
  else if (has_offset)
    sprintf(buffer, "MEM%d[%u (0x%x)]", ref_size, offset, offset);
  else
    OPCODES_ASSERT(false);
  return buffer;
}

static INLINE uint32_t
sim_mem_calc_ref(void *context ATTRIBUTE_UNUSED, brew_sim_state *sim_state, int base_reg_idx, uint32_t offset, bool has_offset)
{
  bool has_base = base_reg_idx != 0xf;
  if (has_base && has_offset)
    return SIM_REG(base_reg_idx) + offset;
  else if (has_base)
    return SIM_REG(base_reg_idx);
  else if (has_offset)
    return offset;
  else
    OPCODES_ASSERT(false);
}

static uint32_t
sim_mem_load(void *context, brew_sim_state *sim_state, int ref_size, bool is_signed, int base_reg_idx, uint32_t offset, bool has_offset)
{
  uint32_t vma = sim_mem_calc_ref(context, sim_state, base_reg_idx, offset, has_offset);
  uint32_t val;
  sim_state->insn_exception = sim_state->read_mem(context, vma, ref_size/8, &val);
  OPCODES_ASSERT((ref_size & 7) == 0);
  if (is_signed) {
    int bits_unused = 32 - ref_size;
    OPCODES_ASSERT(bits_unused >= 0 && bits_unused < 31);
    val = (int32_t)val << bits_unused >> bits_unused;
  }
  return val;
}

static void
sim_mem_store(void *context, brew_sim_state *sim_state, int ref_size, int base_reg_idx, uint32_t offset, bool has_offset, uint32_t val)
{
  uint32_t vma = sim_mem_calc_ref(context, sim_state, base_reg_idx, offset, has_offset);
  sim_state->insn_exception = sim_state->write_mem(context, vma, ref_size/8, val);
  OPCODES_ASSERT((ref_size & 7) == 0);
}


#define INST(...) { if(fpr) fpr(strm_or_buffer, __VA_ARGS__ ); return; }

#define CLASS_NAME(insn_class) BREW_INSN_CLS_##insn_class
#define CLASS(class) { sim_state->insn_class = CLASS_NAME(class); }

typedef enum
{
  OPERAND_UNSIGNED,
  OPERAND_SIGNED,
  OPERAND_FLOAT
} operand_type;

static const char ** typed_reg_names[] = {brew_reg_names, brew_sreg_names, brew_freg_names};

static bool
binary_op(
  brew_sim_state *sim_state,
  fprintf_ftype fpr,
  void *strm_or_buffer,
  uint16_t insn_code,
  uint32_t field_e,
  const char *operation,
  uint32_t (*sim_op)(uint32_t, uint32_t),
  operand_type type_d,
  operand_type type_a,
  operand_type type_b,
  const char *operation_prefix,
  brew_insn_classes insn_class
) {
  bool is_immedate = FIELD_A == 0xf || FIELD_B == 0xf;
  bool is_short = FIELD_B == 0xf;

  sim_state->insn_class = insn_class;

  if (operation_prefix == NULL)
    operation_prefix = "";

  // Test supported operand combinations
  if (
    !(type_d == OPERAND_UNSIGNED && type_a == OPERAND_UNSIGNED && type_b == OPERAND_UNSIGNED) &&
    !(type_d == OPERAND_SIGNED && type_a == OPERAND_SIGNED && type_b == OPERAND_UNSIGNED) &&
    !(type_d == OPERAND_SIGNED && type_a == OPERAND_SIGNED && type_b == OPERAND_SIGNED) &&
    !(type_d == OPERAND_FLOAT && type_a == OPERAND_FLOAT && type_b == OPERAND_FLOAT)
  ) {
    OPCODES_ASSERT(false);
  }
  if (
    (FIELD_A == 0xf && FIELD_B == 0xf) ||
    (is_short && type_d == OPERAND_FLOAT) // Don't support short immediates for floating point operations
  ) {
      print_unknown_insn(fpr, strm_or_buffer, insn_code, field_e, brew_insn_len(insn_code));
      return false;
    }
  if (is_immedate)
    {
      char *prefix = is_short ? "short " : "";
      char immed_str[255];
      float field_e_as_float;
      int reg_idx = is_short ? FIELD_A : FIELD_B;

      if (is_short) field_e = (int32_t)field_e << 16 >> 16; // sign-extend to 32-bits
      memcpy(&field_e_as_float, &field_e, 4);

      switch (type_b)
        {
        case OPERAND_UNSIGNED: sprintf(immed_str, "%u (0x%x)", field_e, field_e); break;
        case OPERAND_SIGNED: sprintf(immed_str, "%d (0x%x)", field_e, field_e); break;
        case OPERAND_FLOAT: sprintf(immed_str, "%f (0x%x)", field_e_as_float, field_e); break;
        default: OPCODES_ASSERT(false);
        }

      fpr(strm_or_buffer, "%s <- %s%s%s %s %s", TYPED_REG(type_d, FIELD_D), prefix, operation_prefix, immed_str, operation, TYPED_REG(type_a, reg_idx));
      SIM(SIM_REGD_T = sim_op(field_e, SIM_REG(reg_idx)));
    }
  else
    {
      fpr(strm_or_buffer, "%s <- %s%s %s %s", TYPED_REG(type_d, FIELD_D), operation_prefix, TYPED_REG(type_a, FIELD_A), operation, TYPED_REG(type_b, FIELD_B));
      SIM(SIM_REGD_T = sim_op(SIM_REG(FIELD_A), SIM_REG(FIELD_B)));
    }
  return true;
}

#define REGULAR_ALU_PATTERN_UUU(base, op, sim_op, prefix, insn_class)                                                                                                                          \
  case 0x##base: if (!binary_op(sim_state, fpr, strm_or_buffer, insn_code, field_e, #op, sim_op, OPERAND_UNSIGNED, OPERAND_UNSIGNED, OPERAND_UNSIGNED, prefix, CLASS_NAME(insn_class))) break; return;     \

#define REGULAR_ALU_PATTERN_SSU(base, op, sim_op, prefix, insn_class)                                                                                                                          \
  case 0x##base: if (!binary_op(sim_state, fpr, strm_or_buffer, insn_code, field_e, #op, sim_op, OPERAND_SIGNED, OPERAND_SIGNED, OPERAND_UNSIGNED, prefix, CLASS_NAME(insn_class))) break; return;         \

#define REGULAR_ALU_PATTERN_SSS(base, op, sim_op, prefix, insn_class)                                                                                                                          \
  case 0x##base: if (!binary_op(sim_state, fpr, strm_or_buffer, insn_code, field_e, #op, sim_op, OPERAND_SIGNED, OPERAND_SIGNED, OPERAND_SIGNED, prefix, CLASS_NAME(insn_class))) break; return;           \

#define REGULAR_ALU_PATTERN_FFF(base, op, sim_op, prefix, insn_class)                                                                                                                          \
  case 0x##base: if (!binary_op(sim_state, fpr, strm_or_buffer, insn_code, field_e, #op, sim_op, OPERAND_FLOAT, OPERAND_FLOAT, OPERAND_FLOAT, prefix, CLASS_NAME(insn_class))) break; return;              \

static INLINE uint32_t bswap(uint32_t val)
{
  return
    (((val >> 0) & 0xff) << 24) |
    (((val >> 8) & 0xff) << 16) |
    (((val >> 16) & 0xff) << 8) |
    (((val >> 24) & 0xff) << 0);
}

static INLINE uint32_t wswap(uint32_t val)
{
  return
    (((val >> 0) & 0xffff) << 16) |
    (((val >> 16) & 0xffff) << 0);
}

static INLINE uint32_t bsi(uint32_t val)
{
  return
    ((val & 0x80) != 0) ?
      (0xffffff00 | (val & 0xff)) :
      (val & 0xff);
}

static INLINE uint32_t wsi(uint32_t val)
{
  return
    ((val & 0x8000) != 0) ?
      (0xffff0000 | (val & 0xffff)) :
      (val & 0xffff);
}

static INLINE uint32_t xor_op(uint32_t a, uint32_t b) { return a ^ b; }
static INLINE uint32_t ior_op(uint32_t a, uint32_t b) { return a | b; }
static INLINE uint32_t and_op(uint32_t a, uint32_t b) { return a & b; }
static INLINE uint32_t add_op(uint32_t a, uint32_t b) { return a + b; }
static INLINE uint32_t sub_op(uint32_t a, uint32_t b) { return a - b; }
static INLINE uint32_t lsh_op(uint32_t a, uint32_t b) { return a << (b & 31); }
static INLINE uint32_t rsh_op(uint32_t a, uint32_t b) { return a >> (b & 31); }
static INLINE uint32_t srsh_op(uint32_t a, uint32_t b) { return ((int32_t)a) >> (b & 31); }
static INLINE uint32_t mul_op(uint32_t a, uint32_t b) { return a * b; }
static INLINE uint32_t uumul_op(uint32_t a, uint32_t b) { return ((uint64_t)a * (uint64_t)b) >> 32; }
static INLINE uint32_t sumul_op(uint32_t a, uint32_t b) { return ((int64_t)(int32_t)a * (int64_t)(int32_t)b) >> 32; }
static INLINE uint32_t fadd_op(uint32_t a, uint32_t b) { return as_uint(as_float(a) + as_float(b)); }
static INLINE uint32_t fsub_op(uint32_t a, uint32_t b) { return as_uint(as_float(a) - as_float(b)); }
static INLINE uint32_t fmul_op(uint32_t a, uint32_t b) { return as_uint(as_float(a) * as_float(b)); }

static INLINE int get_bit(uint32_t word, int idx) { return (word >> idx) & 1; }

void
brew_sim_insn(void *context ATTRIBUTE_UNUSED, brew_sim_state *sim_state, uint16_t insn_code, uint32_t field_e)
{
  int length;

  float field_e_as_float;
  char str_buffer[255];

  fprintf_ftype fpr = sim_state->tracer;
  void *strm_or_buffer = sim_state->tracer_strm;

  length = brew_insn_len(insn_code);
  if (length == 6)
    {
      memcpy(&field_e_as_float, &field_e, 4);
    }
  if (length == 4)
    {
      field_e = (int32_t)field_e << 16 >> 16; // Sign-extend to 32-bits
    }
  if (FIELD_D != 0xf && FIELD_C != 0xf)
    {
      // Some rather special-case pseudo-ops:
      if (FIELD_C == 2 && FIELD_B == FIELD_A && FIELD_D == FIELD_A) { CLASS(NOP); INST("NOP"); } // $rD <- $rD | $rD
      if (FIELD_C == 1 && FIELD_B == FIELD_A) { CLASS(LOGIC); SIM(SIM_REGD_T = 0;) INST("%s <- 0", REG_D); } // $rD <- $rX ^ $rX
      if (FIELD_C == 2 && FIELD_B == FIELD_A) { CLASS(LOGIC); SIM(SIM_REGD_T = SIM_REG(FIELD_B)); INST("%s <- %s", REG_D, REG_B); } // $rD <- $rX | $rX

      // ALU group
      switch (FIELD_C)
        {
        case 0x0: // Unary and special
          // Load immediate
          if (pattern_match(insn_code, "00f.")) { CLASS(IMM); SIM(SIM_REGD_T = field_e); INST("%s <- %u (0x%x)", REG_D, field_e, field_e); }
          if (pattern_match(insn_code, "01f.")) { CLASS(BRANCH); SIM(SIM_PC_T = field_e); INST("$pc <- %u (0x%x)", field_e, field_e); }

          if (pattern_match(insn_code, "0f0.")) { CLASS(IMM); SIM(SIM_REGD_T = field_e); INST("%s <- short %u (0x%x)", REG_D, field_e, field_e); }

          if ((insn_code >> 4) == 0x000)
            {
              // Exception group
              if (pattern_match(insn_code, "0000")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_FILL); INST("FILL"); }
              if (pattern_match(insn_code, "0001")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_BREAK); INST("BREAK"); }
              if (pattern_match(insn_code, "0002")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_SYSCALL); INST("SYSCALL"); }
              if (pattern_match(insn_code, "0003")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_SWI3); INST("SWI3"); }
              if (pattern_match(insn_code, "0004")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_SWI4); INST("SWI4"); }
              if (pattern_match(insn_code, "0005")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_SWI5); INST("SWI5"); }
              if (pattern_match(insn_code, "0006")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_SII); INST("SII"); }
              if (pattern_match(insn_code, "0007")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_HWI); INST("HWI"); }

              if (pattern_match(insn_code, "0008")) { CLASS(BRANCH); SIM(sim_state->nis_task_mode = true); INST("STU"); }
              if (pattern_match(insn_code, "0009")) { CLASS(POWER); INST("WOI"); }
              UNKNOWN;
            }
          if ((insn_code >> 4) == 0x001)
            {
              // Atomic group
              if (pattern_match(insn_code, "0010")) { CLASS(ATOMIC); INST("FENCE"); }
              if (pattern_match(insn_code, "0011")) { CLASS(ATOMIC); INST("WFENCE"); }
              UNKNOWN;
            }
          if ((insn_code >> 8) == 0x00)
            {
              // Branch group
              if (pattern_match(insn_code, "002.")) { CLASS(BRANCH); SIM(SIM_PC_T = SIM_REG(FIELD_D)); INST("$pc <- %s", REG_D); }
              if (pattern_match(insn_code, "003.")) { CLASS(MOV); SIM(SIM_TPC_T = SIM_REG(FIELD_D)); INST("$tpc <- %s", REG_D); }
              if (pattern_match(insn_code, "004.")) { CLASS(MOV); SIM(SIM_REGD_T = SIM_PC); INST("%s <- $pc", REG_D); }
              if (pattern_match(insn_code, "005.")) { CLASS(MOV); SIM(SIM_REGD_T = SIM_TPC); INST("%s <- $tpc", REG_D); }
              UNKNOWN;
            }
          // Unary group
          if (pattern_match(insn_code, "01..")) { CLASS(ARITH); SIM(SIM_REGD_T = SIM_REG(FIELD_A) + 1); INST("%s <- %s + 1", REG_D, REG_A); }
          if (pattern_match(insn_code, "02..")) { CLASS(ARITH); SIM(SIM_REGD_T = SIM_REG(FIELD_A) - 1); INST("%s <- %s - 1", REG_D, REG_A); }
          if (pattern_match(insn_code, "03..")) { CLASS(ARITH); SIM(SIM_SREGD_T = -SIM_SREG(FIELD_A)); INST("%s <- -%s", SREG_D, SREG_A); }
          if (pattern_match(insn_code, "04..")) { CLASS(BIT); SIM(SIM_REGD_T = ~SIM_REG(FIELD_A)); INST("%s <- ~%s", REG_D, REG_A); }
          if (pattern_match(insn_code, "05..")) { CLASS(BIT); SIM(SIM_REGD_T = bswap(SIM_REG(FIELD_A))); INST("%s <- bswap %s", REG_D, REG_A); }
          if (pattern_match(insn_code, "06..")) { CLASS(BIT); SIM(SIM_REGD_T = wswap(SIM_REG(FIELD_A))); INST("%s <- wswap %s", REG_D, REG_A); }
          if (pattern_match(insn_code, "07..")) { CLASS(BIT); SIM(SIM_REGD_T = bsi(SIM_REG(FIELD_A))); INST("%s <- bsi %s", REG_D, REG_A); }
          if (pattern_match(insn_code, "08..")) { CLASS(BIT); SIM(SIM_REGD_T = wsi(SIM_REG(FIELD_A))); INST("%s <- wsi %s", REG_D, REG_A); }
          if (pattern_match(insn_code, "09..")) { CLASS(FP); SIM(SIM_REGD_T = sim_state->floor(SIM_FREG(FIELD_A))); INST("%s <- floor %s", SREG_D, FREG_A); }
          if (pattern_match(insn_code, "0a..")) { CLASS(FP); SIM(SIM_REGD_T = as_uint(SIM_SREG(FIELD_A))); INST("%s <- %s", FREG_D, SREG_A); }
          if (pattern_match(insn_code, "0b..")) {
            CLASS(FP);
            SIM(
              if (SIM_FREG(FIELD_A) == 0.0f)
                sim_state->insn_exception = BREW_EXCEPTION_F_DIV_BY_ZERO;
              else
                SIM_REGD_T = as_uint(1.0f / SIM_FREG(FIELD_A));
            );
            INST("%s <- 1 / %s", FREG_D, FREG_A);
          }
          if (pattern_match(insn_code, "0c..")) {
            CLASS(FP);
            SIM(
              if (SIM_FREG(FIELD_A) <= 0.0f)
                sim_state->insn_exception = BREW_EXCEPTION_F_NEG_RSQRT;
              else
                SIM_REGD_T = as_uint(sim_state->rsqrt(SIM_FREG(FIELD_A)));
            );
            INST("%s <- rsqrt %s", FREG_D, FREG_A);
          }
          if (pattern_match(insn_code, "0d..")) { CLASS(LINK); SIM(SIM_REGD_T = SIM_PC + sim_link_offset(FIELD_A)); INST("%s <- $pc + %s", REG_D, format_link_offset(FIELD_A, str_buffer)); }

          UNKNOWN;
          break;
        REGULAR_ALU_PATTERN_UUU(1, ^, xor_op, "", LOGIC);
        REGULAR_ALU_PATTERN_UUU(2, |, ior_op, "", LOGIC);
        REGULAR_ALU_PATTERN_UUU(3, &, and_op, "", LOGIC);
        REGULAR_ALU_PATTERN_UUU(4, +, add_op, "", ARITH);
        REGULAR_ALU_PATTERN_UUU(5, -, sub_op, "", ARITH);
        REGULAR_ALU_PATTERN_UUU(6, <<, lsh_op, "", SHIFT);
        REGULAR_ALU_PATTERN_UUU(7, >>, rsh_op, "", SHIFT);
        REGULAR_ALU_PATTERN_SSU(8, >>, srsh_op, "", SHIFT);
        REGULAR_ALU_PATTERN_UUU(9, *, mul_op, "", MUL);
        REGULAR_ALU_PATTERN_UUU(a, *, uumul_op, "upper ", MUL);
        REGULAR_ALU_PATTERN_SSS(b, *, sumul_op, "upper ", MUL);
        REGULAR_ALU_PATTERN_FFF(c, +, fadd_op, "", FP);
        REGULAR_ALU_PATTERN_FFF(d, -, fsub_op, "", FP);
        REGULAR_ALU_PATTERN_FFF(e, *, fmul_op, "", FP);
        }
    }
  else if (FIELD_C == 0xf)
    {
      // load-store group
      if (FIELD_D != 0xf)
        {
          switch (FIELD_B & 0x7)
            {
              case 0: SIM( CLASS(LD); SIM_REGD_T = sim_mem_load(context, sim_state,  8, true,  SIM_REG(FIELD_A), field_e, FIELD_B & 0x8)); INST("%s <- %s", SREG_D, format_mem_ref(8,  true, FIELD_A, field_e, FIELD_B & 0x8, str_buffer));
              case 1: SIM( CLASS(LD); SIM_REGD_T = sim_mem_load(context, sim_state,  8, false, SIM_REG(FIELD_A), field_e, FIELD_B & 0x8)); INST("%s <- %s", REG_D,  format_mem_ref(8,  true, FIELD_A, field_e, FIELD_B & 0x8, str_buffer));
              case 2: SIM( CLASS(LD); SIM_REGD_T = sim_mem_load(context, sim_state, 16, true,  SIM_REG(FIELD_A), field_e, FIELD_B & 0x8)); INST("%s <- %s", SREG_D, format_mem_ref(16, true, FIELD_A, field_e, FIELD_B & 0x8, str_buffer));
              case 3: SIM( CLASS(LD); SIM_REGD_T = sim_mem_load(context, sim_state, 16, false, SIM_REG(FIELD_A), field_e, FIELD_B & 0x8)); INST("%s <- %s", REG_D,  format_mem_ref(16, true, FIELD_A, field_e, FIELD_B & 0x8, str_buffer));
              case 4: SIM( CLASS(LD); SIM_REGD_T = sim_mem_load(context, sim_state, 32, false, SIM_REG(FIELD_A), field_e, FIELD_B & 0x8)); INST("%s <- %s", REG_D,  format_mem_ref(32, true, FIELD_A, field_e, FIELD_B & 0x8, str_buffer));
              case 5: SIM( CLASS(ST); sim_mem_store(context, sim_state,  8, SIM_REG(FIELD_A), field_e, FIELD_B & 0x8, SIM_REG(FIELD_D))); INST("%s <- %s", format_mem_ref(8,  false, FIELD_A, field_e, FIELD_B & 0x8, str_buffer), REG_D);
              case 6: SIM( CLASS(ST); sim_mem_store(context, sim_state, 16, SIM_REG(FIELD_A), field_e, FIELD_B & 0x8, SIM_REG(FIELD_D))); INST("%s <- %s", format_mem_ref(16, false, FIELD_A, field_e, FIELD_B & 0x8, str_buffer), REG_D);
              case 7: SIM( CLASS(ST); sim_mem_store(context, sim_state, 32, SIM_REG(FIELD_A), field_e, FIELD_B & 0x8, SIM_REG(FIELD_D))); INST("%s <- %s", format_mem_ref(32, false, FIELD_A, field_e, FIELD_B & 0x8, str_buffer), REG_D);
            }
        }
      else
        {
          if (pattern_match(insn_code, "fcff")) { CLASS(BRANCH); SIM(SIM_PC_T = sim_mem_load(context, sim_state, 32, false, SIM_REG(FIELD_A), field_e, FIELD_B & 0x8)); INST("$pc <- %s", format_mem_ref(32, true, FIELD_A, field_e, FIELD_B & 0x8, str_buffer)); }
          if (pattern_match(insn_code, "fc.f")) { CLASS(BRANCH); SIM(SIM_PC_T = sim_mem_load(context, sim_state, 32, false, SIM_REG(FIELD_A), field_e, FIELD_B & 0x8)); INST("$pc <- %s", format_mem_ref(32, true, FIELD_A, field_e, FIELD_B & 0x8, str_buffer)); }
        }

    }
  else if (FIELD_D == 0xf && FIELD_C != 0xf)
    {
      // conditional branch group
      if (pattern_match(insn_code, "00.f")) { CLASS(CBRANCH0); SIM(if (SIM_REG(FIELD_A)  == 0) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s == 0 $pc %s",  REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "01.f")) { CLASS(CBRANCH0); SIM(if (SIM_REG(FIELD_A)  != 0) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s != 0 $pc %s",  REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "02.f")) { CLASS(CBRANCH0); SIM(if (SIM_SREG(FIELD_A)  < 0) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s < 0 $pc %s",  SREG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "03.f")) { CLASS(CBRANCH0); SIM(if (SIM_SREG(FIELD_A) >= 0) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s >= 0 $pc %s", SREG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "04.f")) { CLASS(CBRANCH0); SIM(if (SIM_SREG(FIELD_A)  > 0) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s > 0 $pc %s",  SREG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "05.f")) { CLASS(CBRANCH0); SIM(if (SIM_SREG(FIELD_A) <= 0) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s <= 0 $pc %s", SREG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "06.f")) { UNKNOWN; }
      if (pattern_match(insn_code, "07.f")) { UNKNOWN; }
      if (pattern_match(insn_code, "08.f")) { UNKNOWN; }
      if (pattern_match(insn_code, "09.f")) { UNKNOWN; }
      if (pattern_match(insn_code, "0a.f")) { UNKNOWN; }
      if (pattern_match(insn_code, "0b.f")) { CLASS(CBRANCH0FP); SIM(if (SIM_FREG(FIELD_A)  < 0.0f) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s < 0 $pc %s",  FREG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "0c.f")) { CLASS(CBRANCH0FP); SIM(if (SIM_FREG(FIELD_A) >= 0.0f) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s >= 0 $pc %s", FREG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "0d.f")) { CLASS(CBRANCH0FP); SIM(if (SIM_FREG(FIELD_A)  > 0.0f) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s > 0 $pc %s",  FREG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "0e.f")) { CLASS(CBRANCH0FP); SIM(if (SIM_FREG(FIELD_A) <= 0.0f) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s <= 0 $pc %s", FREG_A, format_branch_target(field_e, str_buffer)); }

      if (pattern_match(insn_code, "1..f")) { CLASS(CBRANCH); SIM(if (SIM_REG(FIELD_B)  == SIM_REG(FIELD_A))  SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s == %s $pc %s",  REG_B,  REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "2..f")) { CLASS(CBRANCH); SIM(if (SIM_REG(FIELD_B)  != SIM_REG(FIELD_A))  SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s != %s $pc %s",  REG_B,  REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "3..f")) { CLASS(CBRANCH); SIM(if (SIM_SREG(FIELD_B)  < SIM_SREG(FIELD_A)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s < %s $pc %s",  SREG_B, SREG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "4..f")) { CLASS(CBRANCH); SIM(if (SIM_SREG(FIELD_B) >= SIM_SREG(FIELD_A)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s >= %s $pc %s", SREG_B, SREG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "5..f")) { CLASS(CBRANCH); SIM(if (SIM_REG(FIELD_B)   < SIM_REG(FIELD_A))  SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s < %s $pc %s",   REG_B,  REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "6..f")) { CLASS(CBRANCH); SIM(if (SIM_REG(FIELD_B)  >= SIM_REG(FIELD_A))  SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s >= %s $pc %s",  REG_B,  REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "7..f")) { UNKNOWN; }
      if (pattern_match(insn_code, "8..f")) { UNKNOWN; }
      if (pattern_match(insn_code, "9..f")) { UNKNOWN; }
      if (pattern_match(insn_code, "a..f")) { UNKNOWN; }
      if (pattern_match(insn_code, "b..f")) { UNKNOWN; }
      if (pattern_match(insn_code, "c..f")) { UNKNOWN; }
      if (pattern_match(insn_code, "d..f")) { CLASS(CBRANCHFP); SIM(if (SIM_FREG(FIELD_B)  < SIM_FREG(FIELD_A)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s < %s $pc %s",  FREG_B, FREG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "e..f")) { CLASS(CBRANCHFP); SIM(if (SIM_FREG(FIELD_B) >= SIM_FREG(FIELD_A)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s >= %s $pc %s", FREG_B, FREG_A, format_branch_target(field_e, str_buffer)); }

      if (pattern_match(insn_code, ".f.f")) { CLASS(CBRANCHBIT); SIM(if (get_bit(SIM_REG(FIELD_A), field_c_to_bit_map[FIELD_C]) == 1) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s[%d] == 1 $pc %s", REG_A, field_c_to_bit_map[FIELD_C], format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "..ff")) { CLASS(CBRANCHBIT); SIM(if (get_bit(SIM_REG(FIELD_A), field_c_to_bit_map[FIELD_C]) == 0) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s[%d] == 0 $pc %s", REG_B, field_c_to_bit_map[FIELD_C], format_branch_target(field_e, str_buffer)); }
    }
  UNKNOWN;
}

int
brew_insn_len(uint16_t insn_code)
{
  if (FIELD_D == 0xf || FIELD_B == 0xf || FIELD_A == 0xf || (insn_code >> 11) == 0x1f)
    // We do need FIELD_E, but what size?
    if (
      (insn_code >> 11) == 0x1f ? (FIELD_A == 0xf) : (FIELD_A == 0xf && FIELD_D != 0xf)
    )
      return 6;
    else
      return 4;
  else
    return 2;
}


void
brew_print_insn(fprintf_ftype fpr, void *strm_or_buffer, uint16_t insn_code, uint32_t field_e)
{
  brew_sim_state sim_state;
  memset(&sim_state, 0, sizeof(sim_state));
  sim_state.tracer = fpr;
  sim_state.tracer_strm = strm_or_buffer;
  brew_sim_insn(NULL, &sim_state, insn_code, field_e);
}

static const char *insn_class_names[] = {
  "unknown",
  "exception/syscall",
  "atomic",
  "move",
  "load immediate",
  "bit manipulation",
  "logic",
  "arithmetic",
  "shift",
  "multiply",
  "floating point",
  "load",
  "store",
  "unconditional branch",
  "conditional branch",
  "conditional branch with 0",
  "floating point conditional branch",
  "floating point conditional branch with 0",
  "bit-test conditional branch",
  "link setup",
  "power management",
  "nop",
  "----"
};

const char *
brew_insn_class_name(brew_insn_classes cls)
{
  OPCODES_ASSERT(cls < BREW_INSN_CLS_MAX);
  OPCODES_ASSERT(cls < sizeof(insn_class_names)/sizeof(insn_class_names[0]));
  return insn_class_names[cls];
}

