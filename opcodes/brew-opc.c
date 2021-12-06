/* brew-opc.c -- Definitions for brew opcodes.
   Copyright (C) 2009-2021 Free Software Foundation, Inc.
   Contributed by Andras Tantos.

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
   along with this file; see the file COPYING.  If not, write to the
   Free Software Foundation, 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include <stdint.h>
#include "sysdep.h"
#include "opcode/brew.h"

/* TODO: right now this is not changed from the moxie baseline at all. It should be!!!! */

/* The moxie processor's 16-bit instructions come in two forms:

  FORM 1 instructions start with a 0 bit...

    0oooooooaaaabbbb
    0              F

   ooooooo - form 1 opcode number
   aaaa    - operand A
   bbbb    - operand B

  FORM 2 instructions start with bits "10"...

    10ooaaaavvvvvvvv
    0              F

   oo       - form 2 opcode number
   aaaa     - operand A
   vvvvvvvv - 8-bit immediate value

  FORM 3 instructions start with a bits "11"...

    11oooovvvvvvvvvv
    0              F

   oooo         - form 3 opcode number
   vvvvvvvvvv   - 10-bit immediate value.  */

const brew_opc_info_t brew_form1_opc_info[128] =
  {
    { 0x00, BREW_BAD,     "bad" },  /* Reserved as bad.  */
    { 0x01, BREW_F1_A4,   "ldi.l" },
    { 0x02, BREW_F1_AB,   "mov" },
    { 0x03, BREW_F1_M,    "jsra" },
    { 0x04, BREW_F1_NARG, "ret" },
    { 0x05, BREW_F1_AB,   "add" },
    { 0x06, BREW_F1_AB,   "push" },
    { 0x07, BREW_F1_AB,   "pop" },
    { 0x08, BREW_F1_A4,   "lda.l" },
    { 0x09, BREW_F1_4A,   "sta.l" },
    { 0x0a, BREW_F1_ABi,  "ld.l" },
    { 0x0b, BREW_F1_AiB,  "st.l" },
    { 0x0c, BREW_F1_ABi2, "ldo.l" },
    { 0x0d, BREW_F1_AiB2, "sto.l" },
    { 0x0e, BREW_F1_AB,   "cmp" },
    { 0x0f, BREW_F1_NARG, "nop" },
    { 0x10, BREW_F1_AB,   "sex.b" },
    { 0x11, BREW_F1_AB,   "sex.s" },
    { 0x12, BREW_F1_AB,   "zex.b" },
    { 0x13, BREW_F1_AB,   "zex.s" },
    { 0x14, BREW_F1_AB,   "umul.x" },
    { 0x15, BREW_F1_AB,   "mul.x" },
    { 0x16, BREW_BAD,     "bad" },
    { 0x17, BREW_BAD,     "bad" },
    { 0x18, BREW_BAD,     "bad" },
    { 0x19, BREW_F1_A,    "jsr" },
    { 0x1a, BREW_F1_M,    "jmpa" },
    { 0x1b, BREW_F1_A4,   "ldi.b" },
    { 0x1c, BREW_F1_ABi,  "ld.b" },
    { 0x1d, BREW_F1_A4,   "lda.b" },
    { 0x1e, BREW_F1_AiB,  "st.b" },
    { 0x1f, BREW_F1_4A,   "sta.b" },
    { 0x20, BREW_F1_A4,   "ldi.s" },
    { 0x21, BREW_F1_ABi,  "ld.s" },
    { 0x22, BREW_F1_A4,   "lda.s" },
    { 0x23, BREW_F1_AiB,  "st.s" },
    { 0x24, BREW_F1_4A,   "sta.s" },
    { 0x25, BREW_F1_A,    "jmp" },
    { 0x26, BREW_F1_AB,   "and" },
    { 0x27, BREW_F1_AB,   "lshr" },
    { 0x28, BREW_F1_AB,   "ashl" },
    { 0x29, BREW_F1_AB,   "sub" },
    { 0x2a, BREW_F1_AB,   "neg" },
    { 0x2b, BREW_F1_AB,   "or" },
    { 0x2c, BREW_F1_AB,   "not" },
    { 0x2d, BREW_F1_AB,   "ashr" },
    { 0x2e, BREW_F1_AB,   "xor" },
    { 0x2f, BREW_F1_AB,   "mul" },
    { 0x30, BREW_F1_4,    "swi" },
    { 0x31, BREW_F1_AB,   "div" },
    { 0x32, BREW_F1_AB,   "udiv" },
    { 0x33, BREW_F1_AB,   "mod" },
    { 0x34, BREW_F1_AB,   "umod" },
    { 0x35, BREW_F1_NARG, "brk" },
    { 0x36, BREW_F1_ABi2, "ldo.b" },
    { 0x37, BREW_F1_AiB2, "sto.b" },
    { 0x38, BREW_F1_ABi2, "ldo.s" },
    { 0x39, BREW_F1_AiB2, "sto.s" },
    { 0x3a, BREW_BAD,     "bad" },
    { 0x3b, BREW_BAD,     "bad" },
    { 0x3c, BREW_BAD,     "bad" },
    { 0x3d, BREW_BAD,     "bad" },
    { 0x3e, BREW_BAD,     "bad" },
    { 0x3f, BREW_BAD,     "bad" },
    { 0x40, BREW_BAD,     "bad" },
    { 0x41, BREW_BAD,     "bad" },
    { 0x42, BREW_BAD,     "bad" },
    { 0x43, BREW_BAD,     "bad" },
    { 0x44, BREW_BAD,     "bad" },
    { 0x45, BREW_BAD,     "bad" },
    { 0x46, BREW_BAD,     "bad" },
    { 0x47, BREW_BAD,     "bad" },
    { 0x48, BREW_BAD,     "bad" },
    { 0x49, BREW_BAD,     "bad" },
    { 0x4a, BREW_BAD,     "bad" },
    { 0x4b, BREW_BAD,     "bad" },
    { 0x4c, BREW_BAD,     "bad" },
    { 0x4d, BREW_BAD,     "bad" },
    { 0x4e, BREW_BAD,     "bad" },
    { 0x4f, BREW_BAD,     "bad" },
    { 0x50, BREW_BAD,     "bad" },
    { 0x51, BREW_BAD,     "bad" },
    { 0x52, BREW_BAD,     "bad" },
    { 0x53, BREW_BAD,     "bad" },
    { 0x54, BREW_BAD,     "bad" },
    { 0x55, BREW_BAD,     "bad" },
    { 0x56, BREW_BAD,     "bad" },
    { 0x57, BREW_BAD,     "bad" },
    { 0x58, BREW_BAD,     "bad" },
    { 0x59, BREW_BAD,     "bad" },
    { 0x5a, BREW_BAD,     "bad" },
    { 0x5b, BREW_BAD,     "bad" },
    { 0x5c, BREW_BAD,     "bad" },
    { 0x5d, BREW_BAD,     "bad" },
    { 0x5e, BREW_BAD,     "bad" },
    { 0x5f, BREW_BAD,     "bad" },
    { 0x60, BREW_BAD,     "bad" },
    { 0x61, BREW_BAD,     "bad" },
    { 0x62, BREW_BAD,     "bad" },
    { 0x63, BREW_BAD,     "bad" },
    { 0x64, BREW_BAD,     "bad" },
    { 0x65, BREW_BAD,     "bad" },
    { 0x66, BREW_BAD,     "bad" },
    { 0x67, BREW_BAD,     "bad" },
    { 0x68, BREW_BAD,     "bad" },
    { 0x69, BREW_BAD,     "bad" },
    { 0x6a, BREW_BAD,     "bad" },
    { 0x6b, BREW_BAD,     "bad" },
    { 0x6c, BREW_BAD,     "bad" },
    { 0x6d, BREW_BAD,     "bad" },
    { 0x6e, BREW_BAD,     "bad" },
    { 0x6f, BREW_BAD,     "bad" },
    { 0x70, BREW_BAD,     "bad" },
    { 0x71, BREW_BAD,     "bad" },
    { 0x72, BREW_BAD,     "bad" },
    { 0x73, BREW_BAD,     "bad" },
    { 0x74, BREW_BAD,     "bad" },
    { 0x75, BREW_BAD,     "bad" },
    { 0x76, BREW_BAD,     "bad" },
    { 0x77, BREW_BAD,     "bad" },
    { 0x78, BREW_BAD,     "bad" },
    { 0x79, BREW_BAD,     "bad" },
    { 0x7a, BREW_BAD,     "bad" },
    { 0x7b, BREW_BAD,     "bad" },
    { 0x7c, BREW_BAD,     "bad" },
    { 0x7d, BREW_BAD,     "bad" },
    { 0x7e, BREW_BAD,     "bad" },
    { 0x7f, BREW_BAD,     "bad" }
  };

const brew_opc_info_t brew_form2_opc_info[4] =
  {
    { 0x00, BREW_F2_A8V,  "inc" },
    { 0x01, BREW_F2_A8V,  "dec" },
    { 0x02, BREW_F2_A8V,  "gsr" },
    { 0x03, BREW_F2_A8V,  "ssr" }
  };

const brew_opc_info_t brew_form3_opc_info[16] =
  {
    { 0x00, BREW_F3_PCREL,"beq" },
    { 0x01, BREW_F3_PCREL,"bne" },
    { 0x02, BREW_F3_PCREL,"blt" },
    { 0x03, BREW_F3_PCREL,"bgt" },
    { 0x04, BREW_F3_PCREL,"bltu" },
    { 0x05, BREW_F3_PCREL,"bgtu" },
    { 0x06, BREW_F3_PCREL,"bge" },
    { 0x07, BREW_F3_PCREL,"ble" },
    { 0x08, BREW_F3_PCREL,"bgeu" },
    { 0x09, BREW_F3_PCREL,"bleu" },
    { 0x0a, BREW_BAD,     "bad" },
    { 0x0b, BREW_BAD,     "bad" },
    { 0x0c, BREW_BAD,     "bad" },
    { 0x0d, BREW_BAD,     "bad" },
    { 0x0e, BREW_BAD,     "bad" },
    { 0x0f, BREW_BAD,     "bad" }  /* Reserved as bad.  */
  };
