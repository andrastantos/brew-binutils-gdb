/* Disassemble brew instructions.
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

/* TODO: right now this is not changed from the moxie baseline at all. It should be!!!! */

#include "sysdep.h"
#include <stdio.h>
#include <stdint.h>

#define STATIC_TABLE
#define DEFINE_TABLE

#include "opcode/brew.h"
#include "disassemble.h"

#define DEBUG(...) { fprintf (stderr, __VA_ARGS__); fprintf(stderr, "\n"); }
//#define DEBUG(...)

static fprintf_ftype fpr;
static void *stream;
static struct disassemble_info * info;
static bfd_vma addr;

/* Macros to extract operands from the instruction word.  */
#define FIELD_C ((inst_code >> 12) & 0xf)
#define FIELD_B ((inst_code >> 8) & 0xf)
#define FIELD_A ((inst_code >> 4) & 0xf)
#define FIELD_D ((inst_code >> 0) & 0xf)

#define GET_NIBBLE(i, nibble) ((i >> (nibble*4)) & 0xf)

static bool hexdigit(char digit, int *val)
{
  if (digit >= '0' && digit <= '9')
    {
      *val = digit - '0';
      return true;
    }
  if (digit >= 'a' && digit <= 'f')
    {
      *val = digit - 'a' + 10;
      return true;
    }
  if (digit >= 'A' && digit <= 'F')
    {
      *val = digit - 'A' + 10;
      return true;
    }
  return false;
}

static bool pattern_match(uint16_t inst_code, const char *pattern)
{
  const char *p;
  int nibble;
  int hex_p;
  for (p=pattern, nibble=3;nibble >=0;--nibble, ++p)
    {
      if (*p == '.')
        {
          if (GET_NIBBLE(inst_code, nibble) == 0xf)
            {
              return false;
            }
          continue;
        }
      if (!hexdigit(*p, &hex_p))
        {
          return false;
        }
      if (hex_p != GET_NIBBLE(inst_code, nibble))
        {
          return false;
        }
    }
    return true;
}

#define TREG_D (treg_names[FIELD_D])
#define TREG_A (treg_names[FIELD_A])
#define TREG_B (treg_names[FIELD_B])

#define REG_D (reg_names[FIELD_D])
#define REG_A (reg_names[FIELD_A])
#define REG_B (reg_names[FIELD_B])

#define SREG_D (sreg_names[FIELD_D])
#define SREG_A (sreg_names[FIELD_A])
#define SREG_B (sreg_names[FIELD_B])

#define FREG_D (freg_names[FIELD_D])
#define FREG_A (freg_names[FIELD_A])
#define FREG_B (freg_names[FIELD_B])

static const char * treg_names[16] =
  { "$tpc", "$r1", "$r2",  "$r3",  "$r4",  "$r5",  "$r6",  "$r7", 
    "$r8",  "$r9", "$r10", "$r11", "$r12", "$r13", "$r14", "<<<INVALID>>>"};

static const char * reg_names[16] =
  { "$pc", "$r1", "$r2",  "$r3",  "$r4",  "$r5",  "$r6",  "$r7", 
    "$r8", "$r9", "$r10", "$r11", "$r12", "$r13", "$r14", "<<<INVALID>>>"};

static const char * sreg_names[16] =
  { "$spc", "$sr1", "$sr2",  "$sr3",  "$sr4",  "$sr5",  "$sr6",  "$sr7", 
    "$sr8", "$sr9", "$sr10", "$sr11", "$sr12", "$sr13", "$sr14", "<<<INVALID>>>"};

static const char * freg_names[16] =
  { "$fpc", "$fr1", "$fr2",  "$fr3",  "$fr4",  "$fr5",  "$fr6",  "$fr7", 
    "$fr8", "$fr9", "$fr10", "$fr11", "$fr12", "$fr13", "$fr14", "<<<INVALID>>>"};

static bool get_uint16(uint16_t *buffer)
{
  int status;

  if ((status = info->read_memory_func(addr, (bfd_byte *)buffer, 2, info)))
    {
      info->memory_error_func (status, addr, info);
      return false;
    }
  addr += 2;
  *buffer = bfd_getl16(buffer);
  return true;
}

static bool get_uint32(uint32_t *buffer)
{
  int status;

  if ((status = info->read_memory_func(addr, (bfd_byte *)buffer, 4, info)))
    {
      info->memory_error_func (status, addr, info);
      return false;
    }
  addr += 4;
  *buffer = bfd_getl32(buffer);
  return true;
}

#define UNKNOWN \
{ \
  if (length == 2) \
    { \
      fpr(stream, "unknown instruction 0x%04x", inst_code); \
    } \
  else \
    { \
      fpr(stream, "unknown instruction 0x%04x 0x%08x", inst_code, field_e); \
    } \
  return length; \
}

static char branch_target_buffer[255];
static char *branch_target(uint32_t field_e)
{
  if ((field_e & 0) == 0)
    {
      /* Abosolute address */
      sprintf(branch_target_buffer, "<- %u (0x%x)", field_e, field_e);
    }
  else
    {
      /* Relative address */
      sprintf(branch_target_buffer, "<- $pc+%d (0x%x)", (int32_t)field_e, field_e);
    }
  return branch_target_buffer;
}

#define INST(...) { fpr(stream, __VA_ARGS__ ); return length; }
int
print_insn_brew (bfd_vma addrP, struct disassemble_info * infoP)
{
  int length = 2;
  uint16_t inst_code;
  uint32_t field_e;
  float field_e_as_float;

  OPCODES_ASSERT(infoP->endian == BFD_ENDIAN_LITTLE);

  addr = addrP;
  info = infoP;
  stream = info->stream;
  fpr = info->fprintf_func;


  if (!get_uint16(&inst_code))
    {
      return -1;
    }
  /* Determine if we need field_e and read it if we do */
  if (FIELD_D == 0xf || FIELD_B == 0xf || FIELD_A == 0xf || (inst_code >> 11) == 0x1f) {
    length = 6;
    if (!get_uint32(&field_e))
      {
        return -1;
      }
    memcpy(&field_e_as_float, &field_e, 4);
  }
  switch (FIELD_C)
    {
    case 0x0: /* ^ and special */
      if (pattern_match(inst_code, "0000")) INST("FILL");
      if (pattern_match(inst_code, "0110")) INST("BREAK");
      if (pattern_match(inst_code, "0220")) INST("SYSCALL");
      if (pattern_match(inst_code, "0330")) INST("STU");
      if (pattern_match(inst_code, "0440")) INST("SII");
      if (pattern_match(inst_code, "0dd0")) INST("FENCE");
      if (pattern_match(inst_code, "0ee0")) INST("WFENCE");
      if (pattern_match(inst_code, "0..0")) UNKNOWN;
      if (pattern_match(inst_code, "0ff.")) INST("%s <- %u (0x%x)", REG_D, field_e, field_e);
      if (pattern_match(inst_code, "0f..")) UNKNOWN;
      if (pattern_match(inst_code, "0.f.")) INST("%s <- %s ^ %u (0x%x)", REG_D, REG_B, field_e, field_e);
      if (pattern_match(inst_code, "0...")) INST("%s <- %s ^ %s", REG_D, REG_B, REG_A);
      break;
    case 0x1:
      if (pattern_match(inst_code, "1000")) INST("WOI");
      if (pattern_match(inst_code, "1f..")) UNKNOWN;
      if (pattern_match(inst_code, "1.f.")) INST("%s <- %s | %u (0x%x)", REG_D, REG_B, field_e, field_e);
      if (pattern_match(inst_code, "1...")) INST("%s <- %s | %s", REG_D, REG_B, REG_A);
      break;
    case 0x2:
      if (pattern_match(inst_code, "2f..")) UNKNOWN;
      if (pattern_match(inst_code, "2.f.")) INST("%s <- %s & %u (0x%x)", REG_D, REG_B, field_e, field_e);
      if (pattern_match(inst_code, "2...")) INST("%s <- %s & %s", REG_D, REG_B, REG_A);
      break;
    case 0x3:
      if (pattern_match(inst_code, "3f..")) INST("%s <- %u (0x%x) - %s", REG_D, field_e, field_e, REG_A);
      if (pattern_match(inst_code, "3.f.")) INST("%s <- %s - %u (0x%x)", REG_D, REG_B, field_e, field_e);
      if (pattern_match(inst_code, "3...")) INST("%s <- %s - %s", REG_D, REG_B, REG_A);
      break;
    case 0x4:
      if (pattern_match(inst_code, "4f..")) UNKNOWN;
      if (pattern_match(inst_code, "4.f.")) INST("%s <- %s + %u (0x%x)", REG_D, REG_B, field_e, field_e);
      if (pattern_match(inst_code, "4...")) INST("%s <- %s + %s", REG_D, REG_B, REG_A);
      break;
    case 0x5:
      if (pattern_match(inst_code, "5f..")) INST("%s <- %u (0x%x) << %s", REG_D, field_e, field_e, REG_A);
      if (pattern_match(inst_code, "5.f.")) INST("%s <- %s << %u (0x%x)", REG_D, REG_B, field_e, field_e);
      if (pattern_match(inst_code, "5...")) INST("%s <- %s << %s", REG_D, REG_B, REG_A);
      break;
    case 0x6:
      if (pattern_match(inst_code, "6f..")) INST("%s <- %u (0x%x) >> %s", REG_D, field_e, field_e, REG_A);
      if (pattern_match(inst_code, "6.f.")) INST("%s <- %s >> %u (0x%x)", REG_D, REG_B, field_e, field_e);
      if (pattern_match(inst_code, "6...")) INST("%s <- %s >> %s", REG_D, REG_B, REG_A);
      break;
    case 0x7:
      if (pattern_match(inst_code, "7f..")) INST("%s <- %d (0x%x) >> %s", SREG_D, (int32_t)field_e, field_e, REG_A);
      if (pattern_match(inst_code, "7.f.")) INST("%s <- %s >> %u (0x%x)", SREG_D, SREG_B, field_e, field_e);
      if (pattern_match(inst_code, "7...")) INST("%s <- %s >> %s", SREG_D, SREG_B, REG_A);
      break;
    case 0x8:
      if (pattern_match(inst_code, "8f..")) UNKNOWN;
      if (pattern_match(inst_code, "8ff.")) UNKNOWN;
      if (pattern_match(inst_code, "80f.")) INST("%s <- %u (0x%x)", TREG_D, field_e, field_e);
      if (pattern_match(inst_code, "8.f.")) INST("%s <- %s * %u (0x%x)", REG_D, REG_B, field_e, field_e);
      if (pattern_match(inst_code, "80..")) INST("%s <- %s", TREG_D, TREG_A);
      if (pattern_match(inst_code, "8.00")) UNKNOWN;
      if (pattern_match(inst_code, "8...")) INST("%s <- %s * %s", REG_D, REG_B, REG_A);
      break;
    case 0x9:
      if (pattern_match(inst_code, "9f..")) UNKNOWN;
      if (pattern_match(inst_code, "9ff.")) UNKNOWN;
      if (pattern_match(inst_code, "90f.")) UNKNOWN;
      if (pattern_match(inst_code, "9.f0")) UNKNOWN;
      if (pattern_match(inst_code, "9.f.")) UNKNOWN;
      if (pattern_match(inst_code, "900.")) UNKNOWN;
      if (pattern_match(inst_code, "90..")) INST("%s <- %s + 1", REG_D, REG_A);
      if (pattern_match(inst_code, "9.0.")) INST("%s <- %s - 1", REG_D, REG_B);
      if (pattern_match(inst_code, "9..0")) UNKNOWN;
      if (pattern_match(inst_code, "9...")) UNKNOWN;
      break;
    case 0xa:
      if (pattern_match(inst_code, "af..")) UNKNOWN;
      if (pattern_match(inst_code, "aff.")) UNKNOWN;
      if (pattern_match(inst_code, "a0f.")) UNKNOWN;
      if (pattern_match(inst_code, "a.f0")) UNKNOWN;
      if (pattern_match(inst_code, "a.f.")) INST("%s <- upper %s * %u (0x%x)", REG_D, REG_B, field_e, field_e);
      if (pattern_match(inst_code, "a00.")) UNKNOWN;
      if (pattern_match(inst_code, "a0..")) INST("%s <- -%s", SREG_D, SREG_A);
      if (pattern_match(inst_code, "a.0.")) INST("%s <- ~%s", REG_D, REG_B);
      if (pattern_match(inst_code, "a..0")) UNKNOWN;
      if (pattern_match(inst_code, "a...")) INST("%s <- upper %s * %s", REG_D, REG_B, REG_A);
      break;
    case 0xb:
      if (pattern_match(inst_code, "bf..")) UNKNOWN;
      if (pattern_match(inst_code, "bff.")) UNKNOWN;
      if (pattern_match(inst_code, "b0f.")) UNKNOWN;
      if (pattern_match(inst_code, "b.f0")) UNKNOWN;
      if (pattern_match(inst_code, "b.f.")) INST("%s <- upper %s * %d (0x%x)", SREG_D, SREG_B, (int32_t)field_e, field_e);
      if (pattern_match(inst_code, "b00.")) UNKNOWN;
      if (pattern_match(inst_code, "b0..")) INST("%s <- bswap %s", REG_D, REG_A);
      if (pattern_match(inst_code, "b.0.")) INST("%s <- wswap %s", REG_D, REG_B);
      if (pattern_match(inst_code, "b..0")) UNKNOWN;
      if (pattern_match(inst_code, "b...")) INST("%s <- upper %s * %s", SREG_D, SREG_B, SREG_A);
      break;
    case 0xc:
      if (pattern_match(inst_code, "cf..")) UNKNOWN;
      if (pattern_match(inst_code, "c0f.")) UNKNOWN;
      if (pattern_match(inst_code, "c.f0")) UNKNOWN;
      if (pattern_match(inst_code, "cff.")) UNKNOWN;
      if (pattern_match(inst_code, "c.f.")) INST("%s <- %s + %f", FREG_D, FREG_B, field_e_as_float);
      if (pattern_match(inst_code, "c..0")) UNKNOWN;
      if (pattern_match(inst_code, "c00.")) UNKNOWN;
      if (pattern_match(inst_code, "c0..")) INST("%s <- bsi %s", REG_D, REG_A);
      if (pattern_match(inst_code, "c.0.")) INST("%s <- wsi %s", REG_D, REG_B);
      if (pattern_match(inst_code, "c...")) INST("%s <- %s + %s", FREG_D, FREG_B, FREG_A);
      break;
    case 0xd:
      if (pattern_match(inst_code, "df..")) INST("%s <- %f - %s", FREG_D, field_e_as_float, FREG_A);
      if (pattern_match(inst_code, "d0f.")) UNKNOWN;
      if (pattern_match(inst_code, "d.f0")) UNKNOWN;
      if (pattern_match(inst_code, "dff.")) UNKNOWN;
      if (pattern_match(inst_code, "d.f.")) INST("%s <- %s - %f", FREG_D, FREG_B, field_e_as_float);
      if (pattern_match(inst_code, "d..0")) UNKNOWN;
      if (pattern_match(inst_code, "d00.")) UNKNOWN;
      if (pattern_match(inst_code, "d.0.")) INST("%s <- %s", FREG_D, SREG_B);
      if (pattern_match(inst_code, "d0..")) INST("%s <- floor %s", SREG_D, FREG_A);
      if (pattern_match(inst_code, "d...")) INST("%s <- %s - %s", FREG_D, FREG_B, FREG_A);
      break;
    case 0xe:
      if (pattern_match(inst_code, "ef..")) UNKNOWN;
      if (pattern_match(inst_code, "e0f.")) UNKNOWN;
      if (pattern_match(inst_code, "e.f0")) UNKNOWN;
      if (pattern_match(inst_code, "eff.")) UNKNOWN;
      if (pattern_match(inst_code, "e.f.")) INST("%s <- %s * %f", FREG_D, FREG_B, field_e_as_float);
      if (pattern_match(inst_code, "e..0")) UNKNOWN;
      if (pattern_match(inst_code, "e00.")) UNKNOWN;
      if (pattern_match(inst_code, "e.0.")) INST("%s <- rsqrt %s", FREG_D, FREG_B);
      if (pattern_match(inst_code, "e0..")) INST("%s <- 1 / %s", FREG_D, FREG_A);
      if (pattern_match(inst_code, "e...")) INST("%s <- %s * %s", FREG_D, FREG_B, FREG_A);
      break;
    case 0xf:
      if (pattern_match(inst_code, "f0..")) INST("%s <- MEM8[%s]", SREG_D, REG_A);
      if (pattern_match(inst_code, "f1..")) INST("%s <- MEM8[%s]", REG_D, REG_A);
      if (pattern_match(inst_code, "f2..")) INST("%s <- MEM16[%s]", SREG_D, REG_A);
      if (pattern_match(inst_code, "f3..")) INST("%s <- MEM16[%s]", REG_D, REG_A);
      if (pattern_match(inst_code, "f4..")) INST("%s <- MEM32[%s]", REG_D, REG_A);
      if (pattern_match(inst_code, "f5..")) INST("MEM8[%s] <- %s", REG_A, REG_D);
      if (pattern_match(inst_code, "f6..")) INST("MEM16[%s] <- %s", REG_A, REG_D);
      if (pattern_match(inst_code, "f7..")) INST("MEM32[%s] <- %s", REG_A, REG_D);
      if (pattern_match(inst_code, "f7.f")) UNKNOWN;
      if (pattern_match(inst_code, "f7f.")) UNKNOWN;

      if (pattern_match(inst_code, "f8f.")) INST("%s <- MEM8[%d (0x%x)]", SREG_D, field_e, field_e);
      if (pattern_match(inst_code, "f8ff")) INST("$stpc <- MEM8[%d (0x%x)]", field_e, field_e);
      if (pattern_match(inst_code, "f9f.")) INST("%s <- MEM8[%d (0x%x)]", REG_D, field_e, field_e);
      if (pattern_match(inst_code, "f9ff")) INST("$tpc <- MEM8[%d (0x%x)]", field_e, field_e);
      if (pattern_match(inst_code, "faf.")) INST("%s <- MEM16[%d (0x%x)]", SREG_D, field_e, field_e);
      if (pattern_match(inst_code, "faff")) INST("$stpc <- MEM16[%d (0x%x)]", field_e, field_e);
      if (pattern_match(inst_code, "fbf.")) INST("%s <- MEM16[%d (0x%x)]", REG_D, field_e, field_e);
      if (pattern_match(inst_code, "fbff")) INST("$tpc <- MEM16[%d (0x%x)]", field_e, field_e);
      if (pattern_match(inst_code, "fcf.")) INST("%s <- MEM32[%d (0x%x)]", REG_D, field_e, field_e);
      if (pattern_match(inst_code, "fcff")) INST("$tpc <- MEM32[%d (0x%x)]", field_e, field_e);
      if (pattern_match(inst_code, "fdf.")) INST("MEM8[%d (0x%x)] <- %s", field_e, field_e, REG_D);
      if (pattern_match(inst_code, "fdff")) INST("MEM8[%d (0x%x)] <- $tpc", field_e, field_e);
      if (pattern_match(inst_code, "fef.")) INST("MEM16[%d (0x%x)] <- %s", field_e, field_e, REG_D);
      if (pattern_match(inst_code, "feff")) INST("MEM16[%d (0x%x)] <- $tpc", field_e, field_e);
      if (pattern_match(inst_code, "fff.")) INST("MEM32[%d (0x%x)] <- %s", field_e, field_e, REG_D);
      if (pattern_match(inst_code, "ffff")) INST("MEM32[%d (0x%x)] <- $tpc", field_e, field_e);

      if (pattern_match(inst_code, "f8..")) INST("%s <- MEM8[%s,%d (0x%x)]", SREG_D, REG_A, field_e, field_e);
      if (pattern_match(inst_code, "f8.f")) INST("$stpc <- MEM8[%s,%d (0x%x)]", REG_A, field_e, field_e);
      if (pattern_match(inst_code, "f9..")) INST("%s <- MEM8[%s,%d (0x%x)]", REG_D, REG_A, field_e, field_e);
      if (pattern_match(inst_code, "f9.f")) INST("$tpc <- MEM8[%s,%d (0x%x)]", REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fa..")) INST("%s <- MEM16[%s,%d (0x%x)]", SREG_D, REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fa.f")) INST("$stpc <- MEM16[%s,%d (0x%x)]", REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fb..")) INST("%s <- MEM16[%s,%d (0x%x)]", REG_D, REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fb.f")) INST("$tpc <- MEM16[%s,%d (0x%x)]", REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fc..")) INST("%s <- MEM32[%s,%d (0x%x)]", REG_D, REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fc.f")) INST("$tpc <- MEM32[%s,%d (0x%x)]", REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fd..")) INST("MEM8[%s,%d (0x%x)] <- %s", REG_A, field_e, field_e, REG_D);
      if (pattern_match(inst_code, "fd.f")) INST("MEM8[%s,%d (0x%x)] <- $tpc", REG_A, field_e, field_e);
      if (pattern_match(inst_code, "fe..")) INST("MEM16[%s,%d (0x%x)] <- %s", REG_A, field_e, field_e, REG_D);
      if (pattern_match(inst_code, "fe.f")) INST("MEM16[%s,%d (0x%x)] <- $tpc", REG_A, field_e, field_e);
      if (pattern_match(inst_code, "ff..")) INST("MEM32[%s,%d (0x%x)] <- %s", REG_A, field_e, field_e, REG_D);
      if (pattern_match(inst_code, "ff.f")) INST("MEM32[%s,%d (0x%x)] <- $tpc", REG_A, field_e, field_e);
      break;
    default:
      OPCODES_ASSERT(false);
    }
  if (FIELD_D == 0xf)
    {
      if (pattern_match(inst_code, "0.0f")) INST("if %s == 0 $pc %s", REG_B, branch_target(field_e));
      if (pattern_match(inst_code, "0.1f")) INST("if %s != 0 $pc %s", REG_B, branch_target(field_e));
      if (pattern_match(inst_code, "0.2f")) INST("if %s < 0 $pc %s", SREG_B, branch_target(field_e));
      if (pattern_match(inst_code, "0.3f")) INST("if %s >= 0 $pc %s", SREG_B, branch_target(field_e));
      if (pattern_match(inst_code, "0.4f")) INST("if %s > 0 $pc %s", SREG_B, branch_target(field_e));
      if (pattern_match(inst_code, "0.5f")) INST("if %s <= 0 $pc %s", SREG_B, branch_target(field_e));
      if (pattern_match(inst_code, "0.6f")) UNKNOWN;
      if (pattern_match(inst_code, "0.7f")) UNKNOWN;
      if (pattern_match(inst_code, "0.8f")) UNKNOWN;
      if (pattern_match(inst_code, "0.9f")) UNKNOWN;
      if (pattern_match(inst_code, "0.af")) UNKNOWN;
      if (pattern_match(inst_code, "0.bf")) INST("if %s < 0 $pc %s", FREG_B, branch_target(field_e));
      if (pattern_match(inst_code, "0.cf")) INST("if %s >= 0 $pc %s", FREG_B, branch_target(field_e));
      if (pattern_match(inst_code, "0.df")) INST("if %s > 0 $pc %s", FREG_B, branch_target(field_e));
      if (pattern_match(inst_code, "0.ef")) INST("if %s <= 0 $pc %s", FREG_B, branch_target(field_e));

      if (pattern_match(inst_code, "1..f")) INST("if %s == %s $pc %s", REG_B, REG_A, branch_target(field_e));
      if (pattern_match(inst_code, "2..f")) INST("if %s != %s $pc %s", REG_B, REG_A, branch_target(field_e));
      if (pattern_match(inst_code, "3..f")) INST("if %s < %s $pc %s", SREG_B, SREG_A, branch_target(field_e));
      if (pattern_match(inst_code, "4..f")) INST("if %s >= %s $pc %s", SREG_B, SREG_A, branch_target(field_e));
      if (pattern_match(inst_code, "5..f")) INST("if %s < %s $pc %s", REG_B, REG_A, branch_target(field_e));
      if (pattern_match(inst_code, "6..f")) INST("if %s >= %s $pc %s", REG_B, REG_A, branch_target(field_e));
      if (pattern_match(inst_code, "7..f")) UNKNOWN;
      if (pattern_match(inst_code, "8..f")) UNKNOWN;
      if (pattern_match(inst_code, "9..f")) UNKNOWN;
      if (pattern_match(inst_code, "a..f")) UNKNOWN;
      if (pattern_match(inst_code, "b..f")) UNKNOWN;
      if (pattern_match(inst_code, "c..f")) UNKNOWN;
      if (pattern_match(inst_code, "d..f")) INST("if %s < %s $pc %s", FREG_B, FREG_A, branch_target(field_e));
      if (pattern_match(inst_code, "e..f")) INST("if %s >= %s $pc %s", FREG_B, FREG_A, branch_target(field_e));

      if (pattern_match(inst_code, ".f.f")) INST("if %s[%d] == 1 $pc %s", REG_A, FIELD_C, branch_target(field_e));
      if (pattern_match(inst_code, "..ff")) INST("if %s[%d] == 0 $pc %s", REG_B, FIELD_C, branch_target(field_e));
    }
  if (length == 2)
    {
      fpr(stream, "unknown instruction 0x%04x", inst_code);
    }
  else
    {
      fpr(stream, "unknown instruction 0x%04x 0x%08x", inst_code, field_e);
    }
  return length;
}
