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

#if __HAVE_FLOAT16 == 0
#define _Float16 uint16_t
#endif

typedef struct {
  uint32_t addr;
  int access_size;
  bool is_write;
  uint32_t value;
  bool is_valid;
} mem_trace_s;


// forward-declare our struct. This is going to be only valid for simulation contexts, but this header is used everywhere.
struct _sim_cpu;
struct brew_model_functions;

struct _sim_cpu {
  sim_cpu_base base;
  brew_sim_state sim_state;
  char decode_buf[255];
  mem_trace_s mem_trace;
  void *model_info; // pointer to model-specific data
  // This is a giant hack, but I can't solve this: we need to squirrel away all the command-line options before we know what the model is
  // going to be, so neither sim_state.model_functions.XXX nor model_info is filled in.
  const char *rom_file_name;
};

// sim_engine_halt normally tries to set the PC to whatever
// is passed in. We don't implement 'sim_pc_set' (brew_pc_set asserts).
// On top of that, we never actually try to set the PC to
// something else then what it already is, so we don't need
// this hook.
#undef SIM_ENGINE_HALT_HOOK
#define SIM_ENGINE_HALT_HOOK(SD, LAST_CPU, CIA)

#endif
