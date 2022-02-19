/* Definitions for decoding the brew opcode table.
   Copyright (C) 2009-2021 Free Software Foundation, Inc.
   Contributed by Andras Tantos.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#define BREW_REG_PC 0x10
#define BREW_REG_SP 0x0 /* R1 is of course a general purpose register, but the ABI defines it as SP, so let's have an alias for that */
#define BREW_REG_FP 0x1 /* R2 is of course a general purpose register, but the ABI defines it as FP, so let's have an alias for that */
#define BREW_REG_R(x) (x)

#define BREW_REG_TPC 0x11 /* Special register index for TPC. Not used in encoding as is all the time, but returned by the register name parser. */

#define BREW_REG_GP_MASK 0xf
#define BREW_REG_PC_RELATED 0x10
#define BREW_REG_BASE_MASK  0x1f

#define BREW_IS_GP_REG(r) (((r) & BREW_REG_BASE_MASK) == ((r) & BREW_REG_GP_MASK))
#define BREW_IS_PC_RELATED_REG(r) (((r) & BREW_REG_PC_RELATED) != 0)

#define BREW_REG_FLAG_MASK 0xf000
#define BREW_REG_FLAG_FLOAT 0x2000 /* Used to mark register operands for F0...FE. These are aliases for R0...RE for floating point operations */
#define BREW_REG_FLAG_SIGNED 0x4000 /* Used to mark register operands for S0...SE. These are aliases for R0...RE for signed operations */
