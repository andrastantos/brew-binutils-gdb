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
#include "opcode/brew.h"

typedef struct {
  uint32_t addr;
  int access_size;
  bool is_write;
  uint32_t value;
  bool is_valid;
} mem_trace_s;
struct _sim_cpu {
  sim_cpu_base base;
  brew_sim_state sim_state;
  char decode_buf[255];
  mem_trace_s mem_trace;
};

// sim_engine_halt normally tries to set the PC to whatever
// is passed in. We don't implement 'sim_pc_set' (brew_pc_set asserts).
// On top of that, we never actually try to set the PC to
// something else then what it already is, so we don't need
// this hook.
#undef SIM_ENGINE_HALT_HOOK
#define SIM_ENGINE_HALT_HOOK(SD, LAST_CPU, CIA)

#endif
