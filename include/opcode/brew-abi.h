/* Definitions for properly handling ABI aspects of brew.
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
#ifndef __OPCODE_BREW_ABI_H__
#define __OPCODE_BREW_ABI_H__

// NOTE: these register defines need to be kept in sync with the ones in brew-decode.c.
//       An array there called `brew_reg_names` defines the actual names.
//       Also, GCC sources have their own definition of ABI constants, which needs to be
//       kept in sync with the ones here. In fact, one could argue that the ABI is defined
//       *by* the GCC sources.

#define BREW_REG_THUNK           0
#define BREW_REG_STRUCT_VAL_ADDR 1
#define BREW_REG_STATIC_CHAIN    2
#define BREW_REG_ARG0            4
#define BREW_REG_ARG1            5
#define BREW_REG_ARG2            6
#define BREW_REG_ARG3            7
#define BREW_REG_EH_RETURN_DATA0 8 // TODO: change this to the first call-saved register - TODO: this is done, but why was that important ???
#define BREW_REG_EH_RETURN_DATA1 9
#define BREW_REG_FP              12
#define BREW_REG_SP              13
#define BREW_REG_LINK            14
#define BREW_REG_SYSCALL_ERRNO   14

extern const char * brew_reg_names[];

#endif // __OPCODE_BREW_ABI_H__
