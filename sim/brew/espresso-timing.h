/* Cycle model of the Espresso core (--timing=espresso).

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

#ifndef ESPRESSO_TIMING_H
#define ESPRESSO_TIMING_H

#include <stdbool.h>
#include <stdint.h>

struct espresso_timing;

/* What the functional simulator did with one instruction, in program order.  */
typedef struct
{
  uint32_t pc;          /* address of the instruction */
  uint32_t next_pc;     /* address of the next instruction executed */
  uint8_t len;          /* length in bytes: 2, 4 or 6 */
  bool taken;           /* control left the straight line (branch, call, exception) */
  bool has_mem;         /* the instruction accessed data memory... */
  uint32_t mem_addr;    /* ...at this address */
} espresso_timing_insn;

/* Statistics, all in cycles except INSNS.  */
typedef struct
{
  uint64_t cycles;          /* total, until the last instruction entered execute */
  uint64_t insns;           /* instructions that entered execute */
  uint64_t stall_branch;    /* refill after a taken branch, call or return */
  uint64_t stall_fetch;     /* decode had no instruction: fetch couldn't keep up */
  uint64_t stall_dep;       /* waiting for a register written by an earlier instruction */
  uint64_t stall_mem;       /* execute waiting for a load, a store or the bus */
  uint64_t stall_mult;      /* the cycle a multiply blocks the next instruction */
  uint64_t fetch_beats;     /* 16-bit words fetched, wrong path included */
  uint64_t fetch_dropped;   /* fetched words thrown away by a taken branch */
  uint64_t mem_beats;       /* 16-bit data transfers */
  uint64_t refresh;         /* DRAM refresh cycles */
  uint64_t resyncs;         /* times the model's instruction stream lost the functional one */
} espresso_timing_stats;

/* Creates a model.  READ_INSN_WORD reads a 16-bit instruction word at a
   (virtual) address without side effects, returning 0 for unmapped memory.
   With ESPRESSO_MEMORY_MAP, addresses decode as on Espresso (bits 27..26 zero
   select the slow 8-bit non-DRAM bus, bits 31..28 give its wait states);
   otherwise all memory is zero-wait-state DRAM.  */
extern struct espresso_timing *espresso_timing_new (uint16_t (*read_insn_word) (void *ctx, uint32_t addr),
                                                     void *ctx, bool espresso_memory_map);
extern void espresso_timing_free (struct espresso_timing *t);

/* Feeds the next executed instruction and runs the model until it enters
   execute.  Returns the cycle it entered execute (the cycle numbers the
   Espresso RTL test rig prints for 'Exec' events).  */
extern uint64_t espresso_timing_step (struct espresso_timing *t, const espresso_timing_insn *insn);

/* Sets where the instruction last fed went next, once the functional
   simulator knows (after an exception).  */
extern void espresso_timing_set_next_pc (struct espresso_timing *t, uint32_t pc, uint32_t next_pc);

extern const espresso_timing_stats *espresso_timing_get_stats (struct espresso_timing *t);

#endif
