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

#include "disassemble.h"
#include "opcode/brew.h"

static bool get_uint16(uint16_t *buffer, struct disassemble_info * info, bfd_vma addr)
{
  int status;

  if ((status = info->read_memory_func(addr, (bfd_byte *)buffer, 2, info)))
    {
      info->memory_error_func (status, addr, info);
      return false;
    }
  *buffer = bfd_getl16(buffer);
  return true;
}

static bool get_uint32(uint32_t *buffer, struct disassemble_info * info, bfd_vma addr)
{
  int status;

  if ((status = info->read_memory_func(addr, (bfd_byte *)buffer, 4, info)))
    {
      info->memory_error_func (status, addr, info);
      return false;
    }
  *buffer = bfd_getl32(buffer);
  return true;
}

int
print_insn_brew (bfd_vma addrP, struct disassemble_info * infoP)
{
  int length;
  uint16_t inst_code;
  uint32_t field_e;
  uint16_t field_e16;
  fprintf_ftype fpr;
  void *stream;
  bfd_vma addr = addrP;

  OPCODES_ASSERT(infoP->endian == BFD_ENDIAN_LITTLE);

  stream = infoP->stream;
  fpr = infoP->fprintf_func;

  if (!get_uint16(&inst_code, infoP, addr))
    return -1;
  addr += 2;

  length = brew_inst_len(inst_code);
  switch (length)
    {
    case 2:
      field_e = 0;
      break;
    case 4: 
      if (!get_uint16(&field_e16, infoP, addr))
        return -1;
      field_e = (int32_t)field_e16 << 16 >> 16; // sign-extend
      break;
    case 6: 
      if (!get_uint32(&field_e, infoP, addr))
        return -1;
      break;
    default:
      OPCODES_ASSERT(false);
    }

  addr += length-2;
  brew_print_insn(fpr, stream, inst_code, field_e);
  return length;
}
