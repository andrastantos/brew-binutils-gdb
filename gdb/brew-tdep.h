/* Target-dependent code for brew
 
   Copyright (C) 2009-2021 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef BREW_TDEP_H
#define BREW_TDEP_H

#include "gdbarch.h"

struct brew_gdbarch_tdep : gdbarch_tdep
{
  /* gdbarch target dependent data here.  Currently unused for BREW.  */
};

enum brew_regnum
{
  R0_REGNUM = 0,
  R1_REGNUM = 1,
  R2_REGNUM = 2,
  R3_REGNUM = 3,
  R4_REGNUM = 4,
  R5_REGNUM = 5,
  R6_REGNUM = 6,
  R7_REGNUM = 7,
  R8_REGNUM = 8,
  R9_REGNUM = 9,
  R10_REGNUM = 10,
  R11_REGNUM = 11,
  R12_REGNUM = 12,
  R13_REGNUM = 13,
  R14_REGNUM = 14,
  BREW_PC_REGNUM = 0,
  BREW_SP_REGNUM = 1,
  BREW_FP_REGNUM = 2,
  RET1_REGNUM = R4_REGNUM,
  RETN_REGNUM = R7_REGNUM,
  ARG1_REGNUM = R4_REGNUM,
  ARGN_REGNUM = R7_REGNUM,
};

#define BREW_NUM_REGS 14

#endif /* BREW_TDEP_H */
