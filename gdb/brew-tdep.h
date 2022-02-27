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
#include "opcode/brew_abi.h"

struct brew_gdbarch_tdep : gdbarch_tdep
{
  /* gdbarch target dependent data here.  Currently unused for BREW.  */
};

#endif /* BREW_TDEP_H */
