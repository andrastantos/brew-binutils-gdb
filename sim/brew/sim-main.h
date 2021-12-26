/* Brew Simulator definition.
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

/* TODO: At this point, this is just the moxie simulator, renamed. Obviously that's wrong */

#ifndef SIM_MAIN_H
#define SIM_MAIN_H

#include "sim-basics.h"
#include "sim-base.h"
#include "bfd.h"
#include <stdint.h>

/* We will implement the following register layout:
   0 - *current context* pc
   1..14 - $r1...$r14
   15 - *other context* pc
*/

#define BREW_NUM_REGS 17 /* Including TPC in slot 15 and next_pc in slot 16 */
#define BREW_REG_PC 0
#define BREW_REG_OTHER_PC 15
#define BREW_REG_NEXT_PC 16

/* TODO: there's a whole slew of inter-dependencies between the simulator and GDB
   GDB has a brew-tdep.h brew-tdep.c file-set that contains all/most of this.

   For instance, register names and their numbering, even some calling conversion
   stuff if in there */
struct _sim_cpu {
  sim_cpu_base base;

  /* The following are internal simulator state variables: */
  uint32_t regs[BREW_NUM_REGS];
  bool regs_touch[BREW_NUM_REGS]; /* Set to true every time a register is modified to help tracing */
  bool is_task_mode;

};

#endif
