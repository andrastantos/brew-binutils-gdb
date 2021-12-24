/* Simulator for the brew processor
   Copyright (C) 2008-2021 Free Software Foundation, Inc.
   Contributed by Anthony Green, modified by Andras Tantos

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

/* This must come before any other includes.  */
#include "defs.h"

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include "bfd.h"
#include "libiberty.h"
#include "sim/sim.h"

#include "sim-main.h"
#include "sim-base.h"
#include "sim-options.h"
#include "sim-io.h"
#include "sim-signal.h"
#include "target-newlib-syscall.h"





/* Macros to extract operands from the instruction word.  */
#define FIELD_C ((inst_code >> 12) & 0xf)
#define FIELD_B ((inst_code >> 8) & 0xf)
#define FIELD_A ((inst_code >> 4) & 0xf)
#define FIELD_D ((inst_code >> 0) & 0xf)

#define GET_NIBBLE(i, nibble) ((i >> (nibble*4)) & 0xf)

static bool hexdigit(char digit, int *val)
{
  if (digit >= '0' && digit <= '9')
    {
      *val = digit - '0';
      return true;
    }
  if (digit >= 'a' && digit <= 'f')
    {
      *val = digit - 'a' + 10;
      return true;
    }
  if (digit >= 'A' && digit <= 'F')
    {
      *val = digit - 'A' + 10;
      return true;
    }
  return false;
}

static bool pattern_match(uint16_t inst_code, const char *pattern)
{
  const char *p;
  int nibble;
  int hex_p;
  for (p=pattern, nibble=3;nibble >=0;--nibble, ++p)
    {
      if (*p == '.')
        {
          if (GET_NIBBLE(inst_code, nibble) == 0xf)
            {
              return false;
            }
          continue;
        }
      if (!hexdigit(*p, &hex_p))
        {
          return false;
        }
      if (hex_p != GET_NIBBLE(inst_code, nibble))
        {
          return false;
        }
    }
    return true;
}

static const char * reg_names[16] =
  { "$pc", "$r1", "$r2",  "$r3",  "$r4",  "$r5",  "$r6",  "$r7", 
    "$r8", "$r9", "$r10", "$r11", "$r12", "$r13", "$r14", "<<<INVALID>>>"};



/* The machine state.

   This state is maintained in host byte order.  The fetch/store
   register functions must translate between host byte order and the
   target processor byte order.  Keeping this data in target byte
   order simplifies the register read/write functions.  Keeping this
   data in native order improves the performance of the simulator.
   Simulation speed is deemed more important.  */

static void
set_initial_gprs (sim_cpu *scpu)
{
  int i;
  long space;
  
  scpu->regs[BREW_REG_PC] = 0;
  scpu->is_task_mode = true;
  /* FIXME: do we want to get some other state in the registers? */
  /* Such as:
     - explicit randomization
     - results of POST
     - version/revision info

     Also: we will want to start in scheduler mode for a system simulation.
  */
}

static INLINE void write_uint8 (sim_cpu *scpu, uint32_t addr, uint8_t  val) { sim_core_write_aligned_1(scpu, CPU_PC_GET(scpu), write_map, addr, val); }
static INLINE void write_uint16(sim_cpu *scpu, uint32_t addr, uint16_t val) { sim_core_write_aligned_2(scpu, CPU_PC_GET(scpu), write_map, addr, val); }
static INLINE void write_uint32(sim_cpu *scpu, uint32_t addr, uint32_t val) { sim_core_write_aligned_4(scpu, CPU_PC_GET(scpu), write_map, addr, val); }

static INLINE uint8_t  read_uint8 (sim_cpu *scpu, uint32_t addr) { return sim_core_read_aligned_1(scpu, CPU_PC_GET(scpu), read_map, addr); }
static INLINE uint16_t read_uint8 (sim_cpu *scpu, uint32_t addr) { return sim_core_read_aligned_2(scpu, CPU_PC_GET(scpu), read_map, addr); }
static INLINE uint32_t read_uint8 (sim_cpu *scpu, uint32_t addr) { return sim_core_read_aligned_4(scpu, CPU_PC_GET(scpu), read_map, addr); }

#define CHECK_FLAG(T,H) if (tflags & T) { hflags |= H; tflags ^= T; }

static unsigned int
convert_target_flags (unsigned int tflags)
{
  unsigned int hflags = 0x0;

  CHECK_FLAG(0x0001, O_WRONLY);
  CHECK_FLAG(0x0002, O_RDWR);
  CHECK_FLAG(0x0008, O_APPEND);
  CHECK_FLAG(0x0200, O_CREAT);
  CHECK_FLAG(0x0400, O_TRUNC);
  CHECK_FLAG(0x0800, O_EXCL);
  CHECK_FLAG(0x2000, O_SYNC);

  if (tflags != 0x0)
    fprintf (stderr, 
             "Simulator Error: problem converting target open flags for host.  0x%x\n", 
             tflags);

  return hflags;
}

/* TODO: Split this up into finger trace levels than just insn.  */
static INLINE void brew_trace(sim_cpu *scpu, const char *message)
{
  if (message == NULL) message = "---";
  TRACE_INSN(
    scpu,
    "%s, %cPC: 0x%x, %s, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, (%cPC: 0x%x)",
    scpu->is_task_mode?"TSK":"SCH", 
    scpu->is_task_mode?"T":"S", scpu->regs[0],
    message, 
    scpu->regs[0], scpu->regs[1], scpu->regs[2], scpu->regs[3], scpu->regs[4], scpu->regs[5], 
    scpu->regs[6], scpu->regs[7], scpu->regs[8], scpu->regs[9], scpu->regs[10], 
    scpu->regs[11], scpu->regs[12], scpu->regs[13], scpu->regs[14],
    scpu->is_task_mode?"S":"T", scpu->regs[15]);
}
#define BREW_TRACE_INSN(message) brew_trace(scpu, message)

#define UNKNOWN { \
  char inst_str[25]; \
  if (length == 2) \
    sprintf(inst_str, "UKN 0x%04x", inst_code); \
  else \
    sprintf(inst_str, "UKN 0x%04x 0x%08x", inst_code, field_e); \
  BREW_TRACE_INSN("inst_str"); \
  sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); \
}

#define REG_D_TARGET scpu->regs[FIELD_D == BREW_REG_PC ? BREW_REG_NEXT_PC : FIELD_D]
#define REG_TPC_TARGET scpu->regs[scpu->is_task_mode ? BREW_REG_NEXT_PC : BREW_REG_OTHER_PC]

#define REG_D (scpu->regs[FIELD_D] & 0xffffffff) /* simple trick to ensure that this field is invalid on the LHS */
#define REG_A (scpu->regs[FIELD_A] & 0xffffffff) /* simple trick to ensure that this field is invalid on the LHS */
#define REG_B (scpu->regs[FIELD_B] & 0xffffffff) /* simple trick to ensure that this field is invalid on the LHS */
#define REG_TPC (scpu->regs[scpu->is_task_mode ? BREW_REG_PC : BREW_REG_OTHER_PC] & 0xffffffff) /* simple trick to ensure that this field is invalid on the LHS */

void
sim_engine_run (SIM_DESC sd,
                int next_cpu_nr, /* the CPU to run  */
                int nr_cpus, /* ignore  */
                int siggnal) /* ignore  */
{
  uint32_t pc;
  uint16_t inst_code;
  uint32_t field_e;
  float field_e_as_float;
  int length;
  sim_cpu *scpu = STATE_CPU (sd, next_cpu_nr);
  
  /* Run instructions here. */
  do 
    {
      {
        /* scoping pc to make sure we don't even accidentally use that for anything */
        uint32_t pc = CPU_PC_GET (scpu) & ~1; /* PC is always 16-bit aligned, the LSB is ignored */

        /* Fetch the instruction at pc.  */
        length = 2;
        inst_code = sim_core_read_aligned_2(scpu, pc, exec_map, pc);

        /* Determine if we need field_e and read it if we do */
        if (FIELD_D == 0xf || FIELD_B == 0xf || FIELD_A == 0xf || (inst_code >> 11) == 0x1f) {
          length = 6;
          field_e = sim_core_read_unaligned_4(scpu, pc, exec_map, pc+2);
          memcpy(&field_e_as_float, &field_e, 4);
          //DEBUG("0x%08x, 0x%08x", *(int*)x, field_e);
        }
        scpu->regs[BREW_REG_NEXT_PC] = pc + length; /* Update PC to the next instruction. It's possible that the instruction itself is a branch, in which case it'll be overwritten */
      }
      switch (FIELD_C)
        {
        case 0x0: /* ^ and special */
          if (pattern_match(inst_code, "0000")) { BREW_TRACE_INSN("FILL"); sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); }
          if (pattern_match(inst_code, "0110")) { BREW_TRACE_INSN("BREAK"); sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGTRAP); }
          if (pattern_match(inst_code, "0220")) { BREW_TRACE_INSN("SYSCALL"); sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); } /* We should simulate syscalls here */
          if (pattern_match(inst_code, "0330")) { BREW_TRACE_INSN("STU"); } /* Return to task mode --> NOP in task mode */
          if (pattern_match(inst_code, "0440")) { BREW_TRACE_INSN("SII"); sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); }
          if (pattern_match(inst_code, "0dd0")) { BREW_TRACE_INSN("FENCE"); }
          if (pattern_match(inst_code, "0ee0")) { BREW_TRACE_INSN("WFENCE"); }
          if (pattern_match(inst_code, "0..0")) UNKNOWN;
          if (pattern_match(inst_code, "0ff.")) { REG_D_TARGET = field_e; BREW_TRACE_INSN("load immediate"); }
          if (pattern_match(inst_code, "0f..")) UNKNOWN;
          if (pattern_match(inst_code, "0.f.")) { REG_D_TARGET = REG_B ^ field_e; BREW_TRACE_INSN(NULL); }
          if (pattern_match(inst_code, "0...")) { REG_D_TARGET = REG_B ^ REG_A; BREW_TRACE_INSN(NULL); }
          break;
        case 0x1:
          if (pattern_match(inst_code, "1000")) { BREW_TRACE_INSN("WOI"); sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); }
          if (pattern_match(inst_code, "1f..")) UNKNOWN;
          if (pattern_match(inst_code, "1.f.")) INST("%s <- %s | %u", REG_D, REG_B, field_e);
          if (pattern_match(inst_code, "1...")) INST("%s <- %s | %s", REG_D, REG_B, REG_A);
          break;
        case 0x2:
          if (pattern_match(inst_code, "2f..")) UNKNOWN;
          if (pattern_match(inst_code, "2.f.")) INST("%s <- %s & %u", REG_D, REG_B, field_e);
          if (pattern_match(inst_code, "2...")) INST("%s <- %s & %s", REG_D, REG_B, REG_A);
          break;
        case 0x3:
          if (pattern_match(inst_code, "3f..")) INST("%s <- %u - %s", REG_D, field_e, REG_A);
          if (pattern_match(inst_code, "3.f.")) INST("%s <- %s - %u", REG_D, REG_B, field_e);
          if (pattern_match(inst_code, "3...")) INST("%s <- %s - %s", REG_D, REG_B, REG_A);
          break;
        case 0x4:
          if (pattern_match(inst_code, "4f..")) UNKNOWN;
          if (pattern_match(inst_code, "4.f.")) INST("%s <- %s + %u", REG_D, REG_B, field_e);
          if (pattern_match(inst_code, "4...")) INST("%s <- %s + %s", REG_D, REG_B, REG_A);
          break;
        case 0x5:
          if (pattern_match(inst_code, "5f..")) INST("%s <- %u << %s", REG_D, field_e, REG_A);
          if (pattern_match(inst_code, "5.f.")) INST("%s <- %s << %u", REG_D, REG_B, field_e);
          if (pattern_match(inst_code, "5...")) INST("%s <- %s << %s", REG_D, REG_B, REG_A);
          break;
        case 0x6:
          if (pattern_match(inst_code, "6f..")) INST("%s <- %u >> %s", REG_D, field_e, REG_A);
          if (pattern_match(inst_code, "6.f.")) INST("%s <- %s >> %u", REG_D, REG_B, field_e);
          if (pattern_match(inst_code, "6...")) INST("%s <- %s >> %s", REG_D, REG_B, REG_A);
          break;
        case 0x7:
          if (pattern_match(inst_code, "7f..")) INST("%s <- %d >> %s", SREG_D, (int32_t)field_e, REG_A);
          if (pattern_match(inst_code, "7.f.")) INST("%s <- %s >> %u", SREG_D, SREG_B, field_e);
          if (pattern_match(inst_code, "7...")) INST("%s <- %s >> %s", SREG_D, SREG_B, REG_A);
          break;
        case 0x8:
          if (pattern_match(inst_code, "8f..")) UNKNOWN;
          if (pattern_match(inst_code, "8ff.")) UNKNOWN;
          if (pattern_match(inst_code, "80f.")) INST("%s <- %u", TREG_D, field_e);
          if (pattern_match(inst_code, "8.f.")) INST("%s <- %s * %u", REG_D, REG_B, field_e);
          if (pattern_match(inst_code, "80..")) INST("%s <- %s", TREG_D, TREG_A);
          if (pattern_match(inst_code, "8.0.")) UNKNOWN;
          if (pattern_match(inst_code, "8...")) INST("%s <- %s * %s", REG_D, REG_B, REG_A);
          break;
        case 0x9:
          if (pattern_match(inst_code, "9f..")) UNKNOWN;
          if (pattern_match(inst_code, "9ff.")) UNKNOWN;
          if (pattern_match(inst_code, "90f.")) UNKNOWN;
          if (pattern_match(inst_code, "9.f.")) INST("%s <- %s * %d", SREG_D, SREG_B, (int32_t)field_e);
          if (pattern_match(inst_code, "90..")) INST("%s <- %s + 1", REG_D, REG_A);
          if (pattern_match(inst_code, "9.0.")) INST("%s <- %s - 1", REG_D, REG_B);
          if (pattern_match(inst_code, "9...")) INST("%s <- %s * %s", SREG_D, SREG_B, SREG_A);
          break;
        case 0xa:
          if (pattern_match(inst_code, "af..")) UNKNOWN;
          if (pattern_match(inst_code, "aff.")) UNKNOWN;
          if (pattern_match(inst_code, "a0f.")) UNKNOWN;
          if (pattern_match(inst_code, "a.f.")) INST("%s <- upper %s * %u", REG_D, REG_B, field_e);
          if (pattern_match(inst_code, "a0..")) INST("%s <- -%s", REG_D, REG_A);
          if (pattern_match(inst_code, "a.0.")) INST("%s <- ~%s", REG_D, REG_B);
          if (pattern_match(inst_code, "a...")) INST("%s <- upper %s * %s", REG_D, REG_B, REG_A);
          break;
        case 0xb:
          if (pattern_match(inst_code, "bf..")) UNKNOWN;
          if (pattern_match(inst_code, "bff.")) UNKNOWN;
          if (pattern_match(inst_code, "b0f.")) UNKNOWN;
          if (pattern_match(inst_code, "b.f.")) INST("%s <- %s * %d", SREG_D, SREG_B, (int32_t)field_e);
          if (pattern_match(inst_code, "b0..")) INST("%s <- bswap %s", REG_D, REG_A);
          if (pattern_match(inst_code, "b.0.")) INST("%s <- wswap %s", REG_D, REG_B);
          if (pattern_match(inst_code, "b...")) INST("%s <- %s * %s", SREG_D, SREG_B, SREG_A);
          break;
        case 0xc:
          if (pattern_match(inst_code, "cf..")) UNKNOWN;
          if (pattern_match(inst_code, "c0f.")) UNKNOWN;
          if (pattern_match(inst_code, "c.f0")) UNKNOWN;
          if (pattern_match(inst_code, "cff.")) UNKNOWN;
          if (pattern_match(inst_code, "c.f.")) INST("%s <- %s + %f", FREG_D, FREG_B, field_e_as_float);
          if (pattern_match(inst_code, "c..0")) UNKNOWN;
          if (pattern_match(inst_code, "c.0.")) INST("%s <- rsqrt %s", FREG_D, FREG_B);
          if (pattern_match(inst_code, "c0..")) INST("%s <- 1 / %s", FREG_D, FREG_A);
          if (pattern_match(inst_code, "c...")) INST("%s <- %s + %s", FREG_D, FREG_B, FREG_A);
          break;
        case 0xd:
          if (pattern_match(inst_code, "df..")) INST("%s <- %f - %s", FREG_D, field_e_as_float, FREG_A);
          if (pattern_match(inst_code, "d0f.")) UNKNOWN;
          if (pattern_match(inst_code, "d.f0")) UNKNOWN;
          if (pattern_match(inst_code, "dff.")) UNKNOWN;
          if (pattern_match(inst_code, "d.f.")) INST("%s <- %s - %f", FREG_D, FREG_B, field_e_as_float);
          if (pattern_match(inst_code, "d..0")) UNKNOWN;
          if (pattern_match(inst_code, "d.0.")) INST("%s <- %s", FREG_D, SREG_B);
          if (pattern_match(inst_code, "d0..")) INST("%s <- floor %s", SREG_D, FREG_A);
          if (pattern_match(inst_code, "d...")) INST("%s <- %s - %s", FREG_D, FREG_B, FREG_A);
          break;
        case 0xe:
          if (pattern_match(inst_code, "ef..")) UNKNOWN;
          if (pattern_match(inst_code, "e0f.")) UNKNOWN;
          if (pattern_match(inst_code, "e.f0")) UNKNOWN;
          if (pattern_match(inst_code, "eff.")) UNKNOWN;
          if (pattern_match(inst_code, "e.f.")) INST("%s <- %s * %f", FREG_D, FREG_B, field_e_as_float);
          if (pattern_match(inst_code, "e..0")) UNKNOWN;
          if (pattern_match(inst_code, "e.0.")) UNKNOWN;
          if (pattern_match(inst_code, "e0..")) UNKNOWN;
          if (pattern_match(inst_code, "e...")) INST("%s <- %s * %s", FREG_D, FREG_B, FREG_A);
          break;
        case 0xf:
          if (pattern_match(inst_code, "f0..")) INST("%s <- MEM8[%s]", SREG_D, REG_A);
          if (pattern_match(inst_code, "f1..")) INST("%s <- MEM8[%s]", REG_D, REG_A);
          if (pattern_match(inst_code, "f2..")) INST("%s <- MEM16[%s]", SREG_D, REG_A);
          if (pattern_match(inst_code, "f3..")) INST("%s <- MEM16[%s]", REG_D, REG_A);
          if (pattern_match(inst_code, "f4..")) INST("%s <- MEM32[%s]", REG_D, REG_A);
          if (pattern_match(inst_code, "f5..")) INST("MEM8[%s] <- %s", REG_A, REG_D);
          if (pattern_match(inst_code, "f6..")) INST("MEM16[%s] <- %s", REG_A, REG_D);
          if (pattern_match(inst_code, "f7..")) INST("MEM32[%s] <- %s", REG_A, REG_D);

          if (pattern_match(inst_code, "f8f.")) INST("%s <- MEM8[%d]", SREG_D, field_e);
          if (pattern_match(inst_code, "f8ff")) INST("$stpc <- MEM8[%d]", field_e);
          if (pattern_match(inst_code, "f9f.")) INST("%s <- MEM8[%d]", REG_D, field_e);
          if (pattern_match(inst_code, "f9ff")) INST("$tpc <- MEM8[%d]", field_e);
          if (pattern_match(inst_code, "faf.")) INST("%s <- MEM16[%d]", SREG_D, field_e);
          if (pattern_match(inst_code, "faff")) INST("$stpc <- MEM16[%d]", field_e);
          if (pattern_match(inst_code, "fbf.")) INST("%s <- MEM16[%d]", REG_D, field_e);
          if (pattern_match(inst_code, "fbff")) INST("$tpc <- MEM16[%d]", field_e);
          if (pattern_match(inst_code, "fcf.")) INST("%s <- MEM32[%d]", REG_D, field_e);
          if (pattern_match(inst_code, "fcff")) INST("$tpc <- MEM32[%d]", field_e);
          if (pattern_match(inst_code, "fdf.")) INST("MEM8[%d] <- %s", field_e, REG_D);
          if (pattern_match(inst_code, "fdff")) INST("MEM8[%d] <- $tpc", field_e);
          if (pattern_match(inst_code, "fef.")) INST("MEM16[%d] <- %s", field_e, REG_D);
          if (pattern_match(inst_code, "feff")) INST("MEM16[%d] <- $tpc", field_e);
          if (pattern_match(inst_code, "fff.")) INST("MEM32[%d] <- %s", field_e, REG_D);
          if (pattern_match(inst_code, "ffff")) INST("MEM32[%d] <- $tpc", field_e);

          if (pattern_match(inst_code, "f8..")) INST("%s <- MEM8[%s,%d]", SREG_D, REG_A, field_e);
          if (pattern_match(inst_code, "f8.f")) INST("$stpc <- MEM8[%s,%d]", REG_A, field_e);
          if (pattern_match(inst_code, "f9..")) INST("%s <- MEM8[%s,%d]", REG_D, REG_A, field_e);
          if (pattern_match(inst_code, "f9.f")) INST("$tpc <- MEM8[%s,%d]", REG_A, field_e);
          if (pattern_match(inst_code, "fa..")) INST("%s <- MEM16[%s,%d]", SREG_D, REG_A, field_e);
          if (pattern_match(inst_code, "fa.f")) INST("$stpc <- MEM16[%s,%d]", REG_A, field_e);
          if (pattern_match(inst_code, "fb..")) INST("%s <- MEM16[%s,%d]", REG_D, REG_A, field_e);
          if (pattern_match(inst_code, "fb.f")) INST("$tpc <- MEM16[%s,%d]", REG_A, field_e);
          if (pattern_match(inst_code, "fc..")) INST("%s <- MEM32[%s,%d]", REG_D, REG_A, field_e);
          if (pattern_match(inst_code, "fc.f")) INST("$tpc <- MEM32[%s,%d]", REG_A, field_e);
          if (pattern_match(inst_code, "fd..")) INST("MEM8[%s,%d] <- %s", REG_A, field_e, REG_D);
          if (pattern_match(inst_code, "fd.f")) INST("MEM8[%s,%d] <- $tpc", REG_A, field_e);
          if (pattern_match(inst_code, "fe..")) INST("MEM16[%s,%d] <- %s", REG_A, field_e, REG_D);
          if (pattern_match(inst_code, "fe.f")) INST("MEM16[%s,%d] <- $tpc", REG_A, field_e);
          if (pattern_match(inst_code, "ff..")) INST("MEM32[%s,%d] <- %s", REG_A, field_e, REG_D);
          if (pattern_match(inst_code, "ff.f")) INST("MEM32[%s,%d] <- $tpc", REG_A, field_e);
          break;
        default:
          OPCODES_ASSERT(false);
        }
      if (FIELD_D == 0xf)
        {
          if (pattern_match(inst_code, "0.0f")) INST("if %s == 0 $pc %s", REG_B, branch_target(field_e));
          if (pattern_match(inst_code, "0.1f")) INST("if %s != 0 $pc %s", REG_B, branch_target(field_e));
          if (pattern_match(inst_code, "0.2f")) INST("if %s < 0 $pc %s", SREG_B, branch_target(field_e));
          if (pattern_match(inst_code, "0.3f")) INST("if %s >= 0 $pc %s", SREG_B, branch_target(field_e));
          if (pattern_match(inst_code, "0.4f")) INST("if %s > 0 $pc %s", SREG_B, branch_target(field_e));
          if (pattern_match(inst_code, "0.5f")) INST("if %s <= 0 $pc %s", SREG_B, branch_target(field_e));
          if (pattern_match(inst_code, "0.6f")) UNKNOWN;
          if (pattern_match(inst_code, "0.7f")) UNKNOWN;
          if (pattern_match(inst_code, "0.8f")) UNKNOWN;
          if (pattern_match(inst_code, "0.9f")) UNKNOWN;
          if (pattern_match(inst_code, "0.af")) UNKNOWN;
          if (pattern_match(inst_code, "0.bf")) INST("if %s < 0 $pc %s", FREG_B, branch_target(field_e));
          if (pattern_match(inst_code, "0.cf")) INST("if %s >= 0 $pc %s", FREG_B, branch_target(field_e));
          if (pattern_match(inst_code, "0.df")) INST("if %s > 0 $pc %s", FREG_B, branch_target(field_e));
          if (pattern_match(inst_code, "0.ef")) INST("if %s <= 0 $pc %s", FREG_B, branch_target(field_e));

          if (pattern_match(inst_code, "1..f")) INST("if %s == %s $pc %s", REG_B, REG_A, branch_target(field_e));
          if (pattern_match(inst_code, "2..f")) INST("if %s != %s $pc %s", REG_B, REG_A, branch_target(field_e));
          if (pattern_match(inst_code, "3..f")) INST("if %s < %s $pc %s", SREG_B, SREG_A, branch_target(field_e));
          if (pattern_match(inst_code, "4..f")) INST("if %s >= %s $pc %s", SREG_B, SREG_A, branch_target(field_e));
          if (pattern_match(inst_code, "5..f")) INST("if %s < %s $pc %s", REG_B, REG_A, branch_target(field_e));
          if (pattern_match(inst_code, "6..f")) INST("if %s >= %s $pc %s", REG_B, REG_A, branch_target(field_e));
          if (pattern_match(inst_code, "7..f")) UNKNOWN;
          if (pattern_match(inst_code, "8..f")) UNKNOWN;
          if (pattern_match(inst_code, "9..f")) UNKNOWN;
          if (pattern_match(inst_code, "a..f")) UNKNOWN;
          if (pattern_match(inst_code, "b..f")) UNKNOWN;
          if (pattern_match(inst_code, "c..f")) UNKNOWN;
          if (pattern_match(inst_code, "d..f")) INST("if %s < %s $pc %s", FREG_B, FREG_A, branch_target(field_e));
          if (pattern_match(inst_code, "e..f")) INST("if %s >= %s $pc %s", FREG_B, FREG_A, branch_target(field_e));

          if (pattern_match(inst_code, ".f.f")) INST("if %s[%d] == 1 $pc %s", REG_A, FIELD_C, branch_target(field_e));
          if (pattern_match(inst_code, "..ff")) INST("if %s[%d] == 0 $pc %s", REG_B, FIELD_C, branch_target(field_e));
        }






















      if (inst & (1 << 15))
        {
          if (inst & (1 << 14))
            {
              /* This is a Form 3 instruction.  */
              int opcode = (inst >> 10 & 0xf);

              switch (opcode)
                {
                case 0x00: /* beq */
                  {
                    BREW_TRACE_INSN ("beq");
                    if (cpu.asregs.cc & CC_EQ)
                      pc += INST2OFFSET(inst);
                  }
                  break;
                case 0x01: /* bne */
                  {
                    BREW_TRACE_INSN ("bne");
                    if (! (cpu.asregs.cc & CC_EQ))
                      pc += INST2OFFSET(inst);
                  }
                  break;
                case 0x02: /* blt */
                  {
                    BREW_TRACE_INSN ("blt");
                    if (cpu.asregs.cc & CC_LT)
                      pc += INST2OFFSET(inst);
                  }                  break;
                case 0x03: /* bgt */
                  {
                    BREW_TRACE_INSN ("bgt");
                    if (cpu.asregs.cc & CC_GT)
                      pc += INST2OFFSET(inst);
                  }
                  break;
                case 0x04: /* bltu */
                  {
                    BREW_TRACE_INSN ("bltu");
                    if (cpu.asregs.cc & CC_LTU)
                      pc += INST2OFFSET(inst);
                  }
                  break;
                case 0x05: /* bgtu */
                  {
                    BREW_TRACE_INSN ("bgtu");
                    if (cpu.asregs.cc & CC_GTU)
                      pc += INST2OFFSET(inst);
                  }
                  break;
                case 0x06: /* bge */
                  {
                    BREW_TRACE_INSN ("bge");
                    if (cpu.asregs.cc & (CC_GT | CC_EQ))
                      pc += INST2OFFSET(inst);
                  }
                  break;
                case 0x07: /* ble */
                  {
                    BREW_TRACE_INSN ("ble");
                    if (cpu.asregs.cc & (CC_LT | CC_EQ))
                      pc += INST2OFFSET(inst);
                  }
                  break;
                case 0x08: /* bgeu */
                  {
                    BREW_TRACE_INSN ("bgeu");
                    if (cpu.asregs.cc & (CC_GTU | CC_EQ))
                      pc += INST2OFFSET(inst);
                  }
                  break;
                case 0x09: /* bleu */
                  {
                    BREW_TRACE_INSN ("bleu");
                    if (cpu.asregs.cc & (CC_LTU | CC_EQ))
                      pc += INST2OFFSET(inst);
                  }
                  break;
                default:
                  {
                    BREW_TRACE_INSN ("SIGILL3");
                    sim_engine_halt (sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL);
                    break;
                  }
                }
            }
          else
            {
              /* This is a Form 2 instruction.  */
              int opcode = (inst >> 12 & 0x3);
              switch (opcode)
                {
                case 0x00: /* inc */
                  {
                    int a = (inst >> 8) & 0xf;
                    unsigned av = cpu.asregs.regs[a];
                    unsigned v = (inst & 0xff);

                    BREW_TRACE_INSN ("inc");
                    cpu.asregs.regs[a] = av + v;
                  }
                  break;
                case 0x01: /* dec */
                  {
                    int a = (inst >> 8) & 0xf;
                    unsigned av = cpu.asregs.regs[a];
                    unsigned v = (inst & 0xff);

                    BREW_TRACE_INSN ("dec");
                    cpu.asregs.regs[a] = av - v;
                  }
                  break;
                case 0x02: /* gsr */
                  {
                    int a = (inst >> 8) & 0xf;
                    unsigned v = (inst & 0xff);

                    BREW_TRACE_INSN ("gsr");
                    cpu.asregs.regs[a] = cpu.asregs.sregs[v];
                  }
                  break;
                case 0x03: /* ssr */
                  {
                    int a = (inst >> 8) & 0xf;
                    unsigned v = (inst & 0xff);

                    BREW_TRACE_INSN ("ssr");
                    cpu.asregs.sregs[v] = cpu.asregs.regs[a];
                  }
                  break;
                default:
                  BREW_TRACE_INSN ("SIGILL2");
                  sim_engine_halt (sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL);
                  break;
                }
            }
        }
      else
        {
          /* This is a Form 1 instruction.  */
          int opcode = inst >> 8;
          switch (opcode)
            {
            case 0x00: /* bad */
              opc = opcode;
              BREW_TRACE_INSN ("SIGILL0");
              sim_engine_halt (sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL);
              break;
            case 0x01: /* ldi.l (immediate) */
              {
                int reg = (inst >> 4) & 0xf;
                unsigned int val = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("ldi.l");
                cpu.asregs.regs[reg] = val;
                pc += 4;
              }
              break;
            case 0x02: /* mov (register-to-register) */
              {
                int dest  = (inst >> 4) & 0xf;
                int src = (inst ) & 0xf;

                BREW_TRACE_INSN ("mov");
                cpu.asregs.regs[dest] = cpu.asregs.regs[src];
              }
              break;
             case 0x03: /* jsra */
               {
                 unsigned int fn = EXTRACT_WORD(pc+2);
                 unsigned int sp = cpu.asregs.regs[1];

                BREW_TRACE_INSN ("jsra");
                 /* Save a slot for the static chain.  */
                sp -= 4;

                 /* Push the return address.  */
                sp -= 4;
                 wlat (scpu, opc, sp, pc + 6);
                 
                 /* Push the current frame pointer.  */
                 sp -= 4;
                 wlat (scpu, opc, sp, cpu.asregs.regs[0]);
 
                 /* Uncache the stack pointer and set the pc and $fp.  */
                cpu.asregs.regs[1] = sp;
                cpu.asregs.regs[0] = sp;
                 pc = fn - 2;
               }
               break;
             case 0x04: /* ret */
               {
                 unsigned int sp = cpu.asregs.regs[0];

                BREW_TRACE_INSN ("ret");
 
                 /* Pop the frame pointer.  */
                 cpu.asregs.regs[0] = rlat (scpu, opc, sp);
                 sp += 4;
                 
                 /* Pop the return address.  */
                 pc = rlat (scpu, opc, sp) - 2;
                 sp += 4;

                /* Skip over the static chain slot.  */
                sp += 4;
 
                 /* Uncache the stack pointer.  */
                 cpu.asregs.regs[1] = sp;
                }
                break;
            case 0x05: /* add.l */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                unsigned av = cpu.asregs.regs[a];
                unsigned bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("add.l");
                cpu.asregs.regs[a] = av + bv;
              }
              break;
            case 0x06: /* push */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                int sp = cpu.asregs.regs[a] - 4;

                BREW_TRACE_INSN ("push");
                wlat (scpu, opc, sp, cpu.asregs.regs[b]);
                cpu.asregs.regs[a] = sp;
              }
              break;
            case 0x07: /* pop */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                int sp = cpu.asregs.regs[a];

                BREW_TRACE_INSN ("pop");
                cpu.asregs.regs[b] = rlat (scpu, opc, sp);
                cpu.asregs.regs[a] = sp + 4;
              }
              break;
            case 0x08: /* lda.l */
              {
                int reg = (inst >> 4) & 0xf;
                unsigned int addr = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("lda.l");
                cpu.asregs.regs[reg] = rlat (scpu, opc, addr);
                pc += 4;
              }
              break;
            case 0x09: /* sta.l */
              {
                int reg = (inst >> 4) & 0xf;
                unsigned int addr = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("sta.l");
                wlat (scpu, opc, addr, cpu.asregs.regs[reg]);
                pc += 4;
              }
              break;
            case 0x0a: /* ld.l (register indirect) */
              {
                int src  = inst & 0xf;
                int dest = (inst >> 4) & 0xf;
                int xv;

                BREW_TRACE_INSN ("ld.l");
                xv = cpu.asregs.regs[src];
                cpu.asregs.regs[dest] = rlat (scpu, opc, xv);
              }
              break;
            case 0x0b: /* st.l */
              {
                int dest = (inst >> 4) & 0xf;
                int val  = inst & 0xf;

                BREW_TRACE_INSN ("st.l");
                wlat (scpu, opc, cpu.asregs.regs[dest], cpu.asregs.regs[val]);
              }
              break;
            case 0x0c: /* ldo.l */
              {
                unsigned int addr = EXTRACT_OFFSET(pc+2);
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;

                BREW_TRACE_INSN ("ldo.l");
                addr += cpu.asregs.regs[b];
                cpu.asregs.regs[a] = rlat (scpu, opc, addr);
                pc += 2;
              }
              break;
            case 0x0d: /* sto.l */
              {
                unsigned int addr = EXTRACT_OFFSET(pc+2);
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;

                BREW_TRACE_INSN ("sto.l");
                addr += cpu.asregs.regs[a];
                wlat (scpu, opc, addr, cpu.asregs.regs[b]);
                pc += 2;
              }
              break;
            case 0x0e: /* cmp */
              {
                int a  = (inst >> 4) & 0xf;
                int b  = inst & 0xf;
                int cc = 0;
                int va = cpu.asregs.regs[a];
                int vb = cpu.asregs.regs[b]; 

                BREW_TRACE_INSN ("cmp");
                if (va == vb)
                  cc = CC_EQ;
                else
                  {
                    cc |= (va < vb ? CC_LT : 0);
                    cc |= (va > vb ? CC_GT : 0);
                    cc |= ((unsigned int) va < (unsigned int) vb ? CC_LTU : 0);
                    cc |= ((unsigned int) va > (unsigned int) vb ? CC_GTU : 0);
                  }

                cpu.asregs.cc = cc;
              }
              break;
            case 0x0f: /* nop */
              break;
            case 0x10: /* sex.b */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                signed char bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("sex.b");
                cpu.asregs.regs[a] = (int) bv;
              }
              break;
            case 0x11: /* sex.s */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                signed short bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("sex.s");
                cpu.asregs.regs[a] = (int) bv;
              }
              break;
            case 0x12: /* zex.b */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                signed char bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("zex.b");
                cpu.asregs.regs[a] = (int) bv & 0xff;
              }
              break;
            case 0x13: /* zex.s */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                signed short bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("zex.s");
                cpu.asregs.regs[a] = (int) bv & 0xffff;
              }
              break;
            case 0x14: /* umul.x */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                unsigned av = cpu.asregs.regs[a];
                unsigned bv = cpu.asregs.regs[b];
                unsigned long long r = 
                  (unsigned long long) av * (unsigned long long) bv;

                BREW_TRACE_INSN ("umul.x");
                cpu.asregs.regs[a] = r >> 32;
              }
              break;
            case 0x15: /* mul.x */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                unsigned av = cpu.asregs.regs[a];
                unsigned bv = cpu.asregs.regs[b];
                signed long long r = 
                  (signed long long) av * (signed long long) bv;

                BREW_TRACE_INSN ("mul.x");
                cpu.asregs.regs[a] = r >> 32;
              }
              break;
            case 0x16: /* bad */
            case 0x17: /* bad */
            case 0x18: /* bad */
              {
                opc = opcode;
                BREW_TRACE_INSN ("SIGILL0");
                sim_engine_halt (sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL);
                break;
              }
            case 0x19: /* jsr */
              {
                unsigned int fn = cpu.asregs.regs[(inst >> 4) & 0xf];
                unsigned int sp = cpu.asregs.regs[1];

                BREW_TRACE_INSN ("jsr");

                 /* Save a slot for the static chain.  */
                sp -= 4;

                /* Push the return address.  */
                sp -= 4;
                wlat (scpu, opc, sp, pc + 2);
                
                /* Push the current frame pointer.  */
                sp -= 4;
                wlat (scpu, opc, sp, cpu.asregs.regs[0]);

                /* Uncache the stack pointer and set the fp & pc.  */
                cpu.asregs.regs[1] = sp;
                cpu.asregs.regs[0] = sp;
                pc = fn - 2;
              }
              break;
            case 0x1a: /* jmpa */
              {
                unsigned int tgt = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("jmpa");
                pc = tgt - 2;
              }
              break;
            case 0x1b: /* ldi.b (immediate) */
              {
                int reg = (inst >> 4) & 0xf;
                unsigned int val = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("ldi.b");
                cpu.asregs.regs[reg] = val;
                pc += 4;
              }
              break;
            case 0x1c: /* ld.b (register indirect) */
              {
                int src  = inst & 0xf;
                int dest = (inst >> 4) & 0xf;
                int xv;

                BREW_TRACE_INSN ("ld.b");
                xv = cpu.asregs.regs[src];
                cpu.asregs.regs[dest] = rbat (scpu, opc, xv);
              }
              break;
            case 0x1d: /* lda.b */
              {
                int reg = (inst >> 4) & 0xf;
                unsigned int addr = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("lda.b");
                cpu.asregs.regs[reg] = rbat (scpu, opc, addr);
                pc += 4;
              }
              break;
            case 0x1e: /* st.b */
              {
                int dest = (inst >> 4) & 0xf;
                int val  = inst & 0xf;

                BREW_TRACE_INSN ("st.b");
                wbat (scpu, opc, cpu.asregs.regs[dest], cpu.asregs.regs[val]);
              }
              break;
            case 0x1f: /* sta.b */
              {
                int reg = (inst >> 4) & 0xf;
                unsigned int addr = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("sta.b");
                wbat (scpu, opc, addr, cpu.asregs.regs[reg]);
                pc += 4;
              }
              break;
            case 0x20: /* ldi.s (immediate) */
              {
                int reg = (inst >> 4) & 0xf;

                unsigned int val = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("ldi.s");
                cpu.asregs.regs[reg] = val;
                pc += 4;
              }
              break;
            case 0x21: /* ld.s (register indirect) */
              {
                int src  = inst & 0xf;
                int dest = (inst >> 4) & 0xf;
                int xv;

                BREW_TRACE_INSN ("ld.s");
                xv = cpu.asregs.regs[src];
                cpu.asregs.regs[dest] = rsat (scpu, opc, xv);
              }
              break;
            case 0x22: /* lda.s */
              {
                int reg = (inst >> 4) & 0xf;
                unsigned int addr = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("lda.s");
                cpu.asregs.regs[reg] = rsat (scpu, opc, addr);
                pc += 4;
              }
              break;
            case 0x23: /* st.s */
              {
                int dest = (inst >> 4) & 0xf;
                int val  = inst & 0xf;

                BREW_TRACE_INSN ("st.s");
                wsat (scpu, opc, cpu.asregs.regs[dest], cpu.asregs.regs[val]);
              }
              break;
            case 0x24: /* sta.s */
              {
                int reg = (inst >> 4) & 0xf;
                unsigned int addr = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("sta.s");
                wsat (scpu, opc, addr, cpu.asregs.regs[reg]);
                pc += 4;
              }
              break;
            case 0x25: /* jmp */
              {
                int reg = (inst >> 4) & 0xf;

                BREW_TRACE_INSN ("jmp");
                pc = cpu.asregs.regs[reg] - 2;
              }
              break;
            case 0x26: /* and */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                int av, bv;

                BREW_TRACE_INSN ("and");
                av = cpu.asregs.regs[a];
                bv = cpu.asregs.regs[b];
                cpu.asregs.regs[a] = av & bv;
              }
              break;
            case 0x27: /* lshr */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                int av = cpu.asregs.regs[a];
                int bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("lshr");
                cpu.asregs.regs[a] = (unsigned) ((unsigned) av >> bv);
              }
              break;
            case 0x28: /* ashl */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                int av = cpu.asregs.regs[a];
                int bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("ashl");
                cpu.asregs.regs[a] = av << bv;
              }
              break;
            case 0x29: /* sub.l */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                unsigned av = cpu.asregs.regs[a];
                unsigned bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("sub.l");
                cpu.asregs.regs[a] = av - bv;
              }
              break;
            case 0x2a: /* neg */
              {
                int a  = (inst >> 4) & 0xf;
                int b  = inst & 0xf;
                int bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("neg");
                cpu.asregs.regs[a] = - bv;
              }
              break;
            case 0x2b: /* or */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                int av, bv;

                BREW_TRACE_INSN ("or");
                av = cpu.asregs.regs[a];
                bv = cpu.asregs.regs[b];
                cpu.asregs.regs[a] = av | bv;
              }
              break;
            case 0x2c: /* not */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                int bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("not");
                cpu.asregs.regs[a] = 0xffffffff ^ bv;
              }
              break;
            case 0x2d: /* ashr */
              {
                int a  = (inst >> 4) & 0xf;
                int b  = inst & 0xf;
                int av = cpu.asregs.regs[a];
                int bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("ashr");
                cpu.asregs.regs[a] = av >> bv;
              }
              break;
            case 0x2e: /* xor */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                int av, bv;

                BREW_TRACE_INSN ("xor");
                av = cpu.asregs.regs[a];
                bv = cpu.asregs.regs[b];
                cpu.asregs.regs[a] = av ^ bv;
              }
              break;
            case 0x2f: /* mul.l */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                unsigned av = cpu.asregs.regs[a];
                unsigned bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("mul.l");
                cpu.asregs.regs[a] = av * bv;
              }
              break;
            case 0x30: /* swi */
              {
                unsigned int inum = EXTRACT_WORD(pc+2);

                BREW_TRACE_INSN ("swi");
                /* Set the special registers appropriately.  */
                cpu.asregs.sregs[2] = 3; /* BREW_EX_SWI */
                cpu.asregs.sregs[3] = inum;
                switch (inum)
                  {
                  case TARGET_NEWLIB_SYS_exit:
                    {
                      sim_engine_halt (sd, scpu, NULL, pc, sim_exited,
                                       cpu.asregs.regs[2]);
                      break;
                    }
                  case TARGET_NEWLIB_SYS_open:
                    {
                      char fname[1024];
                      int mode = (int) convert_target_flags ((unsigned) cpu.asregs.regs[3]);
                      int perm = (int) cpu.asregs.regs[4];
                      int fd;
                      sim_core_read_buffer (sd, scpu, read_map, fname,
                                            cpu.asregs.regs[2], 1024);
                      fd = sim_io_open (sd, fname, mode);
                      /* FIXME - set errno */
                      cpu.asregs.regs[2] = fd;
                      break;
                    }
                  case TARGET_NEWLIB_SYS_read:
                    {
                      int fd = cpu.asregs.regs[2];
                      unsigned len = (unsigned) cpu.asregs.regs[4];
                      char *buf = malloc (len);
                      cpu.asregs.regs[2] = sim_io_read (sd, fd, buf, len);
                      sim_core_write_buffer (sd, scpu, write_map, buf,
                                             cpu.asregs.regs[3], len);
                      free (buf);
                      break;
                    }
                  case TARGET_NEWLIB_SYS_write:
                    {
                      char *str;
                      /* String length is at 0x12($fp) */
                      unsigned count, len = (unsigned) cpu.asregs.regs[4];
                      str = malloc (len);
                      sim_core_read_buffer (sd, scpu, read_map, str,
                                            cpu.asregs.regs[3], len);
                      count = sim_io_write (sd, cpu.asregs.regs[2], str, len);
                      free (str);
                      cpu.asregs.regs[2] = count;
                      break;
                    }
                  case TARGET_NEWLIB_SYS_unlink:
                    {
                      char fname[1024];
                      int fd;
                      sim_core_read_buffer (sd, scpu, read_map, fname,
                                            cpu.asregs.regs[2], 1024);
                      fd = sim_io_unlink (sd, fname);
                      /* FIXME - set errno */
                      cpu.asregs.regs[2] = fd;
                      break;
                    }
                  case 0xffffffff: /* Linux System Call */
                    {
                      unsigned int handler = cpu.asregs.sregs[1];
                      unsigned int sp = cpu.asregs.regs[1];

                      /* Save a slot for the static chain.  */
                      sp -= 4;

                      /* Push the return address.  */
                      sp -= 4;
                      wlat (scpu, opc, sp, pc + 6);
                
                      /* Push the current frame pointer.  */
                      sp -= 4;
                      wlat (scpu, opc, sp, cpu.asregs.regs[0]);

                      /* Uncache the stack pointer and set the fp & pc.  */
                      cpu.asregs.regs[1] = sp;
                      cpu.asregs.regs[0] = sp;
                      pc = handler - 6;
                    }
                  default:
                    break;
                  }
                pc += 4;
              }
              break;
            case 0x31: /* div.l */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                int av = cpu.asregs.regs[a];
                int bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("div.l");
                cpu.asregs.regs[a] = av / bv;
              }
              break;
            case 0x32: /* udiv.l */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                unsigned int av = cpu.asregs.regs[a];
                unsigned int bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("udiv.l");
                cpu.asregs.regs[a] = (av / bv);
              }
              break;
            case 0x33: /* mod.l */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                int av = cpu.asregs.regs[a];
                int bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("mod.l");
                cpu.asregs.regs[a] = av % bv;
              }
              break;
            case 0x34: /* umod.l */
              {
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;
                unsigned int av = cpu.asregs.regs[a];
                unsigned int bv = cpu.asregs.regs[b];

                BREW_TRACE_INSN ("umod.l");
                cpu.asregs.regs[a] = (av % bv);
              }
              break;
            case 0x35: /* brk */
              BREW_TRACE_INSN ("brk");
              sim_engine_halt (sd, scpu, NULL, pc, sim_stopped, SIM_SIGTRAP);
              pc -= 2; /* Adjust pc */
              break;
            case 0x36: /* ldo.b */
              {
                unsigned int addr = EXTRACT_OFFSET(pc+2);
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;

                BREW_TRACE_INSN ("ldo.b");
                addr += cpu.asregs.regs[b];
                cpu.asregs.regs[a] = rbat (scpu, opc, addr);
                pc += 2;
              }
              break;
            case 0x37: /* sto.b */
              {
                unsigned int addr = EXTRACT_OFFSET(pc+2);
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;

                BREW_TRACE_INSN ("sto.b");
                addr += cpu.asregs.regs[a];
                wbat (scpu, opc, addr, cpu.asregs.regs[b]);
                pc += 2;
              }
              break;
            case 0x38: /* ldo.s */
              {
                unsigned int addr = EXTRACT_OFFSET(pc+2);
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;

                BREW_TRACE_INSN ("ldo.s");
                addr += cpu.asregs.regs[b];
                cpu.asregs.regs[a] = rsat (scpu, opc, addr);
                pc += 2;
              }
              break;
            case 0x39: /* sto.s */
              {
                unsigned int addr = EXTRACT_OFFSET(pc+2);
                int a = (inst >> 4) & 0xf;
                int b = inst & 0xf;

                BREW_TRACE_INSN ("sto.s");
                addr += cpu.asregs.regs[a];
                wsat (scpu, opc, addr, cpu.asregs.regs[b]);
                pc += 2;
              }
              break;
            default:
              opc = opcode;
              BREW_TRACE_INSN ("SIGILL1");
              sim_engine_halt (sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL);
              break;
            }
        }

      cpu.asregs.insts++;
      pc += 2;
      cpu.asregs.regs[PC_REGNO] = pc;

      if (sim_events_tick (sd))
        sim_events_process (sd);

    } while (1);
}

/* Read a memory location (memory) and store it in register (rn). Length is required to be 4 (32-bits) */
/* Returns the number of bytes read from memory */

static int
brew_reg_store (sim_cpu *scpu, int rn, unsigned char *memory, int length)
{
  if (rn > NUM_BREW_REGS && rn < 0)
    return 0;
  if (length != 4)
    return 0;

  long ival;
  
  /* misalignment safe */
  ival = brew_extract_unsigned_integer (memory, 4);
  scpu->registers[rn] = ival;
  return 4;
}

/* Store register (rn) at memory location (memory). Length is required to be 4 (32-bits) */
/* Returns the number of bytes stored in memory */
static int
brew_reg_fetch (sim_cpu *scpu, int rn, unsigned char *memory, int length)
{
  if (rn > NUM_BREW_REGS && rn < 0)
    return 0;
  if (length != 4)
    return 0;

  long ival = scpu->registers[rn];

  /* misalignment-safe */
  brew_store_unsigned_integer (memory, 4, ival);
  return 4;
}

/* Returns the current PC */
static sim_cia
brew_pc_get (sim_cpu *scpu)
{
  return scpu->registers[0];
}

/* Sets the PC to the required value */
static void
brew_pc_set (sim_cpu *scpu, sim_cia pc)
{
  scpu->registers[0] = pc;
}

/* De-allocates any resources, allocated in sim_open */
static void
free_state (SIM_DESC sd)
{
  if (STATE_MODULES (sd) != NULL)
    sim_module_uninstall (sd);
  sim_cpu_free_all (sd);
  sim_state_free (sd);
}

/* Starts the simulation context. Returns a 'whatever' that will be used as a context variable
   kind: SIM_OPEN_STANDALONE for standalone or SIM_OPEN_DEBUG for gdb sessions
*/
SIM_DESC
sim_open (SIM_OPEN_KIND kind, host_callback *cb,
          struct bfd *abfd, char * const *argv)
{
  int i;
  SIM_DESC sd = sim_state_alloc (kind, cb);
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);

  /* Set default options before parsing user options.  */
  current_target_byte_order = BFD_ENDIAN_LITTLE;

  /* The cpu data is kept in a separately allocated chunk of memory.  */
  SIM_ASSERT(MAX_NR_PROCESSORS == 1);
  if (sim_cpu_alloc_all (sd, MAX_NR_PROCESSORS) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  if (sim_pre_argv_init (sd, argv[0]) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* The parser will print an error message for us, so we silently return.  */
  if (sim_parse_args (sd, argv) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* FIXME: not sure what these do. Probably create and allocate some memory */
  sim_do_command(sd," memory region 0x00000000,0x4000000") ; 
  //sim_do_command(sd," memory region 0xE0000000,0x10000") ; 

  /* Check for/establish the a reference program image.  */
  if   (sim_analyze_program (sd, STATE_PROG_FILE (sd), abfd) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* Configure/verify the target byte order and other runtime
     configuration options.  */
  if (sim_config (sd) != SIM_RC_OK)
    {
      sim_module_uninstall (sd);
      return 0;
    }

  if (sim_post_argv_init (sd) != SIM_RC_OK)
    {
      /* Uninstall the modules to avoid memory leaks,
         file descriptor leaks, etc.  */
      sim_module_uninstall (sd);
      return 0;
    }

  /* CPU specific initialization. */
  /* That is: setting up callbacks for load/store registers and for get/set of PC */
  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
    {
      sim_cpu *cpu = STATE_CPU (sd, i);

      CPU_REG_FETCH (cpu) = brew_reg_fetch;
      CPU_REG_STORE (cpu) = brew_reg_store;
      CPU_PC_FETCH (cpu) = brew_pc_get;
      CPU_PC_STORE (cpu) = brew_pc_set;

      set_initial_gprs ();        /* Reset the GPR registers.  */
    }

  return sd;
}

/* Load the device tree blob.  */

static void
load_dtb (SIM_DESC sd, const char *filename)
{
  int size = 0;
  FILE *f = fopen (filename, "rb");
  char *buf;
  sim_cpu *scpu = STATE_CPU (sd, 0); /* FIXME */ 

  /* Don't warn as the sim works fine w/out a device tree.  */
  if (f == NULL)
    return;
  fseek (f, 0, SEEK_END);
  size = ftell(f);
  fseek (f, 0, SEEK_SET);
  buf = alloca (size);
  if (size != fread (buf, 1, size, f))
    {
      sim_io_eprintf (sd, "ERROR: error reading ``%s''.\n", filename);
      fclose (f);
      return;
    }
  sim_core_write_buffer (sd, scpu, write_map, buf, 0xE0000000, size);
  cpu.asregs.sregs[9] = 0xE0000000;
  fclose (f);
}

SIM_RC
sim_create_inferior (SIM_DESC sd, struct bfd *prog_bfd,
                     char * const *argv, char * const *env)
{
  char * const *avp;
  int l, argc, i, tp;
  sim_cpu *scpu = STATE_CPU (sd, 0); /* FIXME */

  if (prog_bfd != NULL)
    cpu.asregs.regs[PC_REGNO] = bfd_get_start_address (prog_bfd);

  /* Copy args into target memory.  */
  avp = argv;
  for (argc = 0; avp && *avp; avp++)
    argc++;

  /* Target memory looks like this:
     0x00000000 zero word
     0x00000004 argc word
     0x00000008 start of argv
     .
     0x0000???? end of argv
     0x0000???? zero word 
     0x0000???? start of data pointed to by argv  */

  wlat (scpu, 0, 0, 0);
  wlat (scpu, 0, 4, argc);

  /* tp is the offset of our first argv data.  */
  tp = 4 + 4 + argc * 4 + 4;

  for (i = 0; i < argc; i++)
    {
      /* Set the argv value.  */
      wlat (scpu, 0, 4 + 4 + i * 4, tp);

      /* Store the string.  */
      sim_core_write_buffer (sd, scpu, write_map, argv[i],
                             tp, strlen(argv[i])+1);
      tp += strlen (argv[i]) + 1;
    }

  wlat (scpu, 0, 4 + 4 + i * 4, 0);

  load_dtb (sd, DTB);

  return SIM_RC_OK;
}
