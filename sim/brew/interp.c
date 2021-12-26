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

#include <math.h>





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

static const char * reg_names[BREW_NUM_REGS] =
  { "$pc", "$r1", "$r2",  "$r3",  "$r4",  "$r5",  "$r6",  "$r7", 
    "$r8", "$r9", "$r10", "$r11", "$r12", "$r13", "$r14", NULL /* this is the 'other' PC */, "$npc" /* this is the next value of pc */};



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
static INLINE uint16_t read_uint16(sim_cpu *scpu, uint32_t addr) { return sim_core_read_aligned_2(scpu, CPU_PC_GET(scpu), read_map, addr); }
static INLINE uint32_t read_uint32(sim_cpu *scpu, uint32_t addr) { return sim_core_read_aligned_4(scpu, CPU_PC_GET(scpu), read_map, addr); }

static INLINE void write_int8 (sim_cpu *scpu, uint32_t addr, uint8_t  val) { sim_core_write_aligned_1(scpu, CPU_PC_GET(scpu), write_map, addr, val); }
static INLINE void write_int16(sim_cpu *scpu, uint32_t addr, uint16_t val) { sim_core_write_aligned_2(scpu, CPU_PC_GET(scpu), write_map, addr, val); }
static INLINE void write_int32(sim_cpu *scpu, uint32_t addr, uint32_t val) { sim_core_write_aligned_4(scpu, CPU_PC_GET(scpu), write_map, addr, val); }

static INLINE int8_t  read_int8 (sim_cpu *scpu, uint32_t addr) { return (int8_t)sim_core_read_aligned_1(scpu, CPU_PC_GET(scpu), read_map, addr); }
static INLINE int16_t read_int16(sim_cpu *scpu, uint32_t addr) { return (int16_t)sim_core_read_aligned_2(scpu, CPU_PC_GET(scpu), read_map, addr); }
static INLINE int32_t read_int32(sim_cpu *scpu, uint32_t addr) { return (int32_t)sim_core_read_aligned_4(scpu, CPU_PC_GET(scpu), read_map, addr); }

#if 0
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
#endif

static INLINE void brew_trace(sim_cpu *scpu, const char *fmt, ...)
{
  char *pc_name;
  char message[256];
  char fragment[256];
  va_list args;

  pc_name = scpu->is_task_mode ? "$tpc": "$spc";

  snprintf(message, sizeof(message), "%cPC: 0x%x ", scpu->is_task_mode?'T':'S', scpu->regs[BREW_REG_PC]);
  message[sizeof(message)-1] = 0;
  va_start (args, fmt);
  vsnprintf(fragment, sizeof(fragment)-1, fmt, args);
  fragment[sizeof(fragment)-1] = 0;
  va_end (args);
  strncat(message, fragment, sizeof(message) - strlen(message) - 1);
  message[sizeof(message)-1] = 0;
  for (int i=0;i<BREW_NUM_REGS;++i)
    {
      if (scpu->regs_touch[i])
        {
          snprintf(fragment, sizeof(fragment)-1, " %s <- 0x%x", reg_names[i] == NULL ? pc_name : reg_names[i], scpu->regs[i]);
          strncat(message, fragment, sizeof(message) - strlen(message) - 1);
          message[sizeof(message)-1] = 0;
        }
    }
  TRACE_INSN(scpu, "%s", message);
}

/* TODO: Split this up into finer trace levels than just insn.  */
static INLINE void brew_trace_all(sim_cpu *scpu, const char *message)
{
  if (message == NULL) message = "---";
  TRACE_INSN(
    scpu,
    "%s, %cPC: 0x%x, %s, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, (%cPC: 0x%x)",
    scpu->is_task_mode?"TSK":"SCH", 
    scpu->is_task_mode?'T':'S', scpu->regs[0],
    message, 
    scpu->regs[0], scpu->regs[1], scpu->regs[2], scpu->regs[3], scpu->regs[4], scpu->regs[5], 
    scpu->regs[6], scpu->regs[7], scpu->regs[8], scpu->regs[9], scpu->regs[10], 
    scpu->regs[11], scpu->regs[12], scpu->regs[13], scpu->regs[14],
    scpu->is_task_mode?'S':'T', scpu->regs[15]);
}
#define BREW_TRACE_INST(...) brew_trace(scpu, __VA_ARGS__)

#define UNKNOWN { \
  char inst_str[25]; \
  if (length == 2) \
    sprintf(inst_str, "UKN 0x%04x", inst_code); \
  else \
    sprintf(inst_str, "UKN 0x%04x 0x%08x", inst_code, field_e); \
  BREW_TRACE_INST(inst_str); \
  update_pc_before_exception = false; \
  if (scpu->is_task_mode) swap_mode = true; \
  sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGILL); \
}

#define REG_D_TARGET scpu->regs_touch[FIELD_D == BREW_REG_PC ? BREW_REG_NEXT_PC : FIELD_D] = true; scpu->regs[FIELD_D == BREW_REG_PC ? BREW_REG_NEXT_PC : FIELD_D]
#define REG_TPC_TARGET scpu->regs_touch[scpu->is_task_mode ? BREW_REG_NEXT_PC : BREW_REG_OTHER_PC] = true; scpu->regs[scpu->is_task_mode ? BREW_REG_NEXT_PC : BREW_REG_OTHER_PC]
#define TREG_D_TARGET temp_reg_idx = FIELD_D == 0 ? scpu->is_task_mode ? BREW_REG_NEXT_PC : BREW_REG_OTHER_PC : FIELD_D == BREW_REG_PC ? BREW_REG_NEXT_PC : FIELD_D; scpu->regs_touch[temp_reg_idx] = true; scpu->regs[temp_reg_idx]

#define REG_D (scpu->regs[FIELD_D] & 0xffffffff) /* simple trick to ensure that this field is invalid on the LHS */
#define REG_A (scpu->regs[FIELD_A] & 0xffffffff) /* simple trick to ensure that this field is invalid on the LHS */
#define REG_B (scpu->regs[FIELD_B] & 0xffffffff) /* simple trick to ensure that this field is invalid on the LHS */
#define REG_TPC (scpu->regs[scpu->is_task_mode ? BREW_REG_PC : BREW_REG_OTHER_PC] & 0xffffffff) /* simple trick to ensure that this field is invalid on the LHS */
#define REG_PC (scpu->regs[BREW_REG_PC] & 0xffffffff) /* simple trick to ensure that this field is invalid on the LHS */
#define TREG_D (scpu->regs[FIELD_D == 0 ? scpu->is_task_mode ? BREW_REG_NEXT_PC : BREW_REG_OTHER_PC : FIELD_D] & 0xffffffff)
#define TREG_A (scpu->regs[FIELD_A == 0 ? scpu->is_task_mode ? BREW_REG_NEXT_PC : BREW_REG_OTHER_PC : FIELD_A] & 0xffffffff)
#define TREG_B (scpu->regs[FIELD_B == 0 ? scpu->is_task_mode ? BREW_REG_NEXT_PC : BREW_REG_OTHER_PC : FIELD_B] & 0xffffffff)

#define NEXT_INST(...) { BREW_TRACE_INST(__VA_ARGS__); goto next_inst; }


/* tracing macros */
#define trTREG_D (tr_treg_names[FIELD_D])
#define trTREG_A (tr_treg_names[FIELD_A])
#define trTREG_B (tr_treg_names[FIELD_B])

#define trREG_D (tr_reg_names[FIELD_D])
#define trREG_A (tr_reg_names[FIELD_A])
#define trREG_B (tr_reg_names[FIELD_B])

#define trSREG_D (tr_sreg_names[FIELD_D])
#define trSREG_A (tr_sreg_names[FIELD_A])
#define trSREG_B (tr_sreg_names[FIELD_B])

#define trFREG_D (tr_freg_names[FIELD_D])
#define trFREG_A (tr_freg_names[FIELD_A])
#define trFREG_B (tr_freg_names[FIELD_B])

static const char * tr_treg_names[16] =
  { "$tpc", "$r1", "$r2",  "$r3",  "$r4",  "$r5",  "$r6",  "$r7", 
    "$r8",  "$r9", "$r10", "$r11", "$r12", "$r13", "$r14", "<<<INVALID>>>"};

static const char * tr_reg_names[16] =
  { "$pc", "$r1", "$r2",  "$r3",  "$r4",  "$r5",  "$r6",  "$r7", 
    "$r8", "$r9", "$r10", "$r11", "$r12", "$r13", "$r14", "<<<INVALID>>>"};

static const char * tr_sreg_names[16] =
  { "$spc", "$sr1", "$sr2",  "$sr3",  "$sr4",  "$sr5",  "$sr6",  "$sr7", 
    "$sr8", "$sr9", "$sr10", "$sr11", "$sr12", "$sr13", "$sr14", "<<<INVALID>>>"};

static const char * tr_freg_names[16] =
  { "$fpc", "$fr1", "$fr2",  "$fr3",  "$fr4",  "$fr5",  "$fr6",  "$fr7", 
    "$fr8", "$fr9", "$fr10", "$fr11", "$fr12", "$fr13", "$fr14", "<<<INVALID>>>"};

static char branch_target_buffer[255];
static char *branch_target(uint32_t field_e)
{
  if ((field_e & 0) == 0)
    {
      /* Abosolute address */
      sprintf(branch_target_buffer, "<- %u", field_e);
    }
  else
    {
      /* Relative address */
      sprintf(branch_target_buffer, "<- $pc+%d", (int32_t)field_e);
    }
  return branch_target_buffer;
}


static INLINE float as_float(uint32_t int_val)
{
  float f;
  memcpy(&f, &int_val, sizeof(float));
  return f;
}

static INLINE uint32_t as_int(float float_val)
{
  uint32_t i;
  memcpy(&i, &float_val, sizeof(uint32_t));
  return i;
}

static INLINE uint32_t bswap(uint32_t val)
{
  return
    (((val >> 0) & 0xff) << 24) |
    (((val >> 8) & 0xff) << 16) |
    (((val >> 16) & 0xff) << 8) |
    (((val >> 24) & 0xff) << 0);
}

static INLINE uint32_t wswap(uint32_t val)
{
  return
    (((val >> 0) & 0xffff) << 16) |
    (((val >> 16) & 0xffff) << 0);
}

#define TEST_ALIGN(reg, alignment) if (reg % (alignment-1) != 0) sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGBUS);

static INLINE void branch_to(sim_cpu *scpu, uint32_t field_e)
{
  scpu->regs_touch[BREW_REG_NEXT_PC] = true;
  if ((field_e & 1) != 0)
    {
      scpu->regs[BREW_REG_NEXT_PC] += field_e & ~1;
    }
  else
    {
      scpu->regs[BREW_REG_NEXT_PC] = field_e & ~1;
    }
}

void
sim_engine_run (SIM_DESC sd,
                int next_cpu_nr, /* the CPU to run  */
                int nr_cpus, /* ignore  */
                int siggnal) /* ignore  */
{
  uint16_t inst_code;
  uint32_t field_e = 0xdeadbeef;
  int length;
  sim_cpu *scpu = STATE_CPU (sd, next_cpu_nr);
  int temp_reg_idx; /* Holds a temporary register index for some macros */
  bool swap_mode;
  bool update_pc_before_exception;
  /* Run instructions here. */
  do 
    {
      {
        /* scoping pc to make sure we don't even accidentally use that for anything */
        uint32_t pc = CPU_PC_GET (scpu) & ~1; /* PC is always 16-bit aligned, the LSB is ignored */

        /* Fetch the instruction at pc.  */
        length = 2;
        swap_mode = false;
        update_pc_before_exception = true; /* normally we update PC before excepting */
        inst_code = sim_core_read_aligned_2(scpu, pc, exec_map, pc);

        /* Determine if we need field_e and read it if we do */
        if (FIELD_D == 0xf || FIELD_B == 0xf || FIELD_A == 0xf || (inst_code >> 11) == 0x1f) {
          length = 6;
          field_e = sim_core_read_unaligned_4(scpu, pc, exec_map, pc+2);
        }
        scpu->regs[BREW_REG_NEXT_PC] = pc + length; /* Update PC to the next instruction. It's possible that the instruction itself is a branch, in which case it'll be overwritten */
      }
      for (int i=0;i<BREW_NUM_REGS;++i)
        scpu->regs_touch[i] = false;
      switch (FIELD_C)
        {
        case 0x0: /* ^ and special */
          if (pattern_match(inst_code, "0000")) { BREW_TRACE_INST("FILL"); if (scpu->is_task_mode) swap_mode = true; sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGILL); }
          if (pattern_match(inst_code, "0110")) { BREW_TRACE_INST("BREAK"); if (scpu->is_task_mode) swap_mode = true; sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGTRAP); }
          if (pattern_match(inst_code, "0220")) { BREW_TRACE_INST("SYSCALL"); if (scpu->is_task_mode) swap_mode = true; sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGILL); } /* We should simulate syscalls here */
          if (pattern_match(inst_code, "0330")) { if (scpu->is_task_mode) { swap_mode = true; NEXT_INST("STU"); } else sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGABRT); } /* Return to task mode --> NOP in task mode */
          if (pattern_match(inst_code, "0440")) UNKNOWN; /* This is actually known, it is the official SII exception */ 
          if (pattern_match(inst_code, "0dd0")) { NEXT_INST("FENCE"); }
          if (pattern_match(inst_code, "0ee0")) { NEXT_INST("WFENCE"); }
          if (pattern_match(inst_code, "0..0")) UNKNOWN;
          if (pattern_match(inst_code, "0ff.")) { REG_D_TARGET = field_e; BREW_TRACE_INST("%s <- %u", trREG_D, field_e); }
          if (pattern_match(inst_code, "0f..")) UNKNOWN;
          if (pattern_match(inst_code, "0.f.")) { REG_D_TARGET = REG_B ^ field_e; NEXT_INST("%s <- %s ^ %u", trREG_D, trREG_B, field_e); }
          if (pattern_match(inst_code, "0...")) { REG_D_TARGET = REG_B ^ REG_A; NEXT_INST("%s <- %s ^ %s", trREG_D, trREG_B, trREG_A); }
          break;
        case 0x1:
          if (pattern_match(inst_code, "1000")) { BREW_TRACE_INST("WOI"); sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGILL); }
          if (pattern_match(inst_code, "1f..")) UNKNOWN;
          if (pattern_match(inst_code, "1.f.")) { REG_D_TARGET = REG_B | field_e; NEXT_INST("%s <- %s | %u", trREG_D, trREG_B, field_e); }
          if (pattern_match(inst_code, "1...")) { REG_D_TARGET = REG_B | REG_A; NEXT_INST("%s <- %s | %s", trREG_D, trREG_B, trREG_A); }
          break;
        case 0x2:
          if (pattern_match(inst_code, "2f..")) UNKNOWN;
          if (pattern_match(inst_code, "2.f.")) { REG_D_TARGET = REG_B & field_e; NEXT_INST("%s <- %s & %u", trREG_D, trREG_B, field_e); }
          if (pattern_match(inst_code, "2...")) { REG_D_TARGET = REG_B & REG_A; NEXT_INST("%s <- %s & %s", trREG_D, trREG_B, trREG_A); }
          break;
        case 0x3:
          if (pattern_match(inst_code, "3f..")) { REG_D_TARGET = field_e - REG_A; NEXT_INST("%s <- %u - %s", trREG_D, field_e, trREG_A); }
          if (pattern_match(inst_code, "3.f.")) { REG_D_TARGET = REG_B - field_e; NEXT_INST("%s <- %s - %u", trREG_D, trREG_B, field_e); }
          if (pattern_match(inst_code, "3...")) { REG_D_TARGET = REG_B - REG_A; NEXT_INST("%s <- %s - %s", trREG_D, trREG_B, trREG_A); }
          break;
        case 0x4:
          if (pattern_match(inst_code, "4f..")) UNKNOWN;
          if (pattern_match(inst_code, "4.f.")) { REG_D_TARGET = REG_B + field_e; NEXT_INST("%s <- %s + %u", trREG_D, trREG_B, field_e); }
          if (pattern_match(inst_code, "4...")) { REG_D_TARGET = REG_B + REG_A; NEXT_INST("%s <- %s + %s", trREG_D, trREG_B, trREG_A); }
          break;
        case 0x5:
          if (pattern_match(inst_code, "5f..")) { REG_D_TARGET = field_e << REG_A; NEXT_INST("%s <- %u << %s", trREG_D, field_e, trREG_A); }
          if (pattern_match(inst_code, "5.f.")) { REG_D_TARGET = REG_B << field_e; NEXT_INST("%s <- %s << %u", trREG_D, trREG_B, field_e); }
          if (pattern_match(inst_code, "5...")) { REG_D_TARGET = REG_B << REG_A; NEXT_INST("%s <- %s << %s", trREG_D, trREG_B, trREG_A); }
          break;
        case 0x6:
          if (pattern_match(inst_code, "6f..")) { REG_D_TARGET = field_e >> REG_A; NEXT_INST("%s <- %u >> %s", trREG_D, field_e, trREG_A); }
          if (pattern_match(inst_code, "6.f.")) { REG_D_TARGET = REG_B >> field_e; NEXT_INST("%s <- %s >> %u", trREG_D, trREG_B, field_e); }
          if (pattern_match(inst_code, "6...")) { REG_D_TARGET = REG_B >> REG_A; NEXT_INST("%s <- %s >> %s", trREG_D, trREG_B, trREG_A); }
          break;
        case 0x7:
          if (pattern_match(inst_code, "7f..")) { REG_D_TARGET = ((int32_t)field_e) >> REG_A; NEXT_INST("%s <- %d >> %s", trSREG_D, (int32_t)field_e, trREG_A); }
          if (pattern_match(inst_code, "7.f.")) { REG_D_TARGET = ((int32_t)REG_B) >> field_e; NEXT_INST("%s <- %s >> %u", trSREG_D, trSREG_B, field_e); }
          if (pattern_match(inst_code, "7...")) { REG_D_TARGET = ((int32_t)REG_B) >> REG_A; NEXT_INST("%s <- %s >> %s", trSREG_D, trSREG_B, trREG_A); }
          break;
        case 0x8:
          if (pattern_match(inst_code, "8f..")) UNKNOWN;
          if (pattern_match(inst_code, "8ff.")) UNKNOWN;
          if (pattern_match(inst_code, "80f.")) { TREG_D_TARGET = field_e; NEXT_INST("%s <- %u", trTREG_D, field_e); }
          if (pattern_match(inst_code, "8.f.")) { REG_D_TARGET = REG_B * field_e; NEXT_INST("%s <- %s * %u", trREG_D, trREG_B, field_e); }
          if (pattern_match(inst_code, "80..")) { TREG_D_TARGET = TREG_A; NEXT_INST("%s <- %s", trTREG_D, trTREG_A); }
          if (pattern_match(inst_code, "8.0.")) UNKNOWN;
          if (pattern_match(inst_code, "8...")) { REG_D_TARGET = REG_B * REG_A; NEXT_INST("%s <- %s * %s", trREG_D, trREG_B, trREG_A); }
          break;
        case 0x9:
          if (pattern_match(inst_code, "9f..")) UNKNOWN;
          if (pattern_match(inst_code, "9ff.")) UNKNOWN;
          if (pattern_match(inst_code, "90f.")) UNKNOWN;
          if (pattern_match(inst_code, "9.f.")) { REG_D_TARGET = ((int32_t)REG_B) * ((int32_t)field_e); NEXT_INST("%s <- %s * %d", trSREG_D, trSREG_B, (int32_t)field_e); }
          if (pattern_match(inst_code, "90..")) { REG_D_TARGET = REG_A + 1; NEXT_INST("%s <- %s + 1", trREG_D, trREG_A); }
          if (pattern_match(inst_code, "9.0.")) { REG_D_TARGET = REG_B - 1; NEXT_INST("%s <- %s - 1", trREG_D, trREG_B); }
          if (pattern_match(inst_code, "9..0")) UNKNOWN;
          if (pattern_match(inst_code, "9...")) { REG_D_TARGET = ((int32_t)REG_B) * ((int32_t)REG_A); NEXT_INST("%s <- %s * %s", trSREG_D, trSREG_B, trSREG_A); }
          break;
        case 0xa:
          if (pattern_match(inst_code, "af..")) UNKNOWN;
          if (pattern_match(inst_code, "aff.")) UNKNOWN;
          if (pattern_match(inst_code, "a0f.")) UNKNOWN;
          if (pattern_match(inst_code, "a.f.")) { REG_D_TARGET = (((uint64_t)REG_B) * ((uint64_t)field_e)) >> 32; NEXT_INST("%s <- upper %s * %u", trREG_D, trREG_B, field_e); }
          if (pattern_match(inst_code, "a0..")) { REG_D_TARGET = -(int32_t)REG_A; NEXT_INST("%s <- -%s", trSREG_D, trSREG_A); }
          if (pattern_match(inst_code, "a.0.")) { REG_D_TARGET = ~REG_B; NEXT_INST("%s <- ~%s", trREG_D, trREG_B); }
          if (pattern_match(inst_code, "a..0")) UNKNOWN;
          if (pattern_match(inst_code, "a...")) { REG_D_TARGET = (((uint64_t)REG_B) * ((uint64_t)REG_A)) >> 32; NEXT_INST("%s <- upper %s * %s", trREG_D, trREG_B, trREG_A); }
          break;
        case 0xb:
          if (pattern_match(inst_code, "bf..")) UNKNOWN;
          if (pattern_match(inst_code, "bff.")) UNKNOWN;
          if (pattern_match(inst_code, "b0f.")) UNKNOWN;
          if (pattern_match(inst_code, "b.f.")) { REG_D_TARGET = (((int64_t)REG_B) * ((int64_t)field_e)) >> 32; NEXT_INST("%s <- %s * %d", trSREG_D, trSREG_B, (int32_t)field_e); }
          if (pattern_match(inst_code, "b0..")) { REG_D_TARGET = bswap(REG_A); NEXT_INST("%s <- bswap %s", trREG_D, trREG_A); }
          if (pattern_match(inst_code, "b.0.")) { REG_D_TARGET = wswap(REG_B); NEXT_INST("%s <- wswap %s", trREG_D, trREG_B); }
          if (pattern_match(inst_code, "b..0")) UNKNOWN;
          if (pattern_match(inst_code, "b...")) { REG_D_TARGET = (((int64_t)REG_B) * ((int64_t)REG_A)) >> 32; NEXT_INST("%s <- %s * %s", trSREG_D, trSREG_B, trSREG_A); }
          break;
        case 0xc:
          if (pattern_match(inst_code, "cf..")) UNKNOWN;
          if (pattern_match(inst_code, "c0f.")) UNKNOWN;
          if (pattern_match(inst_code, "c.f0")) UNKNOWN;
          if (pattern_match(inst_code, "cff.")) UNKNOWN;
          if (pattern_match(inst_code, "c.f.")) { REG_D_TARGET = as_int(as_float(REG_B) + as_float(field_e)); NEXT_INST("%s <- %s + %f", trFREG_D, trFREG_B, as_float(field_e)); }
          if (pattern_match(inst_code, "c..0")) UNKNOWN;
          if (pattern_match(inst_code, "c.0.")) { if (REG_A <= 0.0f) { if (scpu->is_task_mode) { swap_mode = true; update_pc_before_exception = false; } sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGFPE); } REG_D_TARGET = as_int(1.0f / sqrtf(as_float(REG_B))); NEXT_INST("%s <- rsqrt %s", trFREG_D, trFREG_B); }
          if (pattern_match(inst_code, "c0..")) { if (REG_A == 0.0f) { if (scpu->is_task_mode) { swap_mode = true; update_pc_before_exception = false; } sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGFPE); } REG_D_TARGET = as_int(1.0f / as_float(REG_A)); NEXT_INST("%s <- 1 / %s", trFREG_D, trFREG_A); }
          if (pattern_match(inst_code, "c...")) { REG_D_TARGET = as_int(as_float(REG_B) + as_float(REG_A)); NEXT_INST("%s <- %s + %s", trFREG_D, trFREG_B, trFREG_A); }
          break;
        case 0xd:
          if (pattern_match(inst_code, "df..")) { REG_D_TARGET = as_int(as_float(field_e) + as_float(REG_A)); NEXT_INST("%s <- %f - %s", trFREG_D, as_float(field_e), trFREG_A); }
          if (pattern_match(inst_code, "d0f.")) UNKNOWN;
          if (pattern_match(inst_code, "d.f0")) UNKNOWN;
          if (pattern_match(inst_code, "dff.")) UNKNOWN;
          if (pattern_match(inst_code, "d.f.")) { REG_D_TARGET = as_int(as_float(REG_B) - as_float(field_e)); NEXT_INST("%s <- %s - %f", trFREG_D, trFREG_B, as_float(field_e)); }
          if (pattern_match(inst_code, "d..0")) UNKNOWN;
          if (pattern_match(inst_code, "d.0.")) { REG_D_TARGET = as_int((float)REG_B); NEXT_INST("%s <- %s", trFREG_D, trSREG_B); }
          if (pattern_match(inst_code, "d0..")) { REG_D_TARGET = floorf(as_float(REG_A)); NEXT_INST("%s <- floor %s", trSREG_D, trFREG_A); }
          if (pattern_match(inst_code, "d...")) { REG_D_TARGET = as_int(as_float(REG_B) - as_float(REG_A)); NEXT_INST("%s <- %s - %s", trFREG_D, trFREG_B, trFREG_A); }
          break;
        case 0xe:
          if (pattern_match(inst_code, "ef..")) UNKNOWN;
          if (pattern_match(inst_code, "e0f.")) UNKNOWN;
          if (pattern_match(inst_code, "e.f0")) UNKNOWN;
          if (pattern_match(inst_code, "eff.")) UNKNOWN;
          if (pattern_match(inst_code, "e.f.")) { REG_D_TARGET = as_int(as_float(REG_B) * as_float(field_e)); NEXT_INST("R <- FR * FI"); }
          if (pattern_match(inst_code, "e..0")) UNKNOWN;
          if (pattern_match(inst_code, "e.0.")) UNKNOWN;
          if (pattern_match(inst_code, "e0..")) UNKNOWN;
          if (pattern_match(inst_code, "e...")) { REG_D_TARGET = as_int(as_float(REG_B) * as_float(REG_A)); NEXT_INST("R <- FR * FR"); }
          break;
        case 0xf:
          if (pattern_match(inst_code, "f0..")) { REG_D_TARGET = (int32_t)read_int8(scpu, REG_A); NEXT_INST("%s <- MEM8[%s]", trSREG_D, trREG_A); }
          if (pattern_match(inst_code, "f1..")) { REG_D_TARGET = read_uint8(scpu, REG_A); NEXT_INST("%s <- MEM8[%s]", trREG_D, trREG_A); }
          if (pattern_match(inst_code, "f2..")) { TEST_ALIGN(REG_A, 2); REG_D_TARGET = (int32_t)read_int16(scpu, REG_A); NEXT_INST("%s <- MEM16[%s]", trSREG_D, trREG_A); }
          if (pattern_match(inst_code, "f3..")) { TEST_ALIGN(REG_A, 2); REG_D_TARGET = read_uint16(scpu, REG_A); NEXT_INST("%s <- MEM16[%s]", trREG_D, trREG_A); }
          if (pattern_match(inst_code, "f4..")) { TEST_ALIGN(REG_A, 4); REG_D_TARGET = read_uint32(scpu, REG_A); NEXT_INST("%s <- MEM32[%s]", trREG_D, trREG_A); }
          if (pattern_match(inst_code, "f5..")) { write_uint8(scpu, REG_A, REG_D); NEXT_INST("MEM8[%s] <- %s", trREG_A, trREG_D); }
          if (pattern_match(inst_code, "f6..")) { TEST_ALIGN(REG_A, 2); write_uint16(scpu, REG_A, REG_D); NEXT_INST("MEM16[%s] <- %s", trREG_A, trREG_D); }
          if (pattern_match(inst_code, "f7..")) { TEST_ALIGN(REG_A, 4); write_uint32(scpu, REG_A, REG_D); NEXT_INST("MEM32[%s] <- %s", trREG_A, trREG_D); }
          if (pattern_match(inst_code, "f7.f")) UNKNOWN;
          if (pattern_match(inst_code, "f7f.")) UNKNOWN;

          if (pattern_match(inst_code, "f8f.")) { REG_D_TARGET = (int32_t)read_int8(scpu, field_e); NEXT_INST("%s <- MEM8[%d]", trSREG_D, field_e); }
          if (pattern_match(inst_code, "f8ff")) { REG_TPC_TARGET = (int32_t)read_int8(scpu, field_e); NEXT_INST("$stpc <- MEM8[%d]", field_e); }
          if (pattern_match(inst_code, "f9f.")) { REG_D_TARGET = read_uint8(scpu, field_e); NEXT_INST("%s <- MEM8[%d]", trREG_D, field_e); }
          if (pattern_match(inst_code, "f9ff")) { REG_TPC_TARGET = read_uint8(scpu, field_e); NEXT_INST("$tpc <- MEM8[%d]", field_e); }
          if (pattern_match(inst_code, "faf.")) { TEST_ALIGN(field_e, 2); REG_D_TARGET = (int32_t)read_int16(scpu, field_e); NEXT_INST("%s <- MEM16[%d]", trSREG_D, field_e); }
          if (pattern_match(inst_code, "faff")) { TEST_ALIGN(field_e, 2); REG_TPC_TARGET = (int32_t)read_int16(scpu, field_e); NEXT_INST("$stpc <- MEM16[%d]", field_e); }
          if (pattern_match(inst_code, "fbf.")) { TEST_ALIGN(field_e, 2); REG_D_TARGET = read_uint16(scpu, field_e); NEXT_INST("%s <- MEM16[%d]", trREG_D, field_e); }
          if (pattern_match(inst_code, "fbff")) { TEST_ALIGN(field_e, 2); REG_TPC_TARGET = read_uint16(scpu, field_e); NEXT_INST("$tpc <- MEM16[%d]", field_e); }
          if (pattern_match(inst_code, "fcf.")) { TEST_ALIGN(field_e, 4); REG_D_TARGET = read_uint32(scpu, field_e); NEXT_INST("%s <- MEM32[%d]", trREG_D, field_e); }
          if (pattern_match(inst_code, "fcff")) { TEST_ALIGN(field_e, 4); REG_TPC_TARGET = read_uint32(scpu, field_e); NEXT_INST("$tpc <- MEM32[%d]", field_e); }
          if (pattern_match(inst_code, "fdf.")) { write_uint8(scpu, field_e, REG_D); NEXT_INST("MEM8[%d] <- %s", field_e, trREG_D); }
          if (pattern_match(inst_code, "fdff")) { write_uint8(scpu, field_e, REG_TPC); NEXT_INST("MEM8[%d] <- $tpc", field_e); }
          if (pattern_match(inst_code, "fef.")) { TEST_ALIGN(field_e, 2); write_uint16(scpu, field_e, REG_D); NEXT_INST("MEM16[%d] <- %s", field_e, trREG_D); }
          if (pattern_match(inst_code, "feff")) { TEST_ALIGN(field_e, 2); write_uint16(scpu, field_e, REG_TPC); NEXT_INST("MEM16[%d] <- $tpc", field_e); }
          if (pattern_match(inst_code, "fff.")) { TEST_ALIGN(field_e, 4); write_uint32(scpu, field_e, REG_D); NEXT_INST("MEM32[%d] <- %s", field_e, trREG_D); }
          if (pattern_match(inst_code, "ffff")) { TEST_ALIGN(field_e, 4); write_uint32(scpu, field_e, REG_TPC); NEXT_INST("MEM32[%d] <- $tpc", field_e); }

          if (pattern_match(inst_code, "f8..")) { REG_D_TARGET = (int32_t)read_int8(scpu, REG_A+field_e); NEXT_INST("%s <- MEM8[%s,%d]", trSREG_D, trREG_A, field_e); }
          if (pattern_match(inst_code, "f8.f")) { REG_TPC_TARGET = (int32_t)read_int8(scpu, REG_A+field_e); NEXT_INST("$stpc <- MEM8[%s,%d]", trREG_A, field_e); }
          if (pattern_match(inst_code, "f9..")) { REG_D_TARGET = read_uint8(scpu, REG_A+field_e); NEXT_INST("%s <- MEM8[%s,%d]", trREG_D, trREG_A, field_e); }
          if (pattern_match(inst_code, "f9.f")) { REG_TPC_TARGET = read_uint8(scpu, REG_A+field_e); NEXT_INST("$tpc <- MEM8[%s,%d]", trREG_A, field_e); }
          if (pattern_match(inst_code, "fa..")) { TEST_ALIGN(REG_A+field_e, 2); REG_D_TARGET = (int32_t)read_int16(scpu, REG_A+field_e); NEXT_INST("%s <- MEM16[%s,%d]", trSREG_D, trREG_A, field_e); }
          if (pattern_match(inst_code, "fa.f")) { TEST_ALIGN(REG_A+field_e, 2); REG_TPC_TARGET = (int32_t)read_int16(scpu, REG_A+field_e); NEXT_INST("$stpc <- MEM16[%s,%d]", trREG_A, field_e); }
          if (pattern_match(inst_code, "fb..")) { TEST_ALIGN(REG_A+field_e, 2); REG_D_TARGET = read_uint16(scpu, REG_A+field_e); NEXT_INST("%s <- MEM16[%s,%d]", trREG_D, trREG_A, field_e); }
          if (pattern_match(inst_code, "fb.f")) { TEST_ALIGN(REG_A+field_e, 2); REG_TPC_TARGET = read_uint16(scpu, REG_A+field_e); NEXT_INST("$tpc <- MEM16[%s,%d]", trREG_A, field_e); }
          if (pattern_match(inst_code, "fc..")) { TEST_ALIGN(REG_A+field_e, 4); REG_D_TARGET = read_uint32(scpu, REG_A+field_e); NEXT_INST("%s <- MEM32[%s,%d]", trREG_D, trREG_A, field_e); }
          if (pattern_match(inst_code, "fc.f")) { TEST_ALIGN(REG_A+field_e, 4); REG_TPC_TARGET = read_uint32(scpu, REG_A+field_e); NEXT_INST("$tpc <- MEM32[%s,%d]", trREG_A, field_e); }
          if (pattern_match(inst_code, "fd..")) { write_uint8(scpu, REG_A+field_e, REG_D); NEXT_INST("MEM8[%s,%d] <- %s", trREG_A, field_e, trREG_D); }
          if (pattern_match(inst_code, "fd.f")) { write_uint8(scpu, REG_A+field_e, REG_TPC); NEXT_INST("MEM8[%s,%d] <- $tpc", trREG_A, field_e); }
          if (pattern_match(inst_code, "fe..")) { TEST_ALIGN(REG_A+field_e, 2); write_uint16(scpu, REG_A+field_e, REG_D); NEXT_INST("MEM16[%s,%d] <- %s", trREG_A, field_e, trREG_D); }
          if (pattern_match(inst_code, "fe.f")) { TEST_ALIGN(REG_A+field_e, 2); write_uint16(scpu, REG_A+field_e, REG_TPC); NEXT_INST("MEM16[%s,%d] <- $tpc", trREG_A, field_e); }
          if (pattern_match(inst_code, "ff..")) { TEST_ALIGN(REG_A+field_e, 4); write_uint32(scpu, REG_A+field_e, REG_D); NEXT_INST("MEM32[%s,%d] <- %s", trREG_A, field_e, trREG_D); }
          if (pattern_match(inst_code, "ff.f")) { TEST_ALIGN(REG_A+field_e, 4); write_uint32(scpu, REG_A+field_e, REG_TPC); NEXT_INST("MEM32[%s,%d] <- $tpc", trREG_A, field_e); }
          break;
        default:
          SIM_ASSERT(false);
        }
      if (FIELD_D == 0xf)
        {
          if (pattern_match(inst_code, "0.0f")) { if (REG_B == 0) branch_to(scpu, field_e); NEXT_INST("if %s == 0 $pc %s", trREG_B, branch_target(field_e)); }
          if (pattern_match(inst_code, "0.1f")) { if (REG_B != 0) branch_to(scpu, field_e); NEXT_INST("if %s != 0 $pc %s", trREG_B, branch_target(field_e)); }
          if (pattern_match(inst_code, "0.2f")) { if ((int32_t)REG_B < 0) branch_to(scpu, field_e); NEXT_INST("if %s < 0 $pc %s", trSREG_B, branch_target(field_e)); }
          if (pattern_match(inst_code, "0.3f")) { if ((int32_t)REG_B >= 0) branch_to(scpu, field_e); NEXT_INST("if %s >= 0 $pc %s", trSREG_B, branch_target(field_e)); }
          if (pattern_match(inst_code, "0.4f")) { if ((int32_t)REG_B > 0) branch_to(scpu, field_e); NEXT_INST("if %s > 0 $pc %s", trSREG_B, branch_target(field_e)); }
          if (pattern_match(inst_code, "0.5f")) { if ((int32_t)REG_B <= 0) branch_to(scpu, field_e); NEXT_INST("if %s <= 0 $pc %s", trSREG_B, branch_target(field_e)); }
          if (pattern_match(inst_code, "0.6f")) UNKNOWN;
          if (pattern_match(inst_code, "0.7f")) UNKNOWN;
          if (pattern_match(inst_code, "0.8f")) UNKNOWN;
          if (pattern_match(inst_code, "0.9f")) UNKNOWN;
          if (pattern_match(inst_code, "0.af")) UNKNOWN;
          if (pattern_match(inst_code, "0.bf")) { if (as_float(REG_B) < 0.0f) branch_to(scpu, field_e); NEXT_INST("if %s < 0 $pc %s", trFREG_B, branch_target(field_e)); }
          if (pattern_match(inst_code, "0.cf")) { if (as_float(REG_B) >= 0.0f) branch_to(scpu, field_e); NEXT_INST("if %s >= 0 $pc %s", trFREG_B, branch_target(field_e)); }
          if (pattern_match(inst_code, "0.df")) { if (as_float(REG_B) > 0.0f) branch_to(scpu, field_e); NEXT_INST("if %s > 0 $pc %s", trFREG_B, branch_target(field_e)); }
          if (pattern_match(inst_code, "0.ef")) { if (as_float(REG_B) <= 0.0f) branch_to(scpu, field_e); NEXT_INST("if %s <= 0 $pc %s", trFREG_B, branch_target(field_e)); }

          if (pattern_match(inst_code, "1..f")) { if (REG_B == REG_A) branch_to(scpu, field_e); NEXT_INST("if %s == %s $pc %s", trREG_B, trREG_A, branch_target(field_e)); }
          if (pattern_match(inst_code, "2..f")) { if (REG_B != REG_A) branch_to(scpu, field_e); NEXT_INST("if %s != %s $pc %s", trREG_B, trREG_A, branch_target(field_e)); }
          if (pattern_match(inst_code, "3..f")) { if ((int32_t)REG_B < (int32_t)REG_A) branch_to(scpu, field_e); NEXT_INST("if %s < %s $pc %s", trSREG_B, trSREG_A, branch_target(field_e)); }
          if (pattern_match(inst_code, "4..f")) { if ((int32_t)REG_B >= (int32_t)REG_A) branch_to(scpu, field_e); NEXT_INST("if %s >= %s $pc %s", trSREG_B, trSREG_A, branch_target(field_e)); }
          if (pattern_match(inst_code, "5..f")) { if (REG_B < REG_A) branch_to(scpu, field_e); NEXT_INST("if %s < %s $pc %s", trREG_B, trREG_A, branch_target(field_e)); }
          if (pattern_match(inst_code, "6..f")) { if (REG_B >= REG_A) branch_to(scpu, field_e); NEXT_INST("if %s >= %s $pc %s", trREG_B, trREG_A, branch_target(field_e)); }
          if (pattern_match(inst_code, "7..f")) UNKNOWN;
          if (pattern_match(inst_code, "8..f")) UNKNOWN;
          if (pattern_match(inst_code, "9..f")) UNKNOWN;
          if (pattern_match(inst_code, "a..f")) UNKNOWN;
          if (pattern_match(inst_code, "b..f")) UNKNOWN;
          if (pattern_match(inst_code, "c..f")) UNKNOWN;
          if (pattern_match(inst_code, "d..f")) { if (as_float(REG_B) < as_float(REG_A)) branch_to(scpu, field_e); NEXT_INST("if %s < %s $pc %s", trFREG_B, trFREG_A, branch_target(field_e)); }
          if (pattern_match(inst_code, "e..f")) { if (as_float(REG_B) >= as_float(REG_A)) branch_to(scpu, field_e); NEXT_INST("if %s >= %s $pc %s", trFREG_B, trFREG_A, branch_target(field_e)); }

          if (pattern_match(inst_code, ".f.f")) { if ((REG_A & (1 << FIELD_C)) != 0) branch_to(scpu, field_e); NEXT_INST("if %s[%d] == 1 $pc %s", trREG_A, FIELD_C, branch_target(field_e)); }
          if (pattern_match(inst_code, "..ff")) { if ((REG_A & (1 << FIELD_C)) == 0) branch_to(scpu, field_e); NEXT_INST("if %s[%d] == 0 $pc %s", trREG_B, FIELD_C, branch_target(field_e)); }
        }

    next_inst:
      /* TODO: for now we don't support scheduler mode */
      swap_mode = false;

      if (swap_mode)
        {
          uint32_t temp;
          if (update_pc_before_exception)
            scpu->regs[BREW_REG_PC] = scpu->regs[BREW_REG_NEXT_PC];
          temp = scpu->regs[BREW_REG_PC];
          scpu->regs[BREW_REG_PC] = scpu->regs[BREW_REG_OTHER_PC];
          scpu->regs[BREW_REG_OTHER_PC] = temp;
          scpu->is_task_mode = !scpu->is_task_mode;
        }
      else
        {
          scpu->regs[BREW_REG_PC] = scpu->regs[BREW_REG_NEXT_PC];
        }

      if (sim_events_tick (sd))
        sim_events_process (sd);

    } while (1);
}


static INLINE int read_mem_unaligned(uint8_t *memory)
{
  return
    (memory[3] << 24) |
    (memory[2] << 16) |
    (memory[1] << 8) |
    (memory[0] << 0);
}

static INLINE void write_mem_unaligned(uint8_t *memory, uint32_t val)
{
  memory[3] = val >> 24;
  memory[2] = val >> 16;
  memory[1] = val >> 8;
  memory[0] = val >> 0;
}

/* Read a memory location (memory) and store it in register (rn). Length is required to be 4 (32-bits) */
/* Returns the number of bytes read from memory, 0 if no store is performed and negative if an error occured */
/* This apparently is called by GDB when storing all registers in memory for it's ... reasons */
static int
brew_reg_store (sim_cpu *scpu, int rn, unsigned char *memory, int length)
{
  if (rn > BREW_NUM_REGS && rn < 0)
    return 0;
  if (length != 4)
    return -1;

  scpu->regs[rn]= read_mem_unaligned(memory);
  return 4;
}

/* Store register (rn) at memory location (memory). Length is required to be 4 (32-bits) */
/* Returns the number of bytes stored in memory */
static int
brew_reg_fetch (sim_cpu *scpu, int rn, unsigned char *memory, int length)
{
  if (rn > BREW_NUM_REGS && rn < 0)
    return 0;
  if (length != 4)
    return -1;

  write_mem_unaligned(memory, scpu->regs[rn]);
  return 4;
}

/* Returns the current PC */
static sim_cia
brew_pc_get (sim_cpu *scpu)
{
  return scpu->regs[BREW_REG_PC];
}

/* Sets the PC to the required value */
static void
brew_pc_set (sim_cpu *scpu, sim_cia pc)
{
  scpu->regs[BREW_REG_PC] = pc;
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
  sim_do_command(sd," memory region 0x00000000,0x4000000"); /* main memory for program storage */
  //sim_do_command(sd," memory region 0xE0000000,0x10000"); /* this is where the DTB goes, if loaded */

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
      sim_cpu *scpu = STATE_CPU (sd, i);

      CPU_REG_FETCH(scpu) = brew_reg_fetch;
      CPU_REG_STORE(scpu) = brew_reg_store;
      CPU_PC_FETCH(scpu) = brew_pc_get;
      CPU_PC_STORE(scpu) = brew_pc_set;

      set_initial_gprs(scpu);        /* Reset the GPR registers.  */
    }

  return sd;
}

#if 0
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
  scpu->regs[9] = 0xE0000000;
  fclose (f);
}
#endif

SIM_RC
sim_create_inferior (SIM_DESC sd, struct bfd *prog_bfd,
                     char * const *argv, char * const *env)
{
  char * const *avp;
  int l, argc, i, tp;
  sim_cpu *scpu = STATE_CPU (sd, 0); /* FIXME */

  if (prog_bfd != NULL)
    scpu->regs[BREW_REG_PC] = bfd_get_start_address (prog_bfd);

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

  write_uint32(scpu, 0, 0);
  write_uint32(scpu, 4, argc);

  /* tp is the offset of our first argv data.  */
  tp = 4 + 4 + argc * 4 + 4;

  for (i = 0; i < argc; i++)
    {
      /* Set the argv value.  */
      write_uint32(scpu, 4 + 4 + i * 4, tp);

      /* Store the string.  */
      sim_core_write_buffer(sd, scpu, write_map, argv[i], tp, strlen(argv[i])+1);
      tp += strlen (argv[i]) + 1;
    }

  write_uint32(scpu, 4 + 4 + i * 4, 0);

  /*load_dtb(sd, DTB);*/

  return SIM_RC_OK;
}
