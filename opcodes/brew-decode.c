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
#include <libiberty.h>
#include <stdio.h>
#include <stdint.h>

#include "disassemble.h" // for OPCODES_ASSERT
#include "opcode/brew.h"
#include "opcode/brew-abi.h"

typedef enum
{
  COND_EQ,
  COND_NE,
  COND_LT,
  COND_LE,
  COND_GT,
  COND_GE
} brew_cond;

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
#define FIELD_D ((insn_code >> 12) & 0xf)
#define FIELD_C ((insn_code >> 8) & 0xf)
#define FIELD_B ((insn_code >> 4) & 0xf)
#define FIELD_A ((insn_code >> 0) & 0xf)

#define EXT_FIELD_D ((field_e >> 12) & 0xf)
#define EXT_FIELD_C ((field_e >> 8) & 0xf)
#define EXT_FIELD_B ((field_e >> 4) & 0xf)
#define EXT_FIELD_A ((field_e >> 0) & 0xf)

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
      if (*p == '*')
        continue;
      if (hexdigit(*p) != GET_NIBBLE(insn_code, nibble))
        {
          return false;
        }
    }
    return true;
}

const char * brew_reg_names[16] =
  { "$r0", "$r1", "$r2",  "$r3",  "$r4",  "$r5",  "$r6",  "$r7",
    "$r8", "$r9", "$r10", "$r11", "$fp", "$sp", "$lr", "<<<INVALID>>>"};

#define REG_D (brew_reg_names[FIELD_D])
#define REG_A (brew_reg_names[FIELD_A])
#define REG_B (brew_reg_names[FIELD_B])
#define REG(r) (brew_reg_names[r])

#define SIM_REG(idx) ((brew_typed_reg)(sim_state->reg[idx]))
#define SIM_REG_TYPE(idx) SIM_REG(idx).type
#define SIM_REGD_TYPE SIM_REG_TYPE(FIELD_D)

#define SIM_PC (sim_state->is_task_mode ? sim_state->tpc : sim_state->spc)
#define SIM_TPC sim_state->tpc

#define SIM_REG_T(idx) sim_state->dirty_map |= (1<<(idx)); sim_state->reg[idx]
#define SIM_REGD_T SIM_REG_T(FIELD_D)
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
  if (!fpr)
    return;
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
  float as_fp32;
  _Float16 as_fp16[2];
} int_or_float;

typedef union {
  uint16_t as_uint;
  _Float16 as_fp16;
} int16_or_fp16;

static INLINE float as_fp32(uint32_t val) { int_or_float iof; iof.as_uint = val; return iof.as_fp32; }
static INLINE uint32_t as_uint32(float val) { int_or_float iof; iof.as_fp32 = val; return iof.as_uint; }
static INLINE _Float16 as_fp16(uint16_t val) { int16_or_fp16 iof; iof.as_uint = val; return iof.as_fp16; }
static INLINE uint32_t as_uint16(_Float16 val) { int16_or_fp16 iof; iof.as_fp16 = val; return iof.as_uint; }

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
  sprintf(buffer, "%d (0x%x)", offset, offset);
  return buffer;
}

static uint32_t sim_link_offset(uint8_t field_b)
{
  return field_b << 1;
}

static char *
format_mem_ref(const char *mem_ref, int base_reg_idx, uint32_t offset, bool has_offset, bool is_tiny, char *buffer)
{
  bool has_base = base_reg_idx != 0xf;
  if (has_base && has_offset)
    {
      if ((int)offset < 0)
        sprintf(buffer, "%s[%s - %s%u (0x%x)]", mem_ref, brew_reg_names[base_reg_idx], is_tiny?"tiny ":"", -(int)offset, offset);
      else
        sprintf(buffer, "%s[%s + %s%u (0x%x)]", mem_ref, brew_reg_names[base_reg_idx], is_tiny?"tiny ":"", offset, offset);
    }
  else if (has_base)
    sprintf(buffer, "%s[%s]", mem_ref, brew_reg_names[base_reg_idx]);
  else if (has_offset)
    sprintf(buffer, "%s[%u (0x%x)]", mem_ref, offset, offset);
  else
    OPCODES_ASSERT(false);
  return buffer;
}

static INLINE uint32_t
sim_mem_calc_ref(void *context ATTRIBUTE_UNUSED, brew_sim_state *sim_state, int base_reg_idx, uint32_t offset, bool has_offset)
{
  bool has_base = base_reg_idx != 0xf;
  if (has_base && has_offset)
    return SIM_REG(base_reg_idx).val + offset;
  else if (has_base)
    return SIM_REG(base_reg_idx).val;
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
  sim_state->insn_exception = sim_state->read_mem(context, vma, ref_size, &val);
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
  sim_state->insn_exception = sim_state->write_mem(context, vma, ref_size, val);
  OPCODES_ASSERT((ref_size & 7) == 0);
}


#define INST(...) { if(fpr) fpr(strm_or_buffer, __VA_ARGS__ ); return; }

#define CLASS_NAME(insn_class) BREW_INSN_CLS_##insn_class
#define CLASS(class) { sim_state->insn_class = CLASS_NAME(class); }

static bool
binary_op(
  brew_sim_state *sim_state,
  fprintf_ftype fpr,
  void *strm_or_buffer,
  uint16_t insn_code,
  uint32_t field_e,
  const char *operation,
  brew_typed_reg (*sim_op)(brew_typed_reg, brew_typed_reg),
  const char *operation_prefix,
  brew_insn_classes insn_class
) {
  bool is_immedate = FIELD_A == 0xf || FIELD_B == 0xf;
  bool is_short = FIELD_B == 0xf;

  sim_state->insn_class = insn_class;

  if (operation_prefix == NULL)
    operation_prefix = "";

  if (FIELD_A == 0xf && FIELD_B == 0xf)
    {
      print_unknown_insn(fpr, strm_or_buffer, insn_code, field_e, brew_insn_len(insn_code));
      SIM(OPCODES_ASSERT(false));
      return false;
    }
  if (is_immedate)
    {
      char *prefix = is_short ? "short " : "";
      char immed_str[255];
      int reg_idx = is_short ? FIELD_A : FIELD_B;

      sprintf(immed_str, "%d (0x%x)", field_e, field_e);

      if (fpr)
        fpr(strm_or_buffer, "%s <- %s%s%s %s %s", REG_D, prefix, operation_prefix, immed_str, operation, REG(reg_idx));
      SIM(SIM_REGD_T = sim_op((brew_typed_reg){field_e, SIM_REG(reg_idx).type}, SIM_REG(reg_idx)));
    }
  else
    {
      if (fpr)
        fpr(strm_or_buffer, "%s <- %s%s %s %s", REG_D, operation_prefix, REG_A, operation, REG_B);
      SIM(SIM_REGD_T = sim_op(SIM_REG(FIELD_A), SIM_REG(FIELD_B)));
    }
  return true;
}

#define REGULAR_ALU_PATTERN(base, op, sim_op, prefix, insn_class)                                                                                    \
  case 0x##base: if (!binary_op(sim_state, fpr, strm_or_buffer, insn_code, field_e, #op, sim_op, prefix, CLASS_NAME(insn_class))) break; return;     \

static INLINE brew_typed_reg lane_swizzle_op(brew_typed_reg val, uint8_t swizzle_code)
{
  uint8_t lanes_in[4];
  uint8_t lanes_out[4];

  lanes_in[0] = (val.val >>  0) & 0xff;
  lanes_in[1] = (val.val >>  8) & 0xff;
  lanes_in[2] = (val.val >> 16) & 0xff;
  lanes_in[3] = (val.val >> 24) & 0xff;
  for(int i=0;i<4;++i)
    {
      lanes_out[i] = lanes_in[swizzle_code & 3];
      swizzle_code >>= 2;
    }
  return (brew_typed_reg) {
    (lanes_out[0] <<  0) |
    (lanes_out[1] <<  8) |
    (lanes_out[2] << 16) |
    (lanes_out[3] << 24),
    val.type
  };
}

static INLINE brew_typed_reg bsi_op(brew_typed_reg val)
{
  return (brew_typed_reg) {
    ((val.val & 0x80) != 0) ?
      (0xffffff00 | (val.val & 0xff)) :
      (val.val & 0xff),
    val.type
  };
}

static INLINE brew_typed_reg wsi_op(brew_typed_reg val)
{
  return (brew_typed_reg) {
    ((val.val & 0x8000) != 0) ?
      (0xffff0000 | (val.val & 0xffff)) :
      (val.val & 0xffff),
    val.type
  };
}

#define L8_0(x) (((x.val) >>  0) & 0xff)
#define L8_1(x) (((x.val) >>  8) & 0xff)
#define L8_2(x) (((x.val) >> 16) & 0xff)
#define L8_3(x) (((x.val) >> 24) & 0xff)

#define L16_0(x) (((x.val) >>  0) & 0xffff)
#define L16_1(x) (((x.val) >> 16) & 0xffff)

#define L32_0(x) (x.val)

#define FP16_0(x) (as_fp16(L16_0(x)))
#define FP16_1(x) (as_fp16(L16_1(x)))

#define FP32_0(x) (as_fp32(L32_0(x)))

#define USAT8(x) ((int)(x) < 0 ? 0 : (x) > UINT8_MAX ? UINT8_MAX : (x))
#define SSAT8(x) ((int)(x) < INT8_MIN ? INT8_MIN : (x) > INT8_MAX ? INT8_MIN : (x))
#define USAT16(x) ((int)(x) < 0 ? 0 : (x) > UINT16_MAX ? UINT16_MAX : (x))
#define SSAT16(x) ((int)(x) < INT16_MIN ? INT16_MIN : (x) > INT16_MAX ? INT16_MIN : (x))

#define MAKE_INT(l0, type) (brew_typed_reg) { l0, type }
#define MAKE_INT2(l0, l1, type) (brew_typed_reg) { ((((l1) & 0xffff) << 16) | (((l0) & 0xffff) << 0)), type }
#define MAKE_INT4(l0, l1, l2, l3, type) (brew_typed_reg) { ((((l3) & 0xff) << 24) | (((l2) & 0xff) << 16) | (((l1) & 0xff) << 8) | (((l0) & 0xff) << 0)), type }
#define MAKE_FP32(l0, type) MAKE_INT(as_uint32(l0), type)
#define MAKE_FP16X2(l0, l1, type) MAKE_INT2(as_uint16(l0), as_uint16(l1), type)

#define LSH(a, b) ((a) << ((b) & 31))
#define RSH(a, b) ((a) >> ((b) & 31))
#define SRSH(a, b) ((int)(a) >> ((b) & 31))

static INLINE brew_typed_reg xor_op(brew_typed_reg a, brew_typed_reg b) { return (brew_typed_reg) {a.val ^ b.val, a.type}; }
static INLINE brew_typed_reg ior_op(brew_typed_reg a, brew_typed_reg b) { return (brew_typed_reg) {a.val | b.val, a.type}; }
static INLINE brew_typed_reg and_op(brew_typed_reg a, brew_typed_reg b) { return (brew_typed_reg) {a.val & b.val, a.type}; }
static INLINE brew_typed_reg nand_op(brew_typed_reg a, brew_typed_reg b) { return (brew_typed_reg) {~a.val & b.val, a.type}; }
static INLINE brew_typed_reg not_op(brew_typed_reg a) { return (brew_typed_reg) {~a.val, a.type}; }
static INLINE brew_typed_reg add_op(brew_typed_reg a, brew_typed_reg b)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
        return MAKE_INT(L32_0(a) + L32_0(b), a.type);
      case BREW_REG_TYPE_INT16x2:
        return MAKE_INT2(L16_0(a) + L16_0(b), L16_1(a) + L16_1(b), a.type);
      case BREW_REG_TYPE_INT8x4:
        return MAKE_INT4(L8_0(a) + L8_0(b), L8_1(a) + L8_1(b), L8_2(a) + L8_2(b), L8_3(a) + L8_3(b), a.type);
      case BREW_REG_TYPE_UINT16x2S:
        return MAKE_INT2(USAT16(L16_0(a) + L16_0(b)), USAT16(L16_1(a) + L16_1(b)), a.type);
      case BREW_REG_TYPE_SINT16x2S:
        return MAKE_INT2(SSAT16(L16_0(a) + L16_0(b)), SSAT16(L16_1(a) + L16_1(b)), a.type);
      case BREW_REG_TYPE_UINT8x4S:
        return MAKE_INT4(USAT8(L8_0(a) + L8_0(b)), USAT8(L8_1(a) + L8_1(b)), USAT8(L8_2(a) + L8_2(b)), USAT8(L8_3(a) + L8_3(b)), a.type);
      case BREW_REG_TYPE_SINT8x4S:
        return MAKE_INT4(SSAT8(L8_0(a) + L8_0(b)), SSAT8(L8_1(a) + L8_1(b)), SSAT8(L8_2(a) + L8_2(b)), SSAT8(L8_3(a) + L8_3(b)), a.type);
      case BREW_REG_TYPE_FP32:
        return MAKE_FP32(FP32_0(a) + FP32_0(b), a.type);
      case BREW_REG_TYPE_FP16x2:
        return MAKE_FP16X2(FP16_0(a) + FP16_0(b), FP16_1(a) + FP16_1(b), a.type);
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg sub_op(brew_typed_reg a, brew_typed_reg b)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
        return MAKE_INT(L32_0(a) - L32_0(b), a.type);
      case BREW_REG_TYPE_INT16x2:
        return MAKE_INT2(L16_0(a) - L16_0(b), L16_1(a) - L16_1(b), a.type);
      case BREW_REG_TYPE_INT8x4:
        return MAKE_INT4(L8_0(a) - L8_0(b), L8_1(a) - L8_1(b), L8_2(a) - L8_2(b), L8_3(a) - L8_3(b), a.type);
      case BREW_REG_TYPE_UINT16x2S:
        return MAKE_INT2(USAT16(L16_0(a) - L16_0(b)), USAT16(L16_1(a) - L16_1(b)), a.type);
      case BREW_REG_TYPE_SINT16x2S:
        return MAKE_INT2(SSAT16(L16_0(a) - L16_0(b)), SSAT16(L16_1(a) - L16_1(b)), a.type);
      case BREW_REG_TYPE_UINT8x4S:
        return MAKE_INT4(USAT8(L8_0(a) - L8_0(b)), USAT8(L8_1(a) - L8_1(b)), USAT8(L8_2(a) - L8_2(b)), USAT8(L8_3(a) - L8_3(b)), a.type);
      case BREW_REG_TYPE_SINT8x4S:
        return MAKE_INT4(SSAT8(L8_0(a) - L8_0(b)), SSAT8(L8_1(a) - L8_1(b)), SSAT8(L8_2(a) - L8_2(b)), SSAT8(L8_3(a) - L8_3(b)), a.type);
      case BREW_REG_TYPE_FP32:
        return MAKE_FP32(FP32_0(a) - FP32_0(b), a.type);
      case BREW_REG_TYPE_FP16x2:
        return MAKE_FP16X2(FP16_0(a) - FP16_0(b), FP16_1(a) - FP16_1(b), a.type);
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg mul_op(brew_typed_reg a, brew_typed_reg b)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
        return MAKE_INT(L32_0(a) * L32_0(b), a.type);
      case BREW_REG_TYPE_INT16x2:
        return MAKE_INT2(L16_0(a) * L16_0(b), L16_1(a) * L16_1(b), a.type);
      case BREW_REG_TYPE_INT8x4:
        return MAKE_INT4(L8_0(a) * L8_0(b), L8_1(a) * L8_1(b), L8_2(a) * L8_2(b), L8_3(a) * L8_3(b), a.type);
      case BREW_REG_TYPE_UINT16x2S:
        return MAKE_INT2(USAT16(L16_0(a) * L16_0(b)), USAT16(L16_1(a) * L16_1(b)), a.type);
      case BREW_REG_TYPE_SINT16x2S:
        return MAKE_INT2(SSAT16(L16_0(a) * L16_0(b)), SSAT16(L16_1(a) * L16_1(b)), a.type);
      case BREW_REG_TYPE_UINT8x4S:
        return MAKE_INT4(USAT8(L8_0(a) * L8_0(b)), USAT8(L8_1(a) * L8_1(b)), USAT8(L8_2(a) * L8_2(b)), USAT8(L8_3(a) * L8_3(b)), a.type);
      case BREW_REG_TYPE_SINT8x4S:
        return MAKE_INT4(SSAT8(L8_0(a) * L8_0(b)), SSAT8(L8_1(a) * L8_1(b)), SSAT8(L8_2(a) * L8_2(b)), SSAT8(L8_3(a) * L8_3(b)), a.type);
      case BREW_REG_TYPE_FP32:
        return MAKE_FP32(FP32_0(a) * FP32_0(b), a.type);
      case BREW_REG_TYPE_FP16x2:
        return MAKE_FP16X2(FP16_0(a) * FP16_0(b), FP16_1(a) * FP16_1(b), a.type);
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg sfullmul_op(brew_typed_reg a, brew_typed_reg b, uint8_t shift)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
        return MAKE_INT(SRSH(L32_0(a) * L32_0(b), shift), a.type);
      case BREW_REG_TYPE_INT16x2:
        return MAKE_INT2(SRSH(L16_0(a) * L16_0(b), shift), SRSH(L16_1(a) * L16_1(b), shift), a.type);
      case BREW_REG_TYPE_INT8x4:
        return MAKE_INT4(SRSH(L8_0(a) * L8_0(b), shift), SRSH(L8_1(a) * L8_1(b), shift), SRSH(L8_2(a) * L8_2(b), shift), SRSH(L8_3(a) * L8_3(b), shift), a.type);
      case BREW_REG_TYPE_UINT16x2S:
        return MAKE_INT2(USAT16(SRSH(L16_0(a) * L16_0(b), shift)), USAT16(SRSH(L16_1(a) * L16_1(b), shift)), a.type);
      case BREW_REG_TYPE_SINT16x2S:
        return MAKE_INT2(SSAT16(SRSH(L16_0(a) * L16_0(b), shift)), SSAT16(SRSH(L16_1(a) * L16_1(b), shift)), a.type);
      case BREW_REG_TYPE_UINT8x4S:
        return MAKE_INT4(USAT8(SRSH(L8_0(a) * L8_0(b), shift)), USAT8(SRSH(L8_1(a) * L8_1(b), shift)), USAT8(SRSH(L8_2(a) * L8_2(b), shift)), USAT8(SRSH(L8_3(a) * L8_3(b), shift)), a.type);
      case BREW_REG_TYPE_SINT8x4S:
        return MAKE_INT4(SSAT8(SRSH(L8_0(a) * L8_0(b), shift)), SSAT8(SRSH(L8_1(a) * L8_1(b), shift)), SSAT8(SRSH(L8_2(a) * L8_2(b), shift)), SSAT8(SRSH(L8_3(a) * L8_3(b), shift)), a.type);
      case BREW_REG_TYPE_FP32:
        return MAKE_FP32(FP32_0(a) * FP32_0(b), a.type);
      case BREW_REG_TYPE_FP16x2:
        return MAKE_FP16X2(FP16_0(a) * FP16_0(b), FP16_1(a) * FP16_1(b), a.type);
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg fullmul_op(brew_typed_reg a, brew_typed_reg b, uint8_t shift)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
        return MAKE_INT(RSH(L32_0(a) * L32_0(b), shift), a.type);
      case BREW_REG_TYPE_INT16x2:
        return MAKE_INT2(RSH(L16_0(a) * L16_0(b), shift), RSH(L16_1(a) * L16_1(b), shift), a.type);
      case BREW_REG_TYPE_INT8x4:
        return MAKE_INT4(RSH(L8_0(a) * L8_0(b), shift), RSH(L8_1(a) * L8_1(b), shift), RSH(L8_2(a) * L8_2(b), shift), RSH(L8_3(a) * L8_3(b), shift), a.type);
      case BREW_REG_TYPE_UINT16x2S:
        return MAKE_INT2(USAT16(RSH(L16_0(a) * L16_0(b), shift)), USAT16(RSH(L16_1(a) * L16_1(b), shift)), a.type);
      case BREW_REG_TYPE_SINT16x2S:
        return MAKE_INT2(SSAT16(RSH(L16_0(a) * L16_0(b), shift)), SSAT16(RSH(L16_1(a) * L16_1(b), shift)), a.type);
      case BREW_REG_TYPE_UINT8x4S:
        return MAKE_INT4(USAT8(RSH(L8_0(a) * L8_0(b), shift)), USAT8(RSH(L8_1(a) * L8_1(b), shift)), USAT8(RSH(L8_2(a) * L8_2(b), shift)), USAT8(RSH(L8_3(a) * L8_3(b), shift)), a.type);
      case BREW_REG_TYPE_SINT8x4S:
        return MAKE_INT4(SSAT8(RSH(L8_0(a) * L8_0(b), shift)), SSAT8(RSH(L8_1(a) * L8_1(b), shift)), SSAT8(RSH(L8_2(a) * L8_2(b), shift)), SSAT8(RSH(L8_3(a) * L8_3(b), shift)), a.type);
      case BREW_REG_TYPE_FP32:
        return MAKE_FP32(FP32_0(a) * FP32_0(b), a.type);
      case BREW_REG_TYPE_FP16x2:
        return MAKE_FP16X2(FP16_0(a) * FP16_0(b), FP16_1(a) * FP16_1(b), a.type);
      default:
        OPCODES_ASSERT(false);
    }
}

static INLINE brew_typed_reg lsh_op(brew_typed_reg a, brew_typed_reg b)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
      case BREW_REG_TYPE_FP32:
        return MAKE_INT(LSH(L32_0(a), L32_0(b)), a.type);
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
      case BREW_REG_TYPE_FP16x2:
        return MAKE_INT2(LSH(L16_0(a), L16_0(b)), LSH(L16_1(a), L16_1(b)), a.type);
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
        return MAKE_INT4(LSH(L8_0(a), L8_0(b)), LSH(L8_1(a), L8_1(b)), LSH(L8_2(a), L8_2(b)), LSH(L8_3(a), L8_3(b)), a.type);
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg rsh_op(brew_typed_reg a, brew_typed_reg b)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
      case BREW_REG_TYPE_FP32:
        return MAKE_INT(RSH(L32_0(a), L32_0(b)), a.type);
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
      case BREW_REG_TYPE_FP16x2:
        return MAKE_INT2(RSH(L16_0(a), L16_0(b)), RSH(L16_1(a), L16_1(b)), a.type);
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
        return MAKE_INT4(RSH(L8_0(a), L8_0(b)), RSH(L8_1(a), L8_1(b)), RSH(L8_2(a), L8_2(b)), RSH(L8_3(a), L8_3(b)), a.type);
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg srsh_op(brew_typed_reg a, brew_typed_reg b)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
      case BREW_REG_TYPE_FP32:
        return MAKE_INT(SRSH(L32_0(a), L32_0(b)), a.type);
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
      case BREW_REG_TYPE_FP16x2:
        return MAKE_INT2(SRSH(L16_0(a), L16_0(b)), SRSH(L16_1(a), L16_1(b)), a.type);
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
        return MAKE_INT4(SRSH(L8_0(a), L8_0(b)), SRSH(L8_1(a), L8_1(b)), SRSH(L8_2(a), L8_2(b)), SRSH(L8_3(a), L8_3(b)), a.type);
      default:
        OPCODES_ASSERT(false);
    }
}

static INLINE brew_typed_reg neg_op(brew_typed_reg a)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
        return MAKE_INT(-L32_0(a), a.type);
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
        return MAKE_INT2(-L16_0(a), -L16_1(a), a.type);
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
        return MAKE_INT4(-L8_0(a), -L8_1(a), -L8_2(a), -L8_3(a), a.type);
      case BREW_REG_TYPE_FP32:
        return MAKE_FP32(-FP32_0(a), a.type);
      case BREW_REG_TYPE_FP16x2:
        return MAKE_FP16X2(-FP16_0(a), -FP16_1(a), a.type);
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg to_float_op(brew_typed_reg a)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
      case BREW_REG_TYPE_FP32:
        return MAKE_FP32((int32_t)L32_0(a), a.type);
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
      case BREW_REG_TYPE_FP16x2:
        return MAKE_FP16X2((int16_t)L16_0(a), (int16_t)L16_1(a), a.type);
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg to_int_op(brew_typed_reg a)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
      case BREW_REG_TYPE_FP32:
        return MAKE_INT((int32_t)(FP32_0(a)+0.5), a.type);
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
      case BREW_REG_TYPE_FP16x2:
        return MAKE_INT2((int16_t)(FP16_0(a)+0.5), (int16_t)(FP16_1(a)+0.5), a.type);
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg reciprocal_op(brew_typed_reg a)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
      case BREW_REG_TYPE_FP32:
        return MAKE_FP32(1.0 / FP32_0(a), a.type);
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
      case BREW_REG_TYPE_FP16x2:
        return MAKE_FP16X2(1.0 / FP16_0(a), 1.0 / FP16_1(a), a.type);
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg rsqrt_op(brew_typed_reg a, const brew_sim_state *sim_state)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
      case BREW_REG_TYPE_FP32:
        return MAKE_FP32(sim_state->rsqrt(FP32_0(a)), a.type);
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
      case BREW_REG_TYPE_FP16x2:
        return MAKE_FP16X2(sim_state->rsqrt_fp16(FP16_0(a)), sim_state->rsqrt_fp16(FP16_1(a)), a.type);
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg sum_op(brew_typed_reg a)
{
  switch (a.type)
    {
      case BREW_REG_TYPE_INT32:
      case BREW_REG_TYPE_FP32:
        return MAKE_INT(L32_0(a), a.type);
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
        return MAKE_INT(L16_0(a) + L16_1(a), BREW_REG_TYPE_INT32);
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
        return MAKE_INT(L8_0(a) + L8_1(a) + L8_2(a) + L8_3(a), BREW_REG_TYPE_INT32);
      case BREW_REG_TYPE_FP16x2:
        return MAKE_FP32(FP16_0(a) + FP16_1(a), BREW_REG_TYPE_FP32);
      default:
        OPCODES_ASSERT(false);
    }
}
static INLINE brew_typed_reg interpolate_op(brew_typed_reg a ATTRIBUTE_UNUSED, brew_typed_reg b ATTRIBUTE_UNUSED)
{
  OPCODES_ASSERT(false);
}

static INLINE bool any(bool a, bool b, bool c, bool d) { return a || b || c || d; }
static INLINE bool all(bool a, bool b, bool c, bool d) { return a && b && c && d; }
#define ANY_OR_ALL1(a) (a)
#define ANY_OR_ALL2(a,b) (all_not_any ? all(a,b,true,true) : any(a,b,false,false))
#define ANY_OR_ALL4(a,b,c,d) (all_not_any ? all(a,b,c,d) : any(a,b,c,d))
#define CMP(a,b) (      \
  cond == COND_EQ ? (a) == (b) : \
  cond == COND_NE ? (a) != (b) : \
  cond == COND_GT ? (a) >  (b) : \
  cond == COND_GE ? (a) >= (b) : \
  cond == COND_LT ? (a) <  (b) : \
  cond == COND_LE ? (a) <= (b) : \
  opcodes_assert(__FILE__, __LINE__), false   \
)

static INLINE bool cond(brew_typed_reg left, brew_cond cond, brew_typed_reg right, bool is_signed, bool all_not_any)
{
  switch (right.type)
    {
      case BREW_REG_TYPE_INT32:
        if (is_signed)
          return ANY_OR_ALL1(CMP((int32_t)L32_0(left), (int32_t)L32_0(right)));
        else
          return ANY_OR_ALL1(CMP(FP32_0(left), FP32_0(right)));
      case BREW_REG_TYPE_FP32:
        return ANY_OR_ALL1(CMP(L32_0(left), L32_0(right)));
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
        if (is_signed)
          return ANY_OR_ALL2(
            CMP((int16_t)L16_0(left), (int16_t)L16_0(right)),
            CMP((int16_t)L16_1(left), (int16_t)L16_1(right))
          );
        else
          return ANY_OR_ALL2(
            CMP(L16_0(left), L16_0(right)),
            CMP(L16_1(left), L16_1(right))
          );
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
        if (is_signed)
          return ANY_OR_ALL4(
            CMP((int8_t)L8_0(left), (int8_t)L8_0(right)),
            CMP((int8_t)L8_1(left), (int8_t)L8_1(right)),
            CMP((int8_t)L8_2(left), (int8_t)L8_2(right)),
            CMP((int8_t)L8_3(left), (int8_t)L8_3(right))
          );
        else
          return ANY_OR_ALL4(
            CMP(L8_0(left), L8_0(right)),
            CMP(L8_1(left), L8_1(right)),
            CMP(L8_2(left), L8_2(right)),
            CMP(L8_3(left), L8_3(right))
          );
      case BREW_REG_TYPE_FP16x2:
        return ANY_OR_ALL2(
          CMP(FP16_0(left), FP16_0(right)),
          CMP(FP16_1(left), FP16_1(right))
        );
      default:
        OPCODES_ASSERT(false);
    }
}

#define B32(a) (brew_typed_reg) {((a) ? 0xffffffff : 0x00000000), BREW_REG_TYPE_INT32 }
#define B16x2(a,b) (brew_typed_reg) {(((b) ? 0xffff0000 : 0x00000000) | ((a) ? 0x0000ffff : 0x00000000)), BREW_REG_TYPE_INT16x2 }
#define B8x4(a,b,c,d) (brew_typed_reg) {(((d) ? 0xff000000 : 0x00000000) | ((c) ? 0x00ff0000 : 0x00000000) | ((b) ? 0x0000ff00 : 0x00000000) | ((a) ? 0x000000ff : 0x00000000)), BREW_REG_TYPE_INT8x4 }
static INLINE brew_typed_reg lane_cond(brew_typed_reg left, brew_cond cond, brew_typed_reg right, bool is_signed)
{
  switch (right.type)
    {
      case BREW_REG_TYPE_INT32:
        if (is_signed)
          return B32(CMP((int32_t)L32_0(left), (int32_t)L32_0(right)));
        else
          return B32(CMP(FP32_0(left), FP32_0(right)));
      case BREW_REG_TYPE_FP32:
        return B32(CMP(L32_0(left), L32_0(right)));
      case BREW_REG_TYPE_INT16x2:
      case BREW_REG_TYPE_UINT16x2S:
      case BREW_REG_TYPE_SINT16x2S:
        if (is_signed)
          return B16x2(
            CMP((int16_t)L16_0(left), (int16_t)L16_0(right)),
            CMP((int16_t)L16_1(left), (int16_t)L16_1(right))
          );
        else
          return B16x2(
            CMP(L16_0(left), L16_0(right)),
            CMP(L16_1(left), L16_1(right))
          );
      case BREW_REG_TYPE_INT8x4:
      case BREW_REG_TYPE_UINT8x4S:
      case BREW_REG_TYPE_SINT8x4S:
        if (is_signed)
          return B8x4(
            CMP((int8_t)L8_0(left), (int8_t)L8_0(right)),
            CMP((int8_t)L8_1(left), (int8_t)L8_1(right)),
            CMP((int8_t)L8_2(left), (int8_t)L8_2(right)),
            CMP((int8_t)L8_3(left), (int8_t)L8_3(right))
          );
        else
          return B8x4(
            CMP(L8_0(left), L8_0(right)),
            CMP(L8_1(left), L8_1(right)),
            CMP(L8_2(left), L8_2(right)),
            CMP(L8_3(left), L8_3(right))
          );
      case BREW_REG_TYPE_FP16x2:
        return B16x2(
          CMP(FP16_0(left), FP16_0(right)),
          CMP(FP16_1(left), FP16_1(right))
        );
      default:
        OPCODES_ASSERT(false);
    }
}

static INLINE int get_bit(uint32_t word, int idx) { return (word >> idx) & 1; }

static INLINE int decode_ones_complement(uint32_t data, size_t bit_length)
{
  if (bit_length < 32)
    {
      if (((data >> (bit_length - 1)) & 1) != 0)
        {
          // need to set all upper bits to 1
          data |= (((1ul << (32-bit_length)) - 1) << bit_length);
        }
    }
  int32_t signed_data = data;
  return signed_data < 0 ? signed_data + 1 : signed_data;
}

void
brew_sim_insn(void *context ATTRIBUTE_UNUSED, brew_sim_state *sim_state, uint16_t insn_code, uint32_t field_e)
{
  int length;

  char str_buffer[255];

  fprintf_ftype fpr = sim_state->tracer;
  void *strm_or_buffer = sim_state->tracer_strm;

  length = brew_insn_len(insn_code);
  if (length == 4)
    {
      field_e = (int32_t)field_e << 16 >> 16; // Sign-extend to 32-bits
    }
  if (FIELD_D != 0xf && FIELD_C < 0xc)
    {
      // Some rather special-case pseudo-ops:
      // TODO: fix these...
      //if (FIELD_C == 2 && FIELD_B == FIELD_A && FIELD_D == FIELD_A) { CLASS(NOP); INST("NOP"); } // $rD <- $rD | $rD
      //if (FIELD_C == 1 && FIELD_B == FIELD_A) { CLASS(LOGIC); SIM(SIM_REGD_T = 0;) INST("%s <- 0", REG_D); } // $rD <- $rX ^ $rX
      //if (FIELD_C == 2 && FIELD_B == FIELD_A) { CLASS(LOGIC); SIM(SIM_REGD_T = SIM_REG(FIELD_B)); INST("%s <- %s", REG_D, REG_B); } // $rD <- $rX | $rX

      // ALU group
      switch (FIELD_C)
        {
        case 0x0: // Unary and special
          if ((insn_code & 0x0fff) == 0x000)
            {
              // Exception group
              if (pattern_match(insn_code, "0000")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_FILL); INST("fill"); }
              if (pattern_match(insn_code, "1000")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_BREAK); INST("break"); }
              if (pattern_match(insn_code, "2000")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_SYSCALL); INST("syscall"); }
              if (pattern_match(insn_code, "3000")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_SWI3); INST("swi 3"); }
              if (pattern_match(insn_code, "4000")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_SWI4); INST("swi 4"); }
              if (pattern_match(insn_code, "5000")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_SWI5); INST("swi 5"); }
              if (pattern_match(insn_code, "6000")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_SII); INST("sii"); }
              if (pattern_match(insn_code, "7000")) { CLASS(EXCEPTION); SIM(sim_state->insn_exception =  BREW_EXCEPTION_HWI); INST("hwi"); }

              if (pattern_match(insn_code, "8000")) { CLASS(BRANCH); SIM(sim_state->nis_task_mode = true); INST("stm"); }
              if (pattern_match(insn_code, "9000")) { CLASS(POWER); INST("woi"); }
              UNKNOWN;
            }
          if ((insn_code & 0x0fff) == 0x001)
            {
              // Atomic group
              char before[3];
              char after[3];
              before[0] = 0;
              after[0] = 0;
              if ((insn_code & (1 << 12)) == 0) strcat(before, "r");
              if ((insn_code & (1 << 13)) == 0) strcat(before, "w");
              if ((insn_code & (1 << 14)) == 0) strcat(after, "r");
              if ((insn_code & (1 << 15)) == 0) strcat(after, "w");
              CLASS(ATOMIC); INST("fence %s, %s", before, after);
              UNKNOWN;
            }
          if ((insn_code & 0x0ff0) == 0x000)
            {
              // Branch group
              if (pattern_match(insn_code, ".002")) { CLASS(BRANCH); SIM(SIM_PC_T = SIM_REG(FIELD_D).val); INST("$pc <- %s", REG_D); }
              if (pattern_match(insn_code, ".003")) { CLASS(MOV); SIM(SIM_TPC_T = SIM_REG(FIELD_D).val); INST("$tpc <- %s", REG_D); }
              if (pattern_match(insn_code, ".004")) { CLASS(MOV); SIM(SIM_REGD_T = MAKE_INT(SIM_PC, BREW_REG_TYPE_INT32)); INST("%s <- $pc", REG_D); }
              if (pattern_match(insn_code, ".005")) { CLASS(MOV); SIM(SIM_REGD_T = MAKE_INT(SIM_TPC, BREW_REG_TYPE_INT32)); INST("%s <- $tpc", REG_D); }
              // Load immediate
              if (pattern_match(insn_code, ".00f")) { CLASS(IMM); SIM(SIM_REGD_T = MAKE_INT(field_e, SIM_REGD_TYPE)); INST("%s <- %u (0x%x)", REG_D, field_e, field_e); }
              UNKNOWN;
            }

          // Unary group
          if (pattern_match(insn_code, ".01.")) { CLASS(ARITH); SIM(SIM_REGD_T = MAKE_INT(decode_ones_complement(FIELD_A, 4), SIM_REGD_TYPE)); INST("%s <- tiny %d", REG_D, decode_ones_complement(FIELD_A, 4)); }
          if (pattern_match(insn_code, ".02.")) { CLASS(LINK); SIM(SIM_REGD_T = MAKE_INT(SIM_PC + sim_link_offset(FIELD_A), BREW_REG_TYPE_INT32)); INST("%s <- $pc + %s", REG_D, format_link_offset(FIELD_A, str_buffer)); }
          if (pattern_match(insn_code, ".03.")) { CLASS(ARITH); SIM(SIM_REGD_T = neg_op(SIM_REG(FIELD_A))); INST("%s <- -%s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".04.")) { CLASS(BIT); SIM(SIM_REGD_T = not_op(SIM_REG(FIELD_A))); INST("%s <- ~%s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".05.")) { CLASS(BIT); SIM(SIM_REGD_T = bsi_op(SIM_REG(FIELD_A))); INST("%s <- bsi %s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".06.")) { CLASS(BIT); SIM(SIM_REGD_T = wsi_op(SIM_REG(FIELD_A))); INST("%s <- wsi %s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".07.")) { CLASS(FP); SIM(SIM_REGD_T = to_float_op(SIM_REG(FIELD_A))); INST("%s <- float %s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".08.")) { CLASS(FP); SIM(SIM_REGD_T = to_int_op(SIM_REG(FIELD_A))); INST("%s <- int %s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".09.")) { CLASS(FP); SIM(SIM_REGD_T = reciprocal_op(SIM_REG(FIELD_A))); INST("%s <- 1 / %s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".0a.")) { CLASS(FP); SIM(SIM_REGD_T = rsqrt_op(SIM_REG(FIELD_A), sim_state)); INST("%s <- rsqrt %s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".0b.")) { CLASS(ARITH); SIM(SIM_REGD_T = sum_op(SIM_REG(FIELD_A))); INST("%s <- sum %s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".0c.")) { CLASS(TYPE); SIM(SIM_REGD_T = MAKE_INT(L32_0(SIM_REG(FIELD_D)), SIM_REG(FIELD_A).val & 0xf)); INST("type %s <- %s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".0d.")) { CLASS(TYPE); SIM(SIM_REGD_T = MAKE_INT(SIM_REG(FIELD_A).type, BREW_REG_TYPE_INT32)); INST("%s <- type %s", REG_D, REG_A); }
          if (pattern_match(insn_code, ".0e.")) { CLASS(TYPE); SIM(SIM_REGD_T = MAKE_INT(L32_0(SIM_REG(FIELD_D)), FIELD_A)); INST("type %s <- %s", REG_D, REG_A); }

          // immediate branch
          if (pattern_match(insn_code, "20ef")) { CLASS(BRANCH); SIM(SIM_PC_T = field_e); INST("$pc <- %u (0x%x)", field_e, field_e); }
          if (pattern_match(insn_code, "30ef")) { CLASS(BRANCH); SIM(SIM_TPC_T = field_e); INST("$tpc <- %u (0x%x)", field_e, field_e); }

          // load short immediate
          if (pattern_match(insn_code, ".0f0")) { CLASS(IMM); SIM(SIM_REGD_T = MAKE_INT(field_e, SIM_REGD_TYPE)); INST("%s <- short %u (0x%x)", REG_D, field_e, field_e); }

          // short immediate branch
          if (pattern_match(insn_code, "20fe")) { CLASS(BRANCH); SIM(SIM_PC_T = field_e); INST("$pc <- short %u (0x%x)", field_e, field_e); }
          if (pattern_match(insn_code, "30fe")) { CLASS(BRANCH); SIM(SIM_TPC_T = field_e); INST("$tpc <- short %u (0x%x)", field_e, field_e); }

          UNKNOWN;
          break;
        REGULAR_ALU_PATTERN(1, ^, xor_op, "", LOGIC);
        REGULAR_ALU_PATTERN(2, |, ior_op, "", LOGIC);
        REGULAR_ALU_PATTERN(3, &, and_op, "", LOGIC);
        REGULAR_ALU_PATTERN(4, +, add_op, "", ARITH);
        REGULAR_ALU_PATTERN(5, -, sub_op, "", ARITH);
        REGULAR_ALU_PATTERN(6, <<, lsh_op, "", SHIFT);
        REGULAR_ALU_PATTERN(7, >>, rsh_op, "", SHIFT);
        REGULAR_ALU_PATTERN(8, >>>, srsh_op, "", SHIFT);
        REGULAR_ALU_PATTERN(9, *, mul_op, "", MUL);
        case 0xa:
          if (pattern_match(insn_code, ".a..")) { CLASS(BIT); SIM(SIM_REGD_T = nand_op(SIM_REG(FIELD_A), SIM_REG(FIELD_B))); INST("%s <- ~%s & %s", REG_D, REG_A, REG_B); }
          if (pattern_match(insn_code, ".a.f")) {
            SIM(CLASS(TYPE));
            char types[1024];
            types[0] = 0;
            for(int i=0;i<8;++i)
              {
                int type = (field_e >> i*4) & 0xf;
                SIM(SIM_REG_T(i) = MAKE_INT(SIM_REG(i).val, type));
                if (fpr)
                  {
                    if (types[0] != 0)
                      strncat(types, ", ", ARRAY_SIZE(types)-1);
                    strncat(types, brew_reg_type_name(type), ARRAY_SIZE(types)-1);
                  }
              }
            INST("type $r0...$r7 <- %s", types);
          }
          if (pattern_match(insn_code, ".af.")) {
            SIM(CLASS(VECTOR));
            SIM(SIM_REGD_T = lane_swizzle_op(SIM_REG(FIELD_A), field_e));
            INST("%s <- lane_swizzle %s %d%d%d%d", REG_D, REG_A,
              (field_e >> 6) & 3,
              (field_e >> 4) & 3,
              (field_e >> 2) & 3,
              (field_e >> 0) & 3
            );
          }
          UNKNOWN;
          break;
        case 0xb:
          if (pattern_match(insn_code, ".b..")) {
            SIM(CLASS(ARITH));
            SIM(SIM_REGD_T = add_op(MAKE_INT(decode_ones_complement(FIELD_A, 4), SIM_REG_TYPE(FIELD_B)), SIM_REG(FIELD_B)));
            INST("%s <- tiny %s + %d", REG_D, REG_B, decode_ones_complement(FIELD_A, 4));
          }
          if (pattern_match(insn_code, ".b.f")) {
            SIM(CLASS(TYPE));
            char types[1024];
            types[0] = 0;
            for(int i=8;i<15;++i)
              {
                int type = (field_e >> i*4) & 0xf;
                SIM(SIM_REG_T(i) = MAKE_INT(SIM_REG(i).val, type));
                if (fpr)
                  {
                    if (types[0] != 0)
                      strncat(types, ", ", ARRAY_SIZE(types)-1);
                    strncat(types, brew_reg_type_name(type), ARRAY_SIZE(types)-1);
                  }
              }
            INST("type $r8...$r14 <- %s", types);
          }
          UNKNOWN;
          break;
        }
    }
  else if (FIELD_D != 0xf && FIELD_C == 0xc)
    {
      // tiny store
      int offset = decode_ones_complement(insn_code & 0xfe, 8) << 1;
      int base_reg = BREW_REG_FP | (FIELD_A & 1);
      SIM(
        CLASS(ST);
        sim_mem_store(
          context,
          sim_state,
          32,
          base_reg,
          offset,
          true,
          SIM_REG(FIELD_D).val
        );
      );
      INST("%s <- %s", format_mem_ref("mem32", base_reg, offset, true, true, str_buffer), REG_D);
    }
  else if (FIELD_D != 0xf && FIELD_C == 0xd)
    {
      // tiny load
      int offset = decode_ones_complement(insn_code & 0xfe, 8) << 1;
      int base_reg = BREW_REG_FP | (FIELD_A & 1);
      SIM(
        CLASS(LD);
        SIM_REGD_T = MAKE_INT(sim_mem_load(
          context,
          sim_state,
          32,
          false,
          base_reg,
          offset,
          true
        ), SIM_REGD_TYPE)
      );
      INST("%s <- %s", REG_D,  format_mem_ref("mem32", base_reg, offset, true, true, str_buffer));
    }
  else if (FIELD_D != 0xf && (FIELD_C == 0xe || FIELD_C == 0xf))
    {
      if (FIELD_B < 0x4)
        {
          // bulk type load-store
          int offset = decode_ones_complement(FIELD_A, 4) * 4;
          int reg_ofs = (FIELD_B & 1) == 0 ? 0 : 8;
          int reg_cnt = (FIELD_B & 1) == 0 ? 8 : 7;
          uint16_t mask = FIELD_C == 0xf ? 0xffff : field_e;
          char regs[1024];
          regs[0] = 0;
          if (FIELD_B < 2)
            {
              uint32_t types = 0;
              SIM(
                CLASS(TYPE);
                types = sim_mem_load(context, sim_state, 32, false, FIELD_D, offset, true);
              );
              for(int i=0;i<reg_cnt;++i)
                {
                  if (((mask >> i) & 1) != 0)
                    {
                      SIM(
                        int type = (types >> i*4) & 0xf;
                        SIM_REG_T(i+reg_ofs) = MAKE_INT(SIM_REG(i+reg_ofs).val, type);
                      );
                      strncat(regs, brew_reg_names[i+reg_ofs], ARRAY_SIZE(regs)-1);
                      if (regs[0] != 0)
                        strncat(regs, ", ", ARRAY_SIZE(regs)-1);
                    }
                }
              INST("type %s <- %s", regs, format_mem_ref("mem32", FIELD_D, offset, true, false, str_buffer));
            }
          else
            {
              uint32_t types = 0;
              if (FIELD_C == 0xf)
                UNKNOWN;
              SIM(
                CLASS(TYPE);
              );
              SIM(
                for(int i=0;i<reg_cnt;++i)
                  {
                      types |= (SIM_REG(i+reg_ofs).type & 0xf) << (i*4);
                  }
                sim_mem_store(context, sim_state, 32, FIELD_B, offset, true, types);
              );
              INST("%s <- type $r%d...$r%d", format_mem_ref("mem32", FIELD_D, offset, true, false, str_buffer), reg_ofs, reg_ofs+reg_cnt);
            }
        }
      else
        {
          // load-store group
          switch (FIELD_B)
            {
              case 0x4: SIM( CLASS(LD); SIM_REGD_T = MAKE_INT(sim_mem_load(context, sim_state,  8, false, FIELD_A, field_e, FIELD_C == 0xf), SIM_REGD_TYPE)); INST("%s <- %s", REG_D, format_mem_ref("mem8",   FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer));
              case 0x5: SIM( CLASS(LD); SIM_REGD_T = MAKE_INT(sim_mem_load(context, sim_state, 16, false, FIELD_A, field_e, FIELD_C == 0xf), SIM_REGD_TYPE)); INST("%s <- %s", REG_D, format_mem_ref("mem16",  FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer));
              case 0x6: SIM( CLASS(LD); SIM_REGD_T = MAKE_INT(sim_mem_load(context, sim_state, 32, false, FIELD_A, field_e, FIELD_C == 0xf), SIM_REGD_TYPE)); INST("%s <- %s", REG_D, format_mem_ref("mem32",  FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer));
              case 0x7: SIM( CLASS(LD); SIM_REGD_T = MAKE_INT(sim_mem_load(context, sim_state, 32, false, FIELD_A, field_e, FIELD_C == 0xf), SIM_REGD_TYPE)); INST("%s <- %s", REG_D, format_mem_ref("memll32",  FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer));

              case 0x8: SIM( CLASS(ST); sim_mem_store(context, sim_state,  8, FIELD_A, field_e, FIELD_C == 0xf, SIM_REG(FIELD_D).val)); INST("%s <- %s", format_mem_ref("mem8",  FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer), REG_D);
              case 0x9: SIM( CLASS(ST); sim_mem_store(context, sim_state, 16, FIELD_A, field_e, FIELD_C == 0xf, SIM_REG(FIELD_D).val)); INST("%s <- %s", format_mem_ref("mem16", FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer), REG_D);
              case 0xa: SIM( CLASS(ST); sim_mem_store(context, sim_state, 32, FIELD_A, field_e, FIELD_C == 0xf, SIM_REG(FIELD_D).val)); INST("%s <- %s", format_mem_ref("mem32", FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer), REG_D);
              case 0xb: SIM( CLASS(ST); sim_mem_store(context, sim_state, 32, FIELD_A, field_e, FIELD_C == 0xf, SIM_REG(FIELD_D).val)); INST("%s <- %s", format_mem_ref("memsr32", FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer), REG_D);

              case 0xc: SIM( CLASS(LD); SIM_REGD_T = MAKE_INT(sim_mem_load(context, sim_state,  8, true,  FIELD_A, field_e, FIELD_C == 0xf), SIM_REGD_TYPE)); INST("%s <- %s", REG_D, format_mem_ref("smem8",  FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer));
              case 0xd: SIM( CLASS(LD); SIM_REGD_T = MAKE_INT(sim_mem_load(context, sim_state, 16, true,  FIELD_A, field_e, FIELD_C == 0xf), SIM_REGD_TYPE)); INST("%s <- %s", REG_D, format_mem_ref("smem16", FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer));
              case 0xe: switch (FIELD_D)
                {
                  case 0x1: SIM(CLASS(ATOMIC)); INST("%s", format_mem_ref("inv32", FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer));
                  case 0x2: SIM(CLASS(BRANCH); SIM_PC_T = sim_mem_load(context, sim_state, 32, false, FIELD_A, field_e, FIELD_C == 0xf)); INST("$pc <- %s", format_mem_ref("mem32",  FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer));
                  case 0x3: SIM(CLASS(BRANCH); SIM_TPC_T = sim_mem_load(context, sim_state, 32, false, FIELD_A, field_e, FIELD_C == 0xf)); INST("$tpc <- %s", format_mem_ref("mem32",  FIELD_A, field_e, FIELD_C == 0xf, false, str_buffer));
                  default: UNKNOWN;
                }
            }
          UNKNOWN;
        }
    }
  else if (FIELD_D == 0xf && FIELD_C != 0xf)
    {
      #define ZERO (brew_typed_reg){0, BREW_REG_TYPE_INT32}
      // conditional branch group
      if (pattern_match(insn_code, "f00.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_EQ, SIM_REG(FIELD_A), true, false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s == 0 $pc %s", REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f01.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_NE, SIM_REG(FIELD_A), true, false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s != 0 $pc %s", REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f02.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_GT, SIM_REG(FIELD_A), true, false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s < 0 $pc %s",  REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f03.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_LE, SIM_REG(FIELD_A), true, false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s >= 0 $pc %s", REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f04.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_LT, SIM_REG(FIELD_A), true, false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s > 0 $pc %s",  REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f05.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_GE, SIM_REG(FIELD_A), true, false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s <= 0 $pc %s", REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f06.")) { UNKNOWN; }
      if (pattern_match(insn_code, "f07.")) { UNKNOWN; }
      if (pattern_match(insn_code, "f08.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_EQ, SIM_REG(FIELD_A), true, true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s == 0 $pc %s", REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f09.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_NE, SIM_REG(FIELD_A), true, true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s != 0 $pc %s", REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f0a.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_GT, SIM_REG(FIELD_A), true, true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s < 0 $pc %s",  REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f0b.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_LE, SIM_REG(FIELD_A), true, true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s >= 0 $pc %s", REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f0c.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_LT, SIM_REG(FIELD_A), true, true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s > 0 $pc %s",  REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f0d.")) { CLASS(CBRANCH0); SIM(if (cond(ZERO, COND_GE, SIM_REG(FIELD_A), true, true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s <= 0 $pc %s", REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f0e.")) { UNKNOWN; }

      if (pattern_match(insn_code, "f1..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_EQ, SIM_REG(FIELD_A), false, false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s == %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f2..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_NE, SIM_REG(FIELD_A), false, false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s != %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f3..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_LT, SIM_REG(FIELD_A), true,  false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if signed %s < %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f4..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_GE, SIM_REG(FIELD_A), true,  false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if signed %s >= %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f5..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_LT, SIM_REG(FIELD_A), false, false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s < %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f6..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_GE, SIM_REG(FIELD_A), false, false)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s > %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f7..")) { UNKNOWN; }
      if (pattern_match(insn_code, "f8..")) { UNKNOWN; }
      if (pattern_match(insn_code, "f9..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_EQ, SIM_REG(FIELD_A), false, true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s == %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "fa..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_NE, SIM_REG(FIELD_A), false, true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s != %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "fb..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_LT, SIM_REG(FIELD_A), true,  true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if signed %s < %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "fc..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_GE, SIM_REG(FIELD_A), true,  true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if signed %s >= %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "fd..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_LT, SIM_REG(FIELD_A), false, true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s < %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "fe..")) { CLASS(CBRANCH); SIM(if (cond(SIM_REG(FIELD_B), COND_GE, SIM_REG(FIELD_A), false, true)) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s >= %s $pc %s", REG_B, REG_A, format_branch_target(field_e, str_buffer)); }

      if (pattern_match(insn_code, "f.f.")) { CLASS(CBRANCHBIT); SIM(if (get_bit(SIM_REG(FIELD_A).val, field_c_to_bit_map[FIELD_C]) == 1) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s[%d] == 1 $pc %s", REG_A, field_c_to_bit_map[FIELD_C], format_branch_target(field_e, str_buffer)); }
      if (pattern_match(insn_code, "f..f")) { CLASS(CBRANCHBIT); SIM(if (get_bit(SIM_REG(FIELD_B).val, field_c_to_bit_map[FIELD_C]) == 0) SIM_PC_T = sim_branch_target(field_e, SIM_PC)); INST("if %s[%d] == 0 $pc %s", REG_B, field_c_to_bit_map[FIELD_C], format_branch_target(field_e, str_buffer)); }
    }
  else if (FIELD_D == 0xf && FIELD_C == 0xf && FIELD_B == 0xf)
    {
      // Extension group A
      switch (FIELD_A)
        {
        case 0:
          // conditional branch group
          if (pattern_match(field_e, ".00.")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(ZERO, COND_EQ, SIM_REG(FIELD_A), true)); INST("%s <- %s == 0", REG(EXT_FIELD_D), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".01.")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(ZERO, COND_NE, SIM_REG(FIELD_A), true)); INST("%s <- %s != 0", REG(EXT_FIELD_D), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".02.")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(ZERO, COND_GT, SIM_REG(FIELD_A), true)); INST("%s <- %s < 0",  REG(EXT_FIELD_D), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".03.")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(ZERO, COND_LE, SIM_REG(FIELD_A), true)); INST("%s <- %s >= 0", REG(EXT_FIELD_D), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".04.")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(ZERO, COND_LT, SIM_REG(FIELD_A), true)); INST("%s <- %s > 0",  REG(EXT_FIELD_D), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".05.")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(ZERO, COND_GE, SIM_REG(FIELD_A), true)); INST("%s <- %s <= 0", REG(EXT_FIELD_D), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".06.")) { UNKNOWN; }
          if (pattern_match(field_e, ".07.")) { UNKNOWN; }
          if (pattern_match(field_e, ".08.")) { UNKNOWN; }
          if (pattern_match(field_e, ".09.")) { UNKNOWN; }
          if (pattern_match(field_e, ".0a.")) { UNKNOWN; }
          if (pattern_match(field_e, ".0b.")) { UNKNOWN; }
          if (pattern_match(field_e, ".0c.")) { UNKNOWN; }
          if (pattern_match(field_e, ".0d.")) { UNKNOWN; }
          if (pattern_match(field_e, ".0e.")) { UNKNOWN; }

          if (pattern_match(field_e, ".1..")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(SIM_REG(FIELD_B), COND_EQ, SIM_REG(FIELD_A), false)); INST("%s <- %s == %s",        REG(EXT_FIELD_D), REG(EXT_FIELD_B), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".2..")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(SIM_REG(FIELD_B), COND_NE, SIM_REG(FIELD_A), false)); INST("%s <- %s != %s",        REG(EXT_FIELD_D), REG(EXT_FIELD_B), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".3..")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(SIM_REG(FIELD_B), COND_LT, SIM_REG(FIELD_A), true )); INST("%s <- signed %s < %s",  REG(EXT_FIELD_D), REG(EXT_FIELD_B), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".4..")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(SIM_REG(FIELD_B), COND_GE, SIM_REG(FIELD_A), true )); INST("%s <- signed %s >= %s", REG(EXT_FIELD_D), REG(EXT_FIELD_B), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".5..")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(SIM_REG(FIELD_B), COND_LT, SIM_REG(FIELD_A), false)); INST("%s <- %s < %s",         REG(EXT_FIELD_D), REG(EXT_FIELD_B), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".6..")) { CLASS(VECTOR); SIM(SIM_REG_T(EXT_FIELD_D) = lane_cond(SIM_REG(FIELD_B), COND_GE, SIM_REG(FIELD_A), false)); INST("%s <- %s > %s",         REG(EXT_FIELD_D), REG(EXT_FIELD_B), REG(EXT_FIELD_A)); }
          if (pattern_match(field_e, ".7..")) { UNKNOWN; }
          if (pattern_match(field_e, ".8..")) { UNKNOWN; }
          if (pattern_match(field_e, ".9..")) { UNKNOWN; }
          if (pattern_match(field_e, ".a..")) { UNKNOWN; }
          if (pattern_match(field_e, ".b..")) { UNKNOWN; }
          if (pattern_match(field_e, ".c..")) { UNKNOWN; }
          if (pattern_match(field_e, ".d..")) { UNKNOWN; }
          if (pattern_match(field_e, ".e..")) { UNKNOWN; }
          UNKNOWN;
        case 1:
          if (pattern_match(field_e, ".0.."))
            {
              CLASS(VECTOR);
              SIM(SIM_REG_T(EXT_FIELD_D) = interpolate_op(SIM_REG(EXT_FIELD_A), SIM_REG(EXT_FIELD_B)));
              INST("%s <- interpolate %s %s", REG(EXT_FIELD_D), REG(EXT_FIELD_A), REG(EXT_FIELD_B));
            }
          UNKNOWN;
        }
      UNKNOWN;
    }
  else if (FIELD_D == 0xf && FIELD_C == 0xf)
    {
      // extension group B
      switch (FIELD_B >> 1)
        {
        case 0:
          if (pattern_match(field_e, ".0.."))
            {
              CLASS(MUL);
              SIM(SIM_REG_T(EXT_FIELD_D) = sfullmul_op(SIM_REG(EXT_FIELD_A), SIM_REG(EXT_FIELD_B), insn_code & 0x1f));
              INST("%s <- full %s * %s >>> %d", REG(EXT_FIELD_D), REG(EXT_FIELD_A), REG(EXT_FIELD_B), insn_code & 0x1f);
            }
          if (pattern_match(field_e, ".1.."))
            {
              CLASS(MUL);
              SIM(SIM_REG_T(EXT_FIELD_D) = fullmul_op(SIM_REG(EXT_FIELD_A), SIM_REG(EXT_FIELD_B), insn_code & 0x1f));
              INST("%s <- full %s * %s >> %d", REG(EXT_FIELD_D), REG(EXT_FIELD_A), REG(EXT_FIELD_B), insn_code & 0x1f);
            }
          UNKNOWN;
        default:
          UNKNOWN;
        }
    }
  else if ((FIELD_C & 0xe) == 0xe && FIELD_B == 0xf)
    {
      // type override
      OPCODES_ASSERT(false);
    }
  UNKNOWN;
}

int
brew_insn_len(uint16_t insn_code)
{
  if (
    FIELD_D == 0xf ||
    (FIELD_C == 0xf && (FIELD_B != 0xf || FIELD_A == 0xf)) ||
    (FIELD_C == 0xe && FIELD_A == 0xf) ||
    (FIELD_C < 0xc && (FIELD_B == 0xf || FIELD_A == 0xf))
  ) {
      // FIELD_E is needed...
      if (FIELD_D == 0xf || FIELD_A != 0xf)
        return 6;
      return 4;
    }
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
  "bit-test conditional branch",
  "type manipulation",
  "vector specific",
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

static const char *reg_type_names[] = {
  "int32",        //  BREW_REG_TYPE_INT32
  "int16x2",      //  BREW_REG_TYPE_INT16x2
  "int8x4",       //  BREW_REG_TYPE_INT8x4
  "uint16x2s",    //  BREW_REG_TYPE_UINT16x2S
  "sint16x2s",    //  BREW_REG_TYPE_SINT16x2S
  "uint8x4s",     //  BREW_REG_TYPE_UINT8x4S
  "sint8x4s",     //  BREW_REG_TYPE_SINT8x4S
  "type7",
  "fp32",         //  BREW_REG_TYPE_FP32
  "fp16x2",       //  BREW_REG_TYPE_FP16x2
  "type9",
  "type10",
  "type11",
  "type12",
  "type13",
  "type14",
  "MASK"
};

const char *
brew_reg_type_name(brew_reg_types reg_type)
{
  if (reg_type > 0xf)
    reg_type = 0xf;
  return reg_type_names[reg_type];
}
