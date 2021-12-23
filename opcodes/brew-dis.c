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

#define REG_D (reg_names[FIELD_D])
#define REG_A (reg_names[FIELD_A])
#define REG_B (reg_names[FIELD_B])

static const char * reg_names[16] =
  { "$pc", "$r1",  "$r2",  "$r3",  "$r4", "$r5", "$r6",  "$r7", 
    "$r8", "$r9", "$r10", "$r11", "$r12", "$r13" "$r14", "<<<INVALID>>>"};

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

#define ERR_RETURN { info->memory_error_func(0, addr, info); }

int
print_insn_brew (bfd_vma addrP, struct disassemble_info * infoP)
{
  int length = 2;
  uint16_t inst_code;
  uint32_t field_e;

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
  }
  switch (FIELD_C)
    {
    case 0x0: /* ^ and special */
      switch (inst_code)
        {
          case 0x0000:
            fpr(stream, "FILL");
            return length;
          case 0x0110:
            fpr(stream, "BREAK");
            return length;
          case 0x0220:
            fpr(stream, "SYSCALL");
            return length;
          case 0x0330:
            fpr(stream, "STU");
            return length;
          case 0x0440:
            fpr(stream, "SII");
            return length;
          case 0x0dd0:
            fpr(stream, "FENCE");
            return length;
          case 0x0ee0:
            fpr(stream, "WFENCE");
            return length;
        }
      if ((inst_code & 0xfff0) == 0x0ff0)
        {
          /* load immedate */
          fpr(stream, "%s <- %d", REG_D, field_e);
        }
      break;
    case 0x1:
    break;
    case 0x2:
    break;
    case 0x3:
    break;
    case 0x4:
    break;
    case 0x5:
    break;
    case 0x6:
    break;
    case 0x7:
    break;
    case 0x8:
    break;
    case 0x9:
    break;
    case 0xa:
    break;
    case 0xb:
    break;
    case 0xc:
    break;
    case 0xd:
    break;
    case 0xe:
    break;
    case 0xf:
    break;
    default:
      OPCODES_ASSERT(false);
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
