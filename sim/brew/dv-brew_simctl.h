/* Simulator control device.

   Copyright (C) 2026 Free Software Foundation, Inc.

   This file is part of simulators.

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

#pragma once

// Lets bare-metal programs in the operating environment end the simulation
// with an exit status, the way exit() does through a syscall in the user
// environment. By default it sits at 0x20200 (--simctl-base), the address of
// GPIO4 on the Anachron FPGA system: GPIO4 isn't wired to pins, and the RTL
// test bench (anachron/rtl/espresso/anacron_fpga/sim_top.sv) ends its
// simulation on a write to it, so the same store ends both simulations. The
// RTL takes the written byte as the exit code (an 8-bit register); this
// device takes the whole written value.
//
// Registers (little-endian; 8-, 16- and 32-bit accesses at offset 0):
//   EXIT (offset 0, write): end the simulation; the written value is the exit
//                           status ('run' returns its low 8 bits).
//   EXIT (offset 0, read):  reads as SIMCTL_ID, so programs can detect the
//                           device.
#define SIMCTL_EXIT_REG_OFS  0x0
#define SIMCTL_SIZE          4

#define SIMCTL_ID            0x42524557 /* "BREW" */
