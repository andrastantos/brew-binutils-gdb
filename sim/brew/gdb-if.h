/* Brew Simulator GDB interface definition.
   Copyright (C) 2009-2021 Free Software Foundation, Inc.
   Contributed by Anthony Green <green@moxielogic.com>,
    modified by Andras Tantos

This file is part of GDB, the GNU debugger.

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

#ifndef __SIM_BREW_GDB_IF_H__
#define __SIM_BREW_GDB_IF_H__

// Most registers are seen by the same index in both the processor (simulator)
// and in GDB. The two PCs are handled differently though. They are indexed
// as registers normal by GDB while they're special-cased in the simulator.
#define BREW_GDB_REG_TPC 15
#define BREW_GDB_REG_SPC 16

// Total number of registers as seen by GDB.
#define BREW_GDB_NUM_REGS 17

#endif
