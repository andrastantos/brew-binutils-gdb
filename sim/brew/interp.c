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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>
#include <time.h>
#include <sys/times.h>
#include <errno.h>
#include "bfd.h"
#include "libiberty.h"
#include "sim/sim.h"

#include "sim-main.h"
#include "sim-base.h"
#include "sim-options.h"
#include "sim-io.h"
#include "sim-signal.h"
#include "target-newlib-syscall.h"

#include "newlib_syscalls.h"
#include "newlib_stat.h"
#include "brew-calls.h"

#include <math.h>





/* Macros to extract operands from the instruction word.  */
#define FIELD_C ((insn_code >> 12) & 0xf)
#define FIELD_B ((insn_code >> 8) & 0xf)
#define FIELD_A ((insn_code >> 4) & 0xf)
#define FIELD_D ((insn_code >> 0) & 0xf)

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

static bool pattern_match(uint16_t insn_code, const char *pattern)
{
  const char *p;
  int nibble;
  int hex_p;
  for (p=pattern, nibble=3;nibble >=0;--nibble, ++p)
    {
      if (*p == '.')
        {
          if (GET_NIBBLE(insn_code, nibble) == 0xf)
            {
              return false;
            }
          continue;
        }
      if (!hexdigit(*p, &hex_p))
        {
          return false;
        }
      if (hex_p != GET_NIBBLE(insn_code, nibble))
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

typedef struct {
  uint32_t addr;
  int access_size;
  bool sign_extend;
  bool is_write;
  uint32_t value;
  bool is_valid;
} mem_trace_s;
mem_trace_s mem_trace;

static void record_mem_trace(uint32_t addr, int access_size, bool sign_extend, bool is_write, uint32_t value)
{
  mem_trace.addr = addr;
  mem_trace.access_size = access_size;
  mem_trace.sign_extend = sign_extend;
  mem_trace.is_write = is_write;
  mem_trace.value = value;
  mem_trace.is_valid = true;
}

static void mem_trace_to_str(mem_trace_s *mem_trace, char *buffer)
{
  if (mem_trace->is_valid)
    {
      mem_trace->is_valid = false;
      if (mem_trace->is_write)
        {
          sprintf(buffer, "mem%d[%d (0x%x)] <- %d (0x%x)", mem_trace->access_size, mem_trace->addr, mem_trace->addr, mem_trace->value, mem_trace->value);
          return;
        }
      else
        {
          sprintf(buffer, "%d (0x%x) <- mem%d[%d (0x%x)]", mem_trace->value, mem_trace->value, mem_trace->access_size, mem_trace->addr, mem_trace->addr);
          return;
        }
    }
  else
    {
      buffer[0] = 0;
      return;
    }
}

static INLINE void write_uint8 (sim_cpu *scpu, uint32_t addr, uint8_t  val) { record_mem_trace(addr, 8,  false, true, val); sim_core_write_aligned_1(scpu, CPU_PC_GET(scpu), write_map, addr, val); }
static INLINE void write_uint16(sim_cpu *scpu, uint32_t addr, uint16_t val) { record_mem_trace(addr, 16, false, true, val); sim_core_write_aligned_2(scpu, CPU_PC_GET(scpu), write_map, addr, val); }
static INLINE void write_uint32(sim_cpu *scpu, uint32_t addr, uint32_t val) { record_mem_trace(addr, 32, false, true, val); sim_core_write_aligned_4(scpu, CPU_PC_GET(scpu), write_map, addr, val); }

static INLINE uint8_t  read_uint8 (sim_cpu *scpu, uint32_t addr) { uint8_t  val = sim_core_read_aligned_1(scpu, CPU_PC_GET(scpu), read_map, addr); record_mem_trace(addr, 8,  false, false, val); return val; }
static INLINE uint16_t read_uint16(sim_cpu *scpu, uint32_t addr) { uint16_t val = sim_core_read_aligned_2(scpu, CPU_PC_GET(scpu), read_map, addr); record_mem_trace(addr, 16, false, false, val); return val; }
static INLINE uint32_t read_uint32(sim_cpu *scpu, uint32_t addr) { uint32_t val = sim_core_read_aligned_4(scpu, CPU_PC_GET(scpu), read_map, addr); record_mem_trace(addr, 32, false, false, val); return val; }

static INLINE void write_int8 (sim_cpu *scpu, uint32_t addr, uint8_t  val) { record_mem_trace(addr, 8,  false, true, val); sim_core_write_aligned_1(scpu, CPU_PC_GET(scpu), write_map, addr, val); }
static INLINE void write_int16(sim_cpu *scpu, uint32_t addr, uint16_t val) { record_mem_trace(addr, 16, false, true, val); sim_core_write_aligned_2(scpu, CPU_PC_GET(scpu), write_map, addr, val); }
static INLINE void write_int32(sim_cpu *scpu, uint32_t addr, uint32_t val) { record_mem_trace(addr, 32, false, true, val); sim_core_write_aligned_4(scpu, CPU_PC_GET(scpu), write_map, addr, val); }

static INLINE int8_t  read_int8 (sim_cpu *scpu, uint32_t addr) { int8_t  val = (int8_t) sim_core_read_aligned_1(scpu, CPU_PC_GET(scpu), read_map, addr); record_mem_trace(addr, 8,  true, false, val); return val; }
static INLINE int16_t read_int16(sim_cpu *scpu, uint32_t addr) { int16_t val = (int16_t)sim_core_read_aligned_2(scpu, CPU_PC_GET(scpu), read_map, addr); record_mem_trace(addr, 16, true, false, val); return val; }
static INLINE int32_t read_int32(sim_cpu *scpu, uint32_t addr) { int32_t val = (int32_t)sim_core_read_aligned_4(scpu, CPU_PC_GET(scpu), read_map, addr); record_mem_trace(addr, 32, true, false, val); return val; }

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

typedef enum
{
  INSN_CLS_UNKNOWN,
  INSN_CLS_EXCEPTION,
  INSN_CLS_ATOMIC,
  INSN_CLS_MOV,
  INSN_CLS_ALU,
  INSN_CLS_FP,
  INSN_CLS_MUL,
  INSN_CLS_LD,
  INSN_CLS_ST,
  INSN_CLS_CBRANCH,
  INSN_CLS_CBRANCH0,
  INSN_CLS_CBRANCHFP,
  INSN_CLS_CBRANCH0FP,
  INSN_CLS_BBRANCH,
  INSN_CLS_POWER,
  INSN_CLS_NOP,
  INSN_CLS_MAX
} insn_classes;

static const uint16_t INSN_CLS_MASK = 0xff; 
static const uint16_t INSN_FLAG_IMMED = 0x100;

static const char *insn_class_names[] = {
  "unknown",
  "exception/syscall",
  "atomic",
  "move",
  "alu",
  "floating point",
  "multiply",
  "load",
  "store",
  "conditional branch",
  "conditional branch with 0",
  "floating point conditional branch",
  "floating point conditional branch with 0",
  "bit-test conditional branch",
  "power management",
  "nop"
};

static INLINE void brew_trace(sim_cpu *scpu, uint16_t insn_code, unsigned int insn_length, insn_classes insn_cls, const char *fmt, ...)
{
  const char *side_effect_delim = "                                                                                                                ";
  char *pc_name;
  char message[256];
  char fragment[256];
  va_list args;
  bool first = true;
  uint16_t profile_idx;

  if (TRACE_P(scpu, TRACE_INSN_IDX))
    {
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
              if (first)
                {
                  int size_so_far = strlen(message);
                  int side_effect_indent = 80 - size_so_far;
                  if (side_effect_indent < 0) side_effect_indent = 0;
                  #pragma GCC diagnostic push
                  #pragma GCC diagnostic ignored "-Wstringop-truncation"
                  strncat(message, side_effect_delim, MIN(sizeof(message) - strlen(message) - 1, side_effect_indent));
                  #pragma GCC diagnostic pop
                  first = false;
                }
              snprintf(fragment, sizeof(fragment)-1, " %s <- 0x%x", reg_names[i] == NULL ? pc_name : reg_names[i], scpu->regs[i]);
              strncat(message, fragment, sizeof(message) - strlen(message) - 1);
              message[sizeof(message)-1] = 0;
            }
        }
        mem_trace_to_str(&mem_trace, fragment);
        if (fragment[0] != 0)
          {
            if (first)
              {
                  int size_so_far = strlen(message);
                  int side_effect_indent = 80 - size_so_far;
                  if (side_effect_indent < 0) side_effect_indent = 0;
                  #pragma GCC diagnostic push
                  #pragma GCC diagnostic ignored "-Wstringop-truncation"
                  strncat(message, side_effect_delim, MIN(sizeof(message) - strlen(message) - 1, side_effect_indent));
                  #pragma GCC diagnostic pop
                  first = false;
              }
              strncat(message, " | ", sizeof(message) - strlen(message) - 1);
              strncat(message, fragment, sizeof(message) - strlen(message) - 1);
              message[sizeof(message)-1] = 0;
          }
      TRACE_INSN(scpu, "%s", message);
    }
  profile_idx = insn_cls;
  if (insn_length > 2)
    profile_idx |= INSN_FLAG_IMMED;
  PROFILE_COUNT_INSN(scpu, scpu->regs[BREW_REG_PC], profile_idx);
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
#define BREW_TRACE_INST(...) brew_trace(scpu, insn_code, length, __VA_ARGS__)

#define UNKNOWN { \
  char insn_str[25]; \
  if (length == 2) \
    sprintf(insn_str, "UKN 0x%04x", insn_code); \
  else \
    sprintf(insn_str, "UKN 0x%04x 0x%08x", insn_code, field_e); \
  BREW_TRACE_INST(INSN_CLS_UNKNOWN, insn_str); \
  update_pc_before_exception = false; \
  if (scpu->is_task_mode) swap_mode = true; \
  sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGILL); \
}

#define FINAL_ELSE else { \
  char insn_str[80]; \
  if (length == 2) \
    sprintf(insn_str, "FINAL ELSE UKN 0x%04x", insn_code); \
  else \
    sprintf(insn_str, "FINAL ELSE UKN 0x%04x 0x%08x", insn_code, field_e); \
  BREW_TRACE_INST(INSN_CLS_UNKNOWN, insn_str); \
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
      sprintf(branch_target_buffer, "<- 0x%x", field_e);
    }
  else
    {
      /* Relative address */
      sprintf(branch_target_buffer, "<- $pc+0x%x", (int32_t)field_e);
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

static INLINE uint32_t bsi(uint32_t val)
{
  return
    ((val & 0x80) != 0) ?
      (0xffffff00 | (val & 0xff)) :
      (val & 0xff);
}

static INLINE uint32_t wsi(uint32_t val)
{
  return
    ((val & 0x8000) != 0) ?
      (0xffff0000 | (val & 0xffff)) :
      (val & 0xffff);
}

#define TEST_ALIGN(reg, alignment) if (((reg) & (alignment-1)) != 0) sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGBUS);

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

// Copies a string from SIM memory to host memory.
// Returns a pointer to the string, WHICH NEEDS TO BE FREE-d
static char *sim_core_read_str(SIM_DESC sd, sim_cpu *scpu, uint32_t addr)
{
  size_t alloc_size = 1024;
  char *str = (char *)malloc(alloc_size);
  size_t str_len = 0;
  char *c = str;
  SIM_ASSERT(str != NULL);
  do
    {
      *c = sim_core_read_1(scpu, CPU_PC_GET(scpu), read_map, addr);
      if (*c == 0)
        break;
      ++c;
      ++str_len;
      ++addr;
      if (str_len >= alloc_size)
        {
          alloc_size *= 2;
          str = (char *)realloc(str, alloc_size);
          SIM_ASSERT(str != NULL);
          c = str+str_len;
        }
    } while (true);
  return str;
}

#define SET_ERRNO(value) { if (errno_addr != 0) { TEST_ALIGN(errno_addr, 4); write_uint32(scpu, errno_addr, value); } }

static int marshal_o_flags_from_sim(int oflags)
{
  #define  NEWLIB_O_RDONLY    0x000000
  #define  NEWLIB_O_WRONLY    0x000001
  #define  NEWLIB_O_RDWR      0x000002
  #define  NEWLIB_O_APPEND    0x000008
  #define  NEWLIB_O_CREAT     0x000200
  #define  NEWLIB_O_TRUNC     0x000400
  #define  NEWLIB_O_EXCL      0x000800
  #define  NEWLIB_O_SYNC      0x002000
  #define  NEWLIB_O_NONBLOCK  0x004000
  #define  NEWLIB_O_NOCTTY    0x008000
  #define  NEWLIB_O_CLOEXEC   0x040000
  #define  NEWLIB_O_NOFOLLOW  0x100000
  #define  NEWLIB_O_DIRECTORY 0x200000
  //#define  NEWLIB_O_EXEC      0x400000 - no direct equivalent on host?
  int ret_val = 0;
  if ((oflags & NEWLIB_O_RDONLY) != 0)    ret_val |= O_RDONLY;
  if ((oflags & NEWLIB_O_WRONLY) != 0)    ret_val |= O_WRONLY;
  if ((oflags & NEWLIB_O_RDWR) != 0)      ret_val |= O_RDWR;
  if ((oflags & NEWLIB_O_APPEND) != 0)    ret_val |= O_APPEND;
  if ((oflags & NEWLIB_O_CREAT) != 0)     ret_val |= O_CREAT;
  if ((oflags & NEWLIB_O_TRUNC) != 0)     ret_val |= O_TRUNC;
  if ((oflags & NEWLIB_O_EXCL) != 0)      ret_val |= O_EXCL;
  if ((oflags & NEWLIB_O_SYNC) != 0)      ret_val |= O_SYNC;
  if ((oflags & NEWLIB_O_NONBLOCK) != 0)  ret_val |= O_NONBLOCK;
  if ((oflags & NEWLIB_O_NOCTTY) != 0)    ret_val |= O_NOCTTY;
  if ((oflags & NEWLIB_O_CLOEXEC) != 0)   ret_val |= O_CLOEXEC;
  if ((oflags & NEWLIB_O_NOFOLLOW) != 0)  ret_val |= O_NOFOLLOW;
  if ((oflags & NEWLIB_O_DIRECTORY) != 0) ret_val |= O_DIRECTORY;
  return ret_val;
}

static void handle_syscall(SIM_DESC sd, sim_cpu *scpu, uint16_t syscall_no)
{
  // Syscalls follow normal function-call ABI with the addition that
  // $r3 points to 'errno' on entry.
  // The ABI declares that $r8...$r14 needs to maintain it's value
  // accross calls. This applies to SYSCALLs too. $sp and $fp need
  // to be preserved as well. $r3...$r7 can change, though it's quite
  // possible that a true executive implementaton would preserve those
  // as well. In the simulator, we'll only override the return value
  // register(s) and preserve all other.
  //
  // The syscall number is held at the 16-bit address after the SYSCALL
  // instruction. This is the address where the PC points to in real HW,
  // but NOT where it points in the simulator.
  // $pc needs to be advanced by 4 prior returning from the SYSCALL
  // into task mode. Again, the simulator - being task-mode only for now -
  // only needs to worry about setting the PC up appropriately.
  uint32_t errno_addr = scpu->regs[3];
  uint32_t arg1 = scpu->regs[4];
  uint32_t arg2 = scpu->regs[5];
  uint32_t arg3 = scpu->regs[6];
  uint32_t arg4 = scpu->regs[7];
  char *str1;
  char *str2;
  int ret_val;
  int flags;
  REG_TPC_TARGET = REG_TPC + 4; // PC at this point still points
  switch (syscall_no) {
    case SYS_exit:
      // simulated process terminating: let's terminate too!
      sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_exited, arg1);
      //exit(arg1);
      break;
    case SYS_open:
    {
      uint32_t mode;
      str1 = sim_core_read_str(sd, scpu, arg1);
      // We need to map some flags
      flags = marshal_o_flags_from_sim(arg2);
      // mode is a VAARG argument. Even so, those come through registers as well, just as regular arguments.
      mode = arg3;

      fflush(stdout);

      //printf("SIM OPEN: %s with flags: %x, local flags are: %x and mode 0%o\n", str1, arg2, flags, mode);
      ret_val = open(str1, flags, mode);
      //printf("SIM OPEN returned: %d, errno: %d\n", ret_val, errno);
      free(str1);
      scpu->regs[4] = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    }
    case SYS_close:
      if (arg1 > 2)
        ret_val = close(arg1);
      else
        ret_val = 0;
      scpu->regs[4] = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_read: {
      void *buffer = malloc(arg3);
      ret_val = read(arg1, buffer, arg3);
      scpu->regs[4] = ret_val;
      if (ret_val >= 0)
        sim_core_write_buffer(sd, scpu, write_map, buffer, arg2, arg3);
      else
        SET_ERRNO(errno);
      free(buffer);
      break;
    }
    case SYS_write: {
      void *buffer = malloc(arg3);
      sim_core_read_buffer(sd, scpu, write_map, buffer, arg2, arg3);
      ret_val = write(arg1, buffer, arg3);
      scpu->regs[4] = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      free(buffer);
      break;
    }
    case SYS_lseek:
      ret_val = lseek(arg1, arg2, arg3);
      scpu->regs[4] = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_unlink:
      str1 = sim_core_read_str(sd, scpu, arg1);
      ret_val = unlink(str1);
      free(str1);
      scpu->regs[4] = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_getpid:
      // Since we don't simulate multiple processes, we simply return the PID of the simulator.
      scpu->regs[4] = getpid();
      break;
    case SYS_kill:
      ret_val = kill(arg1, arg2);
      scpu->regs[4] = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_fstat: {
      struct stat statbuf;
      ret_val = fstat(arg1, &statbuf);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      // for now, simply return an error...
      ret_val = -1;
      scpu->regs[4] = ret_val;
      SET_ERRNO(EOVERFLOW);
      //SIM_ASSERT(false);
      break;
    }

    // These are not implemented
    case SYS_argvlen:
    case SYS_argv:
      TRACE_EVENTS (scpu, "Unknown SYSCALL: %d", syscall_no);
      sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGTRAP);
      break;
    case SYS_chdir:
      str1 = sim_core_read_str(sd, scpu, arg1);
      ret_val = chdir(str1);
      free(str1);
      scpu->regs[4] = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_stat: {
      struct stat statbuf;
      ret_val = fstat(arg1, &statbuf);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      SIM_ASSERT(false);
      break;
    }
    case SYS_chmod:
      str1 = sim_core_read_str(sd, scpu, arg1);
      ret_val = chmod(str1, arg2);
      free(str1);
      scpu->regs[4] = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_utime: {
      struct utimbuf times;
      str1 = sim_core_read_str(sd, scpu, arg1);
      ret_val = utime(str1, &times);
      free(str1);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      SIM_ASSERT(false);
      break;
    }
    case SYS_time: {
      time_t now = time(NULL);
      scpu->regs[4] = now;
      if (arg1 != 0)
        {
          sim_core_write_aligned_4(scpu, CPU_PC_GET(scpu), write_map, arg1, now);
        }
      break;
    }
    case SYS_gettimeofday: {
      struct timeval time_val;
      struct timezone time_zone;
      ret_val = gettimeofday(&time_val, &time_zone);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      SIM_ASSERT(false);
      break;
    }
    case SYS_times: {
      struct tms time_buf;
      ret_val = times(&time_buf);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      SIM_ASSERT(false);
      break;
    }
    case SYS_link:
      str1 = sim_core_read_str(sd, scpu, arg1);
      str2 = sim_core_read_str(sd, scpu, arg2);
      ret_val = link(str1, str2);
      free(str1);
      free(str2);
      scpu->regs[4] = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;

    // These are not implemented
    case SYS_argc:
    case SYS_argnlen:
    case SYS_argn:
    case SYS_reconfig:
      TRACE_EVENTS (scpu, "Unknown SYSCALL: %d", syscall_no);
      sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGTRAP);
      break;
    // Brew-specific calls:
    case BREW_isatty:
      ret_val = isatty(arg1);
      scpu->regs[4] = ret_val;
      if (ret_val == 0)
        SET_ERRNO(errno);
      break;

    default:
      TRACE_EVENTS (scpu, "Unknown SYSCALL: %d", syscall_no);
      sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGTRAP);
      break;
  }
}

void
sim_engine_run (SIM_DESC sd,
                int next_cpu_nr, /* the CPU to run  */
                int nr_cpus, /* ignore  */
                int siggnal) /* ignore  */
{
  uint16_t insn_code;
  uint32_t field_e = 0xdeadbeef;
  int length;
  sim_cpu *scpu = STATE_CPU (sd, next_cpu_nr);
  int temp_reg_idx; /* Holds a temporary register index for some macros */
  bool swap_mode;
  bool update_pc_before_exception;
  bool cbranch;
  PROFILE_DATA *profile_data = CPU_PROFILE_DATA(scpu);
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
        insn_code = sim_core_read_aligned_2(scpu, pc, exec_map, pc);

        /* Determine if we need field_e and read it if we do */
        if (FIELD_D == 0xf || FIELD_B == 0xf || FIELD_A == 0xf || (insn_code >> 11) == 0x1f) {
          length = 6;
          field_e = sim_core_read_unaligned_4(scpu, pc, exec_map, pc+2);
        }
        scpu->regs[BREW_REG_NEXT_PC] = pc + length; /* Update PC to the next instruction. It's possible that the instruction itself is a branch, in which case it'll be overwritten */
      }
      for (int i=0;i<BREW_NUM_REGS;++i)
        scpu->regs_touch[i] = false;
      cbranch = false;
      if (FIELD_D != 0xf && FIELD_C != 0xf)
        {
          switch (FIELD_C)
            {
            case 0x0: /* ^ and special */
                   if (pattern_match(insn_code, "0000")) { BREW_TRACE_INST(INSN_CLS_EXCEPTION, "FILL"); if (scpu->is_task_mode) swap_mode = true; sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGILL); }
              else if (pattern_match(insn_code, "0110")) { BREW_TRACE_INST(INSN_CLS_EXCEPTION, "BREAK"); if (scpu->is_task_mode) swap_mode = true; sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGTRAP); }
              else if (pattern_match(insn_code, "0220")) { uint32_t pc = (CPU_PC_GET(scpu)&~1); uint16_t syscall_no = sim_core_read_aligned_2(scpu, pc, exec_map, pc+2); BREW_TRACE_INST(INSN_CLS_EXCEPTION, "SYSCALL %d", syscall_no); handle_syscall(sd, scpu, syscall_no); }
              else if (pattern_match(insn_code, "0330")) { if (scpu->is_task_mode) { swap_mode = true; NEXT_INST(INSN_CLS_EXCEPTION, "STU"); } else sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGABRT); } /* Return to task mode --> NOP in task mode */
              else if (pattern_match(insn_code, "0440")) { UNKNOWN; } /* This is actually known, it is the official SII exception */ 
              else if (pattern_match(insn_code, "0dd0")) { NEXT_INST(INSN_CLS_ATOMIC, "FENCE"); }
              else if (pattern_match(insn_code, "0ee0")) { NEXT_INST(INSN_CLS_ATOMIC, "WFENCE"); }
              else if (pattern_match(insn_code, "0..0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "0ff.")) { REG_D_TARGET = field_e; NEXT_INST(INSN_CLS_MOV, "%s <- %u (0x%x)", trREG_D, field_e, field_e); }
              else if (pattern_match(insn_code, "0f..")) { UNKNOWN; }
              else if (pattern_match(insn_code, "0.f.")) { REG_D_TARGET = REG_B ^ field_e; NEXT_INST(INSN_CLS_ALU, "%s <- %s ^ 0x%x", trREG_D, trREG_B, field_e); }
              else if (pattern_match(insn_code, "0...")) { REG_D_TARGET = REG_B ^ REG_A; NEXT_INST(INSN_CLS_ALU, "%s <- %s ^ %s", trREG_D, trREG_B, trREG_A); }
              FINAL_ELSE;
              break;
            case 0x1:
                   if (pattern_match(insn_code, "1000")) { BREW_TRACE_INST(INSN_CLS_POWER, "WOI"); sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGILL); }
              else if (pattern_match(insn_code, "1111")) { NEXT_INST(INSN_CLS_NOP, "NOP"); }
              else if (pattern_match(insn_code, "1f..")) { UNKNOWN; }
              else if (pattern_match(insn_code, "1.f.")) { REG_D_TARGET = REG_B | field_e; NEXT_INST(INSN_CLS_ALU, "%s <- %s | 0x%x", trREG_D, trREG_B, field_e); }
              else if (pattern_match(insn_code, "1...")) { REG_D_TARGET = REG_B | REG_A; NEXT_INST(INSN_CLS_ALU, "%s <- %s | %s", trREG_D, trREG_B, trREG_A); }
              FINAL_ELSE;
              break;
            case 0x2:
                   if (pattern_match(insn_code, "2f..")) { UNKNOWN; }
              else if (pattern_match(insn_code, "2.f.")) { REG_D_TARGET = REG_B & field_e; NEXT_INST(INSN_CLS_ALU, "%s <- %s & 0x%x", trREG_D, trREG_B, field_e); }
              else if (pattern_match(insn_code, "2...")) { REG_D_TARGET = REG_B & REG_A; NEXT_INST(INSN_CLS_ALU, "%s <- %s & %s", trREG_D, trREG_B, trREG_A); }
              FINAL_ELSE;
              break;
            case 0x3:
                   if (pattern_match(insn_code, "3f..")) { REG_D_TARGET = field_e - REG_A; NEXT_INST(INSN_CLS_ALU, "%s <- %u - %s", trREG_D, field_e, trREG_A); }
              else if (pattern_match(insn_code, "3.f.")) { REG_D_TARGET = REG_B - field_e; NEXT_INST(INSN_CLS_ALU, "%s <- %s - %u", trREG_D, trREG_B, field_e); }
              else if (pattern_match(insn_code, "3...")) { REG_D_TARGET = REG_B - REG_A; NEXT_INST(INSN_CLS_ALU, "%s <- %s - %s", trREG_D, trREG_B, trREG_A); }
              FINAL_ELSE;
              break;
            case 0x4:
                   if (pattern_match(insn_code, "4f..")) { UNKNOWN; }
              else if (pattern_match(insn_code, "4.f.")) { REG_D_TARGET = REG_B + field_e; NEXT_INST(INSN_CLS_ALU, "%s <- %s + %u", trREG_D, trREG_B, field_e); }
              else if (pattern_match(insn_code, "4...")) { REG_D_TARGET = REG_B + REG_A; NEXT_INST(INSN_CLS_ALU, "%s <- %s + %s", trREG_D, trREG_B, trREG_A); }
              FINAL_ELSE;
              break;
            case 0x5:
                   if (pattern_match(insn_code, "5f..")) { REG_D_TARGET = field_e << (REG_A & 31); NEXT_INST(INSN_CLS_ALU, "%s <- %u << %s", trREG_D, field_e, trREG_A); }
              else if (pattern_match(insn_code, "5.f.")) { REG_D_TARGET = REG_B << (field_e & 31); NEXT_INST(INSN_CLS_ALU, "%s <- %s << %u", trREG_D, trREG_B, field_e); }
              else if (pattern_match(insn_code, "5...")) { REG_D_TARGET = REG_B << (REG_A & 31); NEXT_INST(INSN_CLS_ALU, "%s <- %s << %s", trREG_D, trREG_B, trREG_A); }
              FINAL_ELSE;
              break;
            case 0x6:
                   if (pattern_match(insn_code, "6f..")) { REG_D_TARGET = field_e >> (REG_A & 31); NEXT_INST(INSN_CLS_ALU, "%s <- %u >> %s", trREG_D, field_e, trREG_A); }
              else if (pattern_match(insn_code, "6.f.")) { REG_D_TARGET = REG_B >> (field_e & 31); NEXT_INST(INSN_CLS_ALU, "%s <- %s >> %u", trREG_D, trREG_B, field_e); }
              else if (pattern_match(insn_code, "6...")) { REG_D_TARGET = REG_B >> (REG_A & 31); NEXT_INST(INSN_CLS_ALU, "%s <- %s >> %s", trREG_D, trREG_B, trREG_A); }
              FINAL_ELSE;
              break;
            case 0x7:
                   if (pattern_match(insn_code, "7f..")) { REG_D_TARGET = ((int32_t)field_e) >> (REG_A & 31); NEXT_INST(INSN_CLS_ALU, "%s <- %d >> %s", trSREG_D, (int32_t)field_e, trREG_A); }
              else if (pattern_match(insn_code, "7.f.")) { REG_D_TARGET = ((int32_t)REG_B) >> (field_e & 31); NEXT_INST(INSN_CLS_ALU, "%s <- %s >> %u", trSREG_D, trSREG_B, field_e); }
              else if (pattern_match(insn_code, "7...")) { REG_D_TARGET = ((int32_t)REG_B) >> (REG_A & 31); NEXT_INST(INSN_CLS_ALU, "%s <- %s >> %s", trSREG_D, trSREG_B, trREG_A); }
              FINAL_ELSE;
              break;
            case 0x8:
                   if (pattern_match(insn_code, "8f..")) { UNKNOWN; }
              else if (pattern_match(insn_code, "8ff.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "80f.")) { TREG_D_TARGET = field_e; NEXT_INST(INSN_CLS_MOV, "%s <- %u", trTREG_D, field_e); }
              else if (pattern_match(insn_code, "8.f.")) { REG_D_TARGET = REG_B * field_e; NEXT_INST(INSN_CLS_MUL, "%s <- %s * %u", trREG_D, trREG_B, field_e); }
              else if (pattern_match(insn_code, "80..")) { TREG_D_TARGET = TREG_A; NEXT_INST(INSN_CLS_MOV, "%s <- %s", trTREG_D, trTREG_A); }
              else if (pattern_match(insn_code, "8.00")) { UNKNOWN; }
              else if (pattern_match(insn_code, "8...")) { REG_D_TARGET = REG_B * REG_A; NEXT_INST(INSN_CLS_MUL, "%s <- %s * %s", trREG_D, trREG_B, trREG_A); }
              FINAL_ELSE;
              break;
            case 0x9:
                   if (pattern_match(insn_code, "9f..")) { UNKNOWN; }
              else if (pattern_match(insn_code, "9ff.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "90f.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "9.f0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "9.f.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "900.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "90..")) { REG_D_TARGET = REG_A + 1; NEXT_INST(INSN_CLS_ALU, "%s <- %s + 1", trREG_D, trREG_A); }
              else if (pattern_match(insn_code, "9.0.")) { REG_D_TARGET = REG_B - 1; NEXT_INST(INSN_CLS_ALU, "%s <- %s - 1", trREG_D, trREG_B); }
              else if (pattern_match(insn_code, "9..0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "9...")) { UNKNOWN; }
              FINAL_ELSE;
              break;
            case 0xa:
                   if (pattern_match(insn_code, "af..")) { UNKNOWN; }
              else if (pattern_match(insn_code, "aff.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "a0f.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "a.f0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "a.f.")) { REG_D_TARGET = (((uint64_t)REG_B) * ((uint64_t)field_e)) >> 32; NEXT_INST(INSN_CLS_MUL, "%s <- upper %s * %u", trREG_D, trREG_B, field_e); }
              else if (pattern_match(insn_code, "a00.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "a0..")) { REG_D_TARGET = -(int32_t)REG_A; NEXT_INST(INSN_CLS_ALU, "%s <- -%s", trSREG_D, trSREG_A); }
              else if (pattern_match(insn_code, "a.0.")) { REG_D_TARGET = ~REG_B; NEXT_INST(INSN_CLS_ALU, "%s <- ~%s", trREG_D, trREG_B); }
              else if (pattern_match(insn_code, "a..0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "a...")) { REG_D_TARGET = (((uint64_t)REG_B) * ((uint64_t)REG_A)) >> 32; NEXT_INST(INSN_CLS_MUL, "%s <- upper %s * %s", trREG_D, trREG_B, trREG_A); }
              FINAL_ELSE;
              break;
            case 0xb:
                   if (pattern_match(insn_code, "bf..")) { UNKNOWN; }
              else if (pattern_match(insn_code, "bff.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "b0f.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "b.f0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "b.f.")) { REG_D_TARGET = (((int64_t)(int32_t)REG_B) * ((int64_t)(int32_t)field_e)) >> 32; NEXT_INST(INSN_CLS_MUL, "%s <- %s * %d", trSREG_D, trSREG_B, (int32_t)field_e); }
              else if (pattern_match(insn_code, "b00.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "b0..")) { REG_D_TARGET = bswap(REG_A); NEXT_INST(INSN_CLS_ALU, "%s <- bswap %s", trREG_D, trREG_A); }
              else if (pattern_match(insn_code, "b.0.")) { REG_D_TARGET = wswap(REG_B); NEXT_INST(INSN_CLS_ALU, "%s <- wswap %s", trREG_D, trREG_B); }
              else if (pattern_match(insn_code, "b..0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "b...")) { REG_D_TARGET = (((int64_t)(int32_t)REG_B) * ((int64_t)(int32_t)REG_A)) >> 32; NEXT_INST(INSN_CLS_MUL, "%s <- %s * %s", trSREG_D, trSREG_B, trSREG_A); }
              FINAL_ELSE;
              break;
            case 0xc:
                   if (pattern_match(insn_code, "cf..")) { UNKNOWN; }
              else if (pattern_match(insn_code, "c0f.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "c.f0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "cff.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "c.f.")) { REG_D_TARGET = as_int(as_float(REG_B) + as_float(field_e)); NEXT_INST(INSN_CLS_FP, "%s <- %s + %f", trFREG_D, trFREG_B, as_float(field_e)); }
              else if (pattern_match(insn_code, "c..0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "c00.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "c.0.")) { REG_D_TARGET = wsi(REG_B); NEXT_INST(INSN_CLS_ALU, "%s <- wsi %s", trREG_D, trREG_B); }
              else if (pattern_match(insn_code, "c0..")) { REG_D_TARGET = bsi(REG_A); NEXT_INST(INSN_CLS_ALU, "%s <- bsi %s", trREG_D, trREG_A); }
              else if (pattern_match(insn_code, "c...")) { REG_D_TARGET = as_int(as_float(REG_B) + as_float(REG_A)); NEXT_INST(INSN_CLS_FP, "%s <- %s + %s", trFREG_D, trFREG_B, trFREG_A); }
              FINAL_ELSE;
              break;
            case 0xd:
                   if (pattern_match(insn_code, "df..")) { REG_D_TARGET = as_int(as_float(field_e) + as_float(REG_A)); NEXT_INST(INSN_CLS_FP, "%s <- %f - %s", trFREG_D, as_float(field_e), trFREG_A); }
              else if (pattern_match(insn_code, "d0f.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "d.f0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "dff.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "d.f.")) { REG_D_TARGET = as_int(as_float(REG_B) - as_float(field_e)); NEXT_INST(INSN_CLS_FP, "%s <- %s - %f", trFREG_D, trFREG_B, as_float(field_e)); }
              else if (pattern_match(insn_code, "d..0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "d00.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "d.0.")) { REG_D_TARGET = as_int((float)REG_B); NEXT_INST(INSN_CLS_FP, "%s <- %s", trFREG_D, trSREG_B); }
              else if (pattern_match(insn_code, "d0..")) { REG_D_TARGET = (int)floorf(as_float(REG_A)); NEXT_INST(INSN_CLS_FP, "%s <- floor %s", trSREG_D, trFREG_A); }
              else if (pattern_match(insn_code, "d...")) { REG_D_TARGET = as_int(as_float(REG_B) - as_float(REG_A)); NEXT_INST(INSN_CLS_FP, "%s <- %s - %s", trFREG_D, trFREG_B, trFREG_A); }
              FINAL_ELSE;
              break;
            case 0xe:
                   if (pattern_match(insn_code, "ef..")) { UNKNOWN; }
              else if (pattern_match(insn_code, "e0f.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "e.f0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "eff.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "e.f.")) { REG_D_TARGET = as_int(as_float(REG_B) * as_float(field_e)); NEXT_INST(INSN_CLS_FP, "%s <- %s * %f", trFREG_D, trFREG_B, as_float(field_e)); }
              else if (pattern_match(insn_code, "e..0")) { UNKNOWN; }
              else if (pattern_match(insn_code, "e00.")) { UNKNOWN; }
              else if (pattern_match(insn_code, "e.0.")) { if (REG_A <= 0.0f) { if (scpu->is_task_mode) { swap_mode = true; update_pc_before_exception = false; } sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGFPE); } REG_D_TARGET = as_int(1.0f / sqrtf(as_float(REG_B))); NEXT_INST(INSN_CLS_FP, "%s <- rsqrt %s", trFREG_D, trFREG_B); }
              else if (pattern_match(insn_code, "e0..")) { if (REG_A == 0.0f) { if (scpu->is_task_mode) { swap_mode = true; update_pc_before_exception = false; } sim_engine_halt(sd, scpu, NULL, scpu->regs[BREW_REG_PC], sim_stopped, SIM_SIGFPE); } REG_D_TARGET = as_int(1.0f / as_float(REG_A)); NEXT_INST(INSN_CLS_FP, "%s <- 1 / %s", trFREG_D, trFREG_A); }
              else if (pattern_match(insn_code, "e...")) { REG_D_TARGET = as_int(as_float(REG_B) * as_float(REG_A)); NEXT_INST(INSN_CLS_FP, "%s <- %s * %s", trFREG_D, trFREG_B, trFREG_A); }
              FINAL_ELSE;
              break;
            default:
              SIM_ASSERT(false);
            }
        }
      else if (FIELD_C == 0xf)
        {
               if (pattern_match(insn_code, "f0..")) { REG_D_TARGET = (int32_t)read_int8(scpu, REG_A); NEXT_INST(INSN_CLS_LD, "%s <- MEM8[%s]", trSREG_D, trREG_A); }
          else if (pattern_match(insn_code, "f1..")) { REG_D_TARGET = read_uint8(scpu, REG_A); NEXT_INST(INSN_CLS_LD, "%s <- MEM8[%s]", trREG_D, trREG_A); }
          else if (pattern_match(insn_code, "f2..")) { TEST_ALIGN(REG_A, 2); REG_D_TARGET = (int32_t)read_int16(scpu, REG_A); NEXT_INST(INSN_CLS_LD, "%s <- MEM16[%s]", trSREG_D, trREG_A); }
          else if (pattern_match(insn_code, "f3..")) { TEST_ALIGN(REG_A, 2); REG_D_TARGET = read_uint16(scpu, REG_A); NEXT_INST(INSN_CLS_LD, "%s <- MEM16[%s]", trREG_D, trREG_A); }
          else if (pattern_match(insn_code, "f4..")) { TEST_ALIGN(REG_A, 4); REG_D_TARGET = read_uint32(scpu, REG_A); NEXT_INST(INSN_CLS_LD, "%s <- MEM32[%s]", trREG_D, trREG_A); }
          else if (pattern_match(insn_code, "f5..")) { write_uint8(scpu, REG_A, REG_D); NEXT_INST(INSN_CLS_ST, "MEM8[%s] <- %s", trREG_A, trREG_D); }
          else if (pattern_match(insn_code, "f6..")) { TEST_ALIGN(REG_A, 2); write_uint16(scpu, REG_A, REG_D); NEXT_INST(INSN_CLS_ST, "MEM16[%s] <- %s", trREG_A, trREG_D); }
          else if (pattern_match(insn_code, "f7..")) { TEST_ALIGN(REG_A, 4); write_uint32(scpu, REG_A, REG_D); NEXT_INST(INSN_CLS_ST, "MEM32[%s] <- %s", trREG_A, trREG_D); }
          else if (pattern_match(insn_code, "f7.f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "f7f.")) { UNKNOWN; }

          else if (pattern_match(insn_code, "f8f.")) { REG_D_TARGET = (int32_t)read_int8(scpu, field_e); NEXT_INST(INSN_CLS_LD, "%s <- MEM8[0x%x]", trSREG_D, field_e); }
          else if (pattern_match(insn_code, "f8ff")) { REG_TPC_TARGET = (int32_t)read_int8(scpu, field_e); NEXT_INST(INSN_CLS_LD, "$stpc <- MEM8[0x%x]", field_e); }
          else if (pattern_match(insn_code, "f9f.")) { REG_D_TARGET = read_uint8(scpu, field_e); NEXT_INST(INSN_CLS_LD, "%s <- MEM8[0x%x]", trREG_D, field_e); }
          else if (pattern_match(insn_code, "f9ff")) { REG_TPC_TARGET = read_uint8(scpu, field_e); NEXT_INST(INSN_CLS_LD, "$tpc <- MEM8[0x%x]", field_e); }
          else if (pattern_match(insn_code, "faf.")) { TEST_ALIGN(field_e, 2); REG_D_TARGET = (int32_t)read_int16(scpu, field_e); NEXT_INST(INSN_CLS_LD, "%s <- MEM16[0x%x]", trSREG_D, field_e); }
          else if (pattern_match(insn_code, "faff")) { TEST_ALIGN(field_e, 2); REG_TPC_TARGET = (int32_t)read_int16(scpu, field_e); NEXT_INST(INSN_CLS_LD, "$stpc <- MEM16[0x%x]", field_e); }
          else if (pattern_match(insn_code, "fbf.")) { TEST_ALIGN(field_e, 2); REG_D_TARGET = read_uint16(scpu, field_e); NEXT_INST(INSN_CLS_LD, "%s <- MEM16[0x%x]", trREG_D, field_e); }
          else if (pattern_match(insn_code, "fbff")) { TEST_ALIGN(field_e, 2); REG_TPC_TARGET = read_uint16(scpu, field_e); NEXT_INST(INSN_CLS_LD, "$tpc <- MEM16[0x%x]", field_e); }
          else if (pattern_match(insn_code, "fcf.")) { TEST_ALIGN(field_e, 4); REG_D_TARGET = read_uint32(scpu, field_e); NEXT_INST(INSN_CLS_LD, "%s <- MEM32[0x%x]", trREG_D, field_e); }
          else if (pattern_match(insn_code, "fcff")) { TEST_ALIGN(field_e, 4); REG_TPC_TARGET = read_uint32(scpu, field_e); NEXT_INST(INSN_CLS_LD, "$tpc <- MEM32[0x%x]", field_e); }
          else if (pattern_match(insn_code, "fdf.")) { write_uint8(scpu, field_e, REG_D); NEXT_INST(INSN_CLS_ST, "MEM8[0x%x] <- %s", field_e, trREG_D); }
          else if (pattern_match(insn_code, "fdff")) { write_uint8(scpu, field_e, REG_TPC); NEXT_INST(INSN_CLS_ST, "MEM8[0x%x] <- $tpc", field_e); }
          else if (pattern_match(insn_code, "fef.")) { TEST_ALIGN(field_e, 2); write_uint16(scpu, field_e, REG_D); NEXT_INST(INSN_CLS_ST, "MEM16[0x%x] <- %s", field_e, trREG_D); }
          else if (pattern_match(insn_code, "feff")) { TEST_ALIGN(field_e, 2); write_uint16(scpu, field_e, REG_TPC); NEXT_INST(INSN_CLS_ST, "MEM16[0x%x] <- $tpc", field_e); }
          else if (pattern_match(insn_code, "fff.")) { TEST_ALIGN(field_e, 4); write_uint32(scpu, field_e, REG_D); NEXT_INST(INSN_CLS_ST, "MEM32[0x%x] <- %s", field_e, trREG_D); }
          else if (pattern_match(insn_code, "ffff")) { TEST_ALIGN(field_e, 4); write_uint32(scpu, field_e, REG_TPC); NEXT_INST(INSN_CLS_ST, "MEM32[0x%x] <- $tpc", field_e); }

          else if (pattern_match(insn_code, "f8..")) { REG_D_TARGET = (int32_t)read_int8(scpu, REG_A+field_e); NEXT_INST(INSN_CLS_LD, "%s <- MEM8[%s,0x%x]", trSREG_D, trREG_A, field_e); }
          else if (pattern_match(insn_code, "f8.f")) { REG_TPC_TARGET = (int32_t)read_int8(scpu, REG_A+field_e); NEXT_INST(INSN_CLS_LD, "$stpc <- MEM8[%s,0x%x]", trREG_A, field_e); }
          else if (pattern_match(insn_code, "f9..")) { REG_D_TARGET = read_uint8(scpu, REG_A+field_e); NEXT_INST(INSN_CLS_LD, "%s <- MEM8[%s,0x%x]", trREG_D, trREG_A, field_e); }
          else if (pattern_match(insn_code, "f9.f")) { REG_TPC_TARGET = read_uint8(scpu, REG_A+field_e); NEXT_INST(INSN_CLS_LD, "$tpc <- MEM8[%s,0x%x]", trREG_A, field_e); }
          else if (pattern_match(insn_code, "fa..")) { TEST_ALIGN(REG_A+field_e, 2); REG_D_TARGET = (int32_t)read_int16(scpu, REG_A+field_e); NEXT_INST(INSN_CLS_LD, "%s <- MEM16[%s,0x%x]", trSREG_D, trREG_A, field_e); }
          else if (pattern_match(insn_code, "fa.f")) { TEST_ALIGN(REG_A+field_e, 2); REG_TPC_TARGET = (int32_t)read_int16(scpu, REG_A+field_e); NEXT_INST(INSN_CLS_LD, "$stpc <- MEM16[%s,0x%x]", trREG_A, field_e); }
          else if (pattern_match(insn_code, "fb..")) { TEST_ALIGN(REG_A+field_e, 2); REG_D_TARGET = read_uint16(scpu, REG_A+field_e); NEXT_INST(INSN_CLS_LD, "%s <- MEM16[%s,0x%x]", trREG_D, trREG_A, field_e); }
          else if (pattern_match(insn_code, "fb.f")) { TEST_ALIGN(REG_A+field_e, 2); REG_TPC_TARGET = read_uint16(scpu, REG_A+field_e); NEXT_INST(INSN_CLS_LD, "$tpc <- MEM16[%s,0x%x]", trREG_A, field_e); }
          else if (pattern_match(insn_code, "fc..")) { TEST_ALIGN(REG_A+field_e, 4); REG_D_TARGET = read_uint32(scpu, REG_A+field_e); NEXT_INST(INSN_CLS_LD, "%s <- MEM32[%s,0x%x]", trREG_D, trREG_A, field_e); }
          else if (pattern_match(insn_code, "fc.f")) { TEST_ALIGN(REG_A+field_e, 4); REG_TPC_TARGET = read_uint32(scpu, REG_A+field_e); NEXT_INST(INSN_CLS_LD, "$tpc <- MEM32[%s,0x%x]", trREG_A, field_e); }
          else if (pattern_match(insn_code, "fd..")) { write_uint8(scpu, REG_A+field_e, REG_D); NEXT_INST(INSN_CLS_ST, "MEM8[%s,0x%x] <- %s", trREG_A, field_e, trREG_D); }
          else if (pattern_match(insn_code, "fd.f")) { write_uint8(scpu, REG_A+field_e, REG_TPC); NEXT_INST(INSN_CLS_ST, "MEM8[%s,0x%x] <- $tpc", trREG_A, field_e); }
          else if (pattern_match(insn_code, "fe..")) { TEST_ALIGN(REG_A+field_e, 2); write_uint16(scpu, REG_A+field_e, REG_D); NEXT_INST(INSN_CLS_ST, "MEM16[%s,0x%x] <- %s", trREG_A, field_e, trREG_D); }
          else if (pattern_match(insn_code, "fe.f")) { TEST_ALIGN(REG_A+field_e, 2); write_uint16(scpu, REG_A+field_e, REG_TPC); NEXT_INST(INSN_CLS_ST, "MEM16[%s,0x%x] <- $tpc", trREG_A, field_e); }
          else if (pattern_match(insn_code, "ff..")) { TEST_ALIGN(REG_A+field_e, 4); write_uint32(scpu, REG_A+field_e, REG_D); NEXT_INST(INSN_CLS_ST, "MEM32[%s,0x%x] <- %s", trREG_A, field_e, trREG_D); }
          else if (pattern_match(insn_code, "ff.f")) { TEST_ALIGN(REG_A+field_e, 4); write_uint32(scpu, REG_A+field_e, REG_TPC); NEXT_INST(INSN_CLS_ST, "MEM32[%s,0x%x] <- $tpc", trREG_A, field_e); }
          FINAL_ELSE;
        }
      else
        {
               if (pattern_match(insn_code, "0.0f")) { cbranch = true; if (REG_B == 0) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH0, "if %s == 0 $pc %s", trREG_B, branch_target(field_e)); }
          else if (pattern_match(insn_code, "0.1f")) { cbranch = true; if (REG_B != 0) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH0, "if %s != 0 $pc %s", trREG_B, branch_target(field_e)); }
          else if (pattern_match(insn_code, "0.2f")) { cbranch = true; if ((int32_t)REG_B < 0) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH0, "if %s < 0 $pc %s", trSREG_B, branch_target(field_e)); }
          else if (pattern_match(insn_code, "0.3f")) { cbranch = true; if ((int32_t)REG_B >= 0) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH0, "if %s >= 0 $pc %s", trSREG_B, branch_target(field_e)); }
          else if (pattern_match(insn_code, "0.4f")) { cbranch = true; if ((int32_t)REG_B > 0) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH0, "if %s > 0 $pc %s", trSREG_B, branch_target(field_e)); }
          else if (pattern_match(insn_code, "0.5f")) { cbranch = true; if ((int32_t)REG_B <= 0) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH0, "if %s <= 0 $pc %s", trSREG_B, branch_target(field_e)); }
          else if (pattern_match(insn_code, "0.6f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "0.7f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "0.8f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "0.9f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "0.af")) { UNKNOWN; }
          else if (pattern_match(insn_code, "0.bf")) { cbranch = true; if (as_float(REG_B) < 0.0f) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH0FP, "if %s < 0 $pc %s", trFREG_B, branch_target(field_e)); }
          else if (pattern_match(insn_code, "0.cf")) { cbranch = true; if (as_float(REG_B) >= 0.0f) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH0FP, "if %s >= 0 $pc %s", trFREG_B, branch_target(field_e)); }
          else if (pattern_match(insn_code, "0.df")) { cbranch = true; if (as_float(REG_B) > 0.0f) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH0FP, "if %s > 0 $pc %s", trFREG_B, branch_target(field_e)); }
          else if (pattern_match(insn_code, "0.ef")) { cbranch = true; if (as_float(REG_B) <= 0.0f) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH0FP, "if %s <= 0 $pc %s", trFREG_B, branch_target(field_e)); }

          else if (pattern_match(insn_code, "1..f")) { cbranch = true; if (REG_B == REG_A) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH, "if %s == %s $pc %s", trREG_B, trREG_A, branch_target(field_e)); }
          else if (pattern_match(insn_code, "2..f")) { cbranch = true; if (REG_B != REG_A) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH, "if %s != %s $pc %s", trREG_B, trREG_A, branch_target(field_e)); }
          else if (pattern_match(insn_code, "3..f")) { cbranch = true; if ((int32_t)REG_B < (int32_t)REG_A) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH, "if %s < %s $pc %s", trSREG_B, trSREG_A, branch_target(field_e)); }
          else if (pattern_match(insn_code, "4..f")) { cbranch = true; if ((int32_t)REG_B >= (int32_t)REG_A) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH, "if %s >= %s $pc %s", trSREG_B, trSREG_A, branch_target(field_e)); }
          else if (pattern_match(insn_code, "5..f")) { cbranch = true; if (REG_B < REG_A) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH, "if %s < %s $pc %s", trREG_B, trREG_A, branch_target(field_e)); }
          else if (pattern_match(insn_code, "6..f")) { cbranch = true; if (REG_B >= REG_A) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCH, "if %s >= %s $pc %s", trREG_B, trREG_A, branch_target(field_e)); }
          else if (pattern_match(insn_code, "7..f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "8..f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "9..f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "a..f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "b..f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "c..f")) { UNKNOWN; }
          else if (pattern_match(insn_code, "d..f")) { cbranch = true; if (as_float(REG_B) < as_float(REG_A)) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCHFP, "if %s < %s $pc %s", trFREG_B, trFREG_A, branch_target(field_e)); }
          else if (pattern_match(insn_code, "e..f")) { cbranch = true; if (as_float(REG_B) >= as_float(REG_A)) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_CBRANCHFP, "if %s >= %s $pc %s", trFREG_B, trFREG_A, branch_target(field_e)); }

          else if (pattern_match(insn_code, ".f.f")) { cbranch = true; if ((REG_A & (1 << FIELD_C)) != 0) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_BBRANCH, "if %s[%d] == 1 $pc %s", trREG_A, FIELD_C, branch_target(field_e)); }
          else if (pattern_match(insn_code, "..ff")) { cbranch = true; if ((REG_A & (1 << FIELD_C)) == 0) branch_to(scpu, field_e); NEXT_INST(INSN_CLS_BBRANCH, "if %s[%d] == 0 $pc %s", trREG_B, FIELD_C, branch_target(field_e)); }
          FINAL_ELSE;
        }

    next_inst:
      /* TODO: for now we don't support scheduler mode */
      swap_mode = false;

      if (cbranch)
        {
          if (scpu->regs_touch[BREW_REG_NEXT_PC])
            {
              ++PROFILE_MODEL_TAKEN_COUNT(profile_data);
            }
          else
            {
              ++PROFILE_MODEL_UNTAKEN_COUNT(profile_data);
            }
        }
      else
        {
          // Not all branches are conditional. If we - through any means - changed the next PC, that's a branch.
          if (scpu->regs_touch[BREW_REG_NEXT_PC])
            {
              ++PROFILE_MODEL_TAKEN_COUNT(profile_data);
            }
        }
      ++PROFILE_TOTAL_INSN_COUNT(profile_data);
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

/* Given the instruction code, return a descriptive string */
static const char *
brew_insn_name (SIM_CPU *scpu, int insn_code)
{
  static char insn_name[200];
  uint16_t insn_cls = insn_code & INSN_CLS_MASK;

  if (insn_cls >= sizeof(insn_class_names)/sizeof(insn_class_names[0]))
    return "---";
  sprintf(insn_name, "%s%s", insn_class_names[insn_cls], (insn_code & INSN_FLAG_IMMED) ? " immediate": "");
  //sprintf(insn_name, "INST %04x", insn_code);
  return insn_name;
}

static void
brew_model_init(SIM_CPU *cpu)
{
  CPU_MODEL_DATA(cpu) = NULL; // TODO: Not sure what this is, but hopefully it could be set to NULL. It appears to be something that's target specific.
}

static void
brew_init_cpu (SIM_CPU *scpu)
{
  CPU_REG_FETCH(scpu) = brew_reg_fetch;
  CPU_REG_STORE(scpu) = brew_reg_store;
  CPU_PC_FETCH(scpu) = brew_pc_get;
  CPU_PC_STORE(scpu) = brew_pc_set;
  CPU_MAX_INSNS(scpu) = 0x10000; // To make is simple, every instruction code is counted separately (for now)
  CPU_INSN_NAME(scpu) = brew_insn_name;
  set_initial_gprs(scpu);        /* Reset the GPR registers.  */
}

static void
brew_prepare_run (SIM_CPU *cpu)
{
}

static const SIM_MACH_IMP_PROPERTIES brew_imp_properties =
{
  sizeof(SIM_CPU),
  0,
};

static const SIM_MODEL brew_models[];

#define MACH_BREW (0) // I don't think this is used
static const SIM_MACH brew_mach =
{
  "brew", "brew", MACH_BREW,
  32, 32, &brew_models[0], &brew_imp_properties,
  brew_init_cpu,
  brew_prepare_run
};

static const SIM_MODEL brew_models[] =
{
  { "brew", &brew_mach, 0, NULL, brew_model_init},
  { 0, NULL, 0, NULL, NULL, }
};

const SIM_MACH * const brew_sim_machs[] =
{
  &brew_mach,
  NULL
};

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
  STATE_MACHS(sd) = brew_sim_machs;

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
  sim_do_command(sd," memory region 0x00000000,0x80000000"); /* main memory for program storage */
  //sim_do_command(sd," memory region 0xE0000000,0x10000"); /* this is where the DTB goes, if loaded */

  /* Check for/establish the a reference program image.  */
  if(sim_analyze_program (sd, STATE_PROG_FILE (sd), abfd) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* Configure/verify the target byte order and other runtime
     configuration options.  */
  if(sim_config (sd) != SIM_RC_OK)
    {
      sim_module_uninstall (sd);
      return 0;
    }

  if(sim_post_argv_init (sd) != SIM_RC_OK)
    {
      /* Uninstall the modules to avoid memory leaks,
         file descriptor leaks, etc.  */
      sim_module_uninstall (sd);
      return 0;
    }

  /* CPU specific initialization. */
  /* That is: setting up callbacks for load/store registers and for get/set of PC */
//  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
//    {
//      sim_cpu *scpu = STATE_CPU (sd, i);
//
//      brew_init_cpu(scpu);
//    }

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
