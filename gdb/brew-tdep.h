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

/* TODO: At this point, this is just the moxie debugger, renamed. Obviously that's wrong */

#ifndef BREW_TDEP_H
#define BREW_TDEP_H

#include "gdbarch.h"

struct brew_gdbarch_tdep : gdbarch_tdep
{
  /* gdbarch target dependent data here.  Currently unused for BREW.  */
};

enum brew_regnum
{
  BREW_FP_REGNUM = 0,
  BREW_SP_REGNUM = 1,
  R0_REGNUM = 2,
  R1_REGNUM = 3,
  BREW_PC_REGNUM = 16,
  BREW_CC_REGNUM = 17,
  RET1_REGNUM = R0_REGNUM,
  ARG1_REGNUM = R0_REGNUM,
  ARGN_REGNUM = R1_REGNUM,
};

#define BREW_NUM_REGS 18

#endif /* BREW_TDEP_H */
