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

#define DEBUG(...) { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
//#define DEBUG(...)

/* Macros to extract operands from the instruction word.  */
#define FIELD_C ((inst_code >> 12) & 0xf)
#define FIELD_B ((inst_code >> 8) & 0xf)
#define FIELD_A ((inst_code >> 4) & 0xf)
#define FIELD_D ((inst_code >> 0) & 0xf)

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

static bool pattern_match(uint16_t inst_code, const char *pattern)
{
  const char *p;
  int nibble;
  for (p=pattern, nibble=strlen(pattern)-1;nibble >=0;--nibble, ++p)
    {
      if (*p == '.')
        {
          if (GET_NIBBLE(inst_code, nibble) == 0xf)
            {
              return false;
            }
          continue;
        }
      if (hexdigit(*p) != GET_NIBBLE(inst_code, nibble))
        {
          return false;
        }
    }
    return true;
}

static const char * reg_names[16] =
  { "$sp", "$fp", "$r2",  "$r3",  "$r4",  "$r5",  "$r6",  "$r7", 
    "$r8", "$r9", "$r10", "$r11", "$r12", "$r13", "$r14", "<<<INVALID>>>"};

static const char * sreg_names[16] =
  { "$sr0", "$sr1", "$sr2",  "$sr3",  "$sr4",  "$sr5",  "$sr6",  "$sr7", 
    "$sr8", "$sr9", "$sr10", "$sr11", "$sr12", "$sr13", "$sr14", "<<<INVALID>>>"};

static const char * freg_names[16] =
  { "$fr0", "$fr1", "$fr2",  "$fr3",  "$fr4",  "$fr5",  "$fr6",  "$fr7", 
    "$fr8", "$fr9", "$fr10", "$fr11", "$fr12", "$fr13", "$fr14", "<<<INVALID>>>"};

#define REG_D (reg_names[FIELD_D])
#define REG_A (reg_names[FIELD_A])
#define REG_B (reg_names[FIELD_B])

#define SREG_D (sreg_names[FIELD_D])
#define SREG_A (sreg_names[FIELD_A])
#define SREG_B (sreg_names[FIELD_B])

#define FREG_D (freg_names[FIELD_D])
#define FREG_A (freg_names[FIELD_A])
#define FREG_B (freg_names[FIELD_B])

static const int field_c_to_bit_map[] =
  {
    0,  1,  2,  3,  4,  5,  6,  7,
    8,  9, 14, 15, 16, 30, 31, -1
  };

static void
print_unknown_insn(fprintf_ftype fpr, void *strm_or_buffer, uint16_t inst_code, uint32_t field_e, int length)
{
  switch (length)
    {
    case 2:
      fpr(strm_or_buffer, "unknown instruction 0x%04x", inst_code);
      break;
    case 4:
      fpr(strm_or_buffer, "unknown instruction 0x%04x 0x%04x", inst_code, field_e);
      break;
    case 6:
      fpr(strm_or_buffer, "unknown instruction 0x%04x 0x%08x", inst_code, field_e);
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

static char *
format_link_offset(uint8_t field_b, char *buffer)
{
  int32_t offset = field_b << 28 >> 27;
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

#define UNKNOWN { print_unknown_insn(fpr, strm_or_buffer, inst_code, field_e, length); return; }
#define INST(...) { fpr(strm_or_buffer, __VA_ARGS__ ); return; }

#define REGULAR_ALU_PATTERN_UUU(base, op, prefix)                                                                                          \
  case 0x##base:                                                                                                                           \
    if (pattern_match(inst_code, #base "...")) INST("%s <- %s %s " #op " %s", REG_D, #prefix, REG_A, REG_B);                               \
    if (pattern_match(inst_code, #base ".f.")) INST("%s <- %s %u (0x%x) " #op " %s", REG_D, #prefix, field_e, field_e, REG_B);             \
    if (pattern_match(inst_code, #base "f..")) INST("%s <- %s short %u (0x%x) " #op " %s", REG_D, #prefix, field_e, field_e, REG_A);       \
    UNKNOWN;                                                                                                                               \
    break;

#define REGULAR_ALU_PATTERN_SSU(base, op, prefix)                                                                                          \
  case 0x##base:                                                                                                                           \
    if (pattern_match(inst_code, #base "...")) INST("%s <- %s %s " #op " %s", SREG_D, #prefix, SREG_A, REG_A);                             \
    if (pattern_match(inst_code, #base ".f.")) INST("%s <- %s %d (0x%x) " #op " %s", SREG_D, #prefix, field_e, field_e, REG_B);            \
    if (pattern_match(inst_code, #base "f..")) INST("%s <- %s short %d (0x%x) " #op " %s", SREG_D, #prefix, field_e, field_e, REG_A);      \
    UNKNOWN;                                                                                                                               \
    break;

#define REGULAR_ALU_PATTERN_SSS(base, op, prefix)                                                                                          \
  case 0x##base:                                                                                                                           \
    if (pattern_match(inst_code, #base "...")) INST("%s <- %s %s " #op " %s", SREG_D, #prefix, SREG_A, SREG_A);                            \
    if (pattern_match(inst_code, #base ".f.")) INST("%s <- %s %d (0x%x) " #op " %s", SREG_D, #prefix, field_e, field_e, SREG_B);           \
    if (pattern_match(inst_code, #base "f..")) INST("%s <- %s short %d (0x%x) " #op " %s", SREG_D, #prefix, field_e, field_e, SREG_A);     \
    UNKNOWN;                                                                                                                               \
    break;

#define REGULAR_ALU_PATTERN_FFF(base, op, prefix)                                                                                          \
  case 0x##base:                                                                                                                           \
    if (pattern_match(inst_code, #base "...")) INST("%s <- %s %s " #op " %s", FREG_D, #prefix, FREG_A, FREG_A);                            \
    if (pattern_match(inst_code, #base ".f.")) INST("%s <- %s %f (0x%0x)" #op " %s", FREG_D, #prefix, field_e_as_float, field_e, FREG_B);  \
    UNKNOWN;                                                                                                                               \
    break;

void
brew_print_insn(fprintf_ftype fpr, void *strm_or_buffer, uint16_t inst_code, uint32_t field_e)
{
  int length;

  float field_e_as_float;
  char branch_target_str[255];

  length = brew_inst_len(inst_code);
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
      if (FIELD_C == 2 && FIELD_B == FIELD_A && FIELD_D == FIELD_A) INST("NOP"); // $rD <- $rD | $rD
      if (FIELD_C == 1 && FIELD_B == FIELD_A) INST("%s <- 0", REG_D); // $rD <- $rX ^ $rX
      if (FIELD_C == 2 && FIELD_B == FIELD_A) INST("%s <- %s", REG_D, REG_B); // $rD <- $rX | $rX

      // ALU group
      switch (FIELD_C)
        {
        case 0x0: // Unary and special
          if ((inst_code >> 4) == 0x000)
            {
              // Exception group
              if (pattern_match(inst_code, "0000")) INST("FILL");
              if (pattern_match(inst_code, "0001")) INST("BREAK");
              if (pattern_match(inst_code, "0002")) INST("SYSCALL");
              if (pattern_match(inst_code, "0003")) INST("STU");
              if (pattern_match(inst_code, "0004")) INST("SII");
              if (pattern_match(inst_code, "0005")) INST("WOI");
              UNKNOWN;
            }
          if ((inst_code >> 4) == 0x000)
            {
              // Atomic group
              if (pattern_match(inst_code, "0010")) INST("FENCE");
              if (pattern_match(inst_code, "0011")) INST("WFENCE");
              UNKNOWN;
            }
          if ((inst_code >> 8) == 0x00)
            {
              // Branch group
              if (pattern_match(inst_code, "002.")) INST("$pc <- %s", REG_D);
              if (pattern_match(inst_code, "003.")) INST("$tpc <- %s", REG_D);
              if (pattern_match(inst_code, "004.")) INST("%s <- $pc", REG_D);
              if (pattern_match(inst_code, "005.")) INST("%s <- $tpc", REG_D);
              UNKNOWN;
            }
          // Unary group
          if (pattern_match(inst_code, "01..")) INST("%s <- %s + 1", REG_D, REG_A);
          if (pattern_match(inst_code, "02..")) INST("%s <- %s - 1", REG_D, REG_A);
          if (pattern_match(inst_code, "03..")) INST("%s <- -%s", SREG_D, SREG_A);
          if (pattern_match(inst_code, "04..")) INST("%s <- ~%s", REG_D, REG_A);
          if (pattern_match(inst_code, "05..")) INST("%s <- bswap %s", REG_D, REG_A);
          if (pattern_match(inst_code, "06..")) INST("%s <- wswap %s", REG_D, REG_A);
          if (pattern_match(inst_code, "07..")) INST("%s <- bsi %s", REG_D, REG_A);
          if (pattern_match(inst_code, "08..")) INST("%s <- wsi %s", REG_D, REG_A);
          if (pattern_match(inst_code, "09..")) INST("%s <- floor %s", SREG_D, FREG_A);
          if (pattern_match(inst_code, "0a..")) INST("%s <- %s", FREG_D, SREG_A);
          if (pattern_match(inst_code, "0b..")) INST("%s <- 1 / %s", FREG_D, FREG_A);
          if (pattern_match(inst_code, "0c..")) INST("%s <- rsqrt %s", FREG_D, FREG_A);
          if (pattern_match(inst_code, "0d..")) INST("%s <- $pc + %s", FREG_D, format_link_offset(FIELD_A, branch_target_str));

          // Load immediate
          if (pattern_match(inst_code, "00f.")) INST("%s <- %u (0x%x)", REG_D, field_e, field_e);
          if (pattern_match(inst_code, "01f.")) INST("$pc <- %u (0x%x)", field_e, field_e);

          if (pattern_match(inst_code, "0f0.")) INST("%s <- short %u (0x%x)", REG_D, field_e, field_e);

          UNKNOWN;
          break;
        REGULAR_ALU_PATTERN_UUU(1, ^,);
        REGULAR_ALU_PATTERN_UUU(2, |,);
        REGULAR_ALU_PATTERN_UUU(3, &,);
        REGULAR_ALU_PATTERN_UUU(4, +,);
        REGULAR_ALU_PATTERN_UUU(5, -,);
        REGULAR_ALU_PATTERN_UUU(6, <<,);
        REGULAR_ALU_PATTERN_UUU(7, >>,);
        REGULAR_ALU_PATTERN_SSU(8, >>,);
        REGULAR_ALU_PATTERN_UUU(9, *,);
        REGULAR_ALU_PATTERN_UUU(a, *, upper);
        REGULAR_ALU_PATTERN_SSS(b, *, upper);
        REGULAR_ALU_PATTERN_FFF(c, +,);
        REGULAR_ALU_PATTERN_FFF(d, -,);
        REGULAR_ALU_PATTERN_FFF(e, *,);
        }
    }
  else if (FIELD_C == 0xf)
    {
      // load-store group
      if (pattern_match(inst_code, "f0..")) INST("%s <- MEM8[%s]", SREG_D, REG_A);
      if (pattern_match(inst_code, "f1..")) INST("%s <- MEM8[%s]", REG_D, REG_A);
      if (pattern_match(inst_code, "f2..")) INST("%s <- MEM16[%s]", SREG_D, REG_A);
      if (pattern_match(inst_code, "f3..")) INST("%s <- MEM16[%s]", REG_D, REG_A);
      if (pattern_match(inst_code, "f4..")) INST("%s <- MEM32[%s]", REG_D, REG_A);
      if (pattern_match(inst_code, "f5..")) INST("MEM8[%s] <- %s", REG_A, REG_D);
      if (pattern_match(inst_code, "f6..")) INST("MEM16[%s] <- %s", REG_A, REG_D);
      if (pattern_match(inst_code, "f7..")) INST("MEM32[%s] <- %s", REG_A, REG_D);

      if (pattern_match(inst_code, "f8f.")) INST("%s <- MEM8[%d (0x%x)]", SREG_D, field_e, field_e);
      if (pattern_match(inst_code, "f9f.")) INST("%s <- MEM8[%d (0x%x)]", REG_D, field_e, field_e);
      if (pattern_match(inst_code, "faf.")) INST("%s <- MEM16[%d (0x%x)]", SREG_D, field_e, field_e);
      if (pattern_match(inst_code, "fbf.")) INST("%s <- MEM16[%d (0x%x)]", REG_D, field_e, field_e);
      if (pattern_match(inst_code, "fcf.")) INST("%s <- MEM32[%d (0x%x)]", REG_D, field_e, field_e);
      if (pattern_match(inst_code, "fdf.")) INST("MEM8[%d (0x%x)] <- %s", field_e, field_e, REG_D);
      if (pattern_match(inst_code, "fef.")) INST("MEM16[%d (0x%x)] <- %s", field_e, field_e, REG_D);
      if (pattern_match(inst_code, "fff.")) INST("MEM32[%d (0x%x)] <- %s", field_e, field_e, REG_D);

      if (pattern_match(inst_code, "f8..")) INST("%s <- MEM8[%s,%d (0x%x)]", SREG_D, REG_A, field_e, field_e);
      if (pattern_match(inst_code, "f9..")) INST("%s <- MEM8[%s,%d (0x%x)]", REG_D, REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fa..")) INST("%s <- MEM16[%s,%d (0x%x)]", SREG_D, REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fb..")) INST("%s <- MEM16[%s,%d (0x%x)]", REG_D, REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fc..")) INST("%s <- MEM32[%s,%d (0x%x)]", REG_D, REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fd..")) INST("MEM8[%s,%d (0x%x)] <- %s", REG_A, field_e, field_e, REG_D);
      if (pattern_match(inst_code, "fe..")) INST("MEM16[%s,%d (0x%x)] <- %s", REG_A, field_e, field_e, REG_D);
      if (pattern_match(inst_code, "ff..")) INST("MEM32[%s,%d (0x%x)] <- %s", REG_A, field_e, field_e, REG_D);

      if (pattern_match(inst_code, "fcff")) INST("$pc <- MEM32[%d (0x%x)]", field_e, field_e);
      if (pattern_match(inst_code, "fc.f")) INST("$pc <- MEM32[%s,%d (0x%x)]", REG_A, field_e, field_e);
    }
  else if (FIELD_D == 0xf && FIELD_C != 0xf)
    {
      // conditional branch group
      if (pattern_match(inst_code, "0.0f")) INST("if %s == 0 $pc %s", REG_B, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "0.1f")) INST("if %s != 0 $pc %s", REG_B, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "0.2f")) INST("if %s < 0 $pc %s", SREG_B, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "0.3f")) INST("if %s >= 0 $pc %s", SREG_B, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "0.4f")) INST("if %s > 0 $pc %s", SREG_B, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "0.5f")) INST("if %s <= 0 $pc %s", SREG_B, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "0.6f")) UNKNOWN;
      if (pattern_match(inst_code, "0.7f")) UNKNOWN;
      if (pattern_match(inst_code, "0.8f")) UNKNOWN;
      if (pattern_match(inst_code, "0.9f")) UNKNOWN;
      if (pattern_match(inst_code, "0.af")) UNKNOWN;
      if (pattern_match(inst_code, "0.bf")) INST("if %s < 0 $pc %s", FREG_B, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "0.cf")) INST("if %s >= 0 $pc %s", FREG_B, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "0.df")) INST("if %s > 0 $pc %s", FREG_B, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "0.ef")) INST("if %s <= 0 $pc %s", FREG_B, format_branch_target(field_e, branch_target_str));

      if (pattern_match(inst_code, "1..f")) INST("if %s == %s $pc %s", REG_B, REG_A, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "2..f")) INST("if %s != %s $pc %s", REG_B, REG_A, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "3..f")) INST("if %s < %s $pc %s", SREG_B, SREG_A, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "4..f")) INST("if %s >= %s $pc %s", SREG_B, SREG_A, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "5..f")) INST("if %s < %s $pc %s", REG_B, REG_A, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "6..f")) INST("if %s >= %s $pc %s", REG_B, REG_A, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "7..f")) UNKNOWN;
      if (pattern_match(inst_code, "8..f")) UNKNOWN;
      if (pattern_match(inst_code, "9..f")) UNKNOWN;
      if (pattern_match(inst_code, "a..f")) UNKNOWN;
      if (pattern_match(inst_code, "b..f")) UNKNOWN;
      if (pattern_match(inst_code, "c..f")) UNKNOWN;
      if (pattern_match(inst_code, "d..f")) INST("if %s < %s $pc %s", FREG_B, FREG_A, format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "e..f")) INST("if %s >= %s $pc %s", FREG_B, FREG_A, format_branch_target(field_e, branch_target_str));

      if (pattern_match(inst_code, ".f.f")) INST("if %s[%d] == 1 $pc %s", REG_A, field_c_to_bit_map[FIELD_C], format_branch_target(field_e, branch_target_str));
      if (pattern_match(inst_code, "..ff")) INST("if %s[%d] == 0 $pc %s", REG_B, field_c_to_bit_map[FIELD_C], format_branch_target(field_e, branch_target_str));
    }
  UNKNOWN;
}

int
brew_inst_len(uint16_t inst_code)
{
  if (FIELD_D == 0xf || FIELD_B == 0xf || FIELD_A == 0xf || (inst_code >> 11) == 0x1f)
    // We do need FIELD_E, but what size?
    if (((inst_code >> 11) == 0x1f) || (FIELD_A == 0xf && FIELD_D != 0xf))
      return 6;
    else
      return 4;
  else
    return 2;
}

