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
#include <math.h>
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
#include "opcode/brew.h"
#include "opcode/brew_abi.h"
#include "gdb-if.h"

static int32_t
sim_floor(float num)
{
  return (int32_t)floorf(num);
}

static float
rsqrt(float num)
{
  return 1.0f / sqrtf(num);
}

static INLINE void record_mem_trace(sim_cpu *scpu, uint32_t addr, int access_size, bool is_write, uint32_t value)
{
  scpu->mem_trace.addr = addr;
  scpu->mem_trace.is_write = is_write;
  scpu->mem_trace.access_size = access_size;
  scpu->mem_trace.value = value;
  scpu->mem_trace.is_valid = true;
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

static brew_exception_type
read_mem(void *context, uint32_t vma, int length, uint32_t *value)
{
  sim_cpu *scpu = (sim_cpu*)context;
  SIM_DESC sd = CPU_STATE(scpu);

  switch (length)
    {
    case 8:
      *value = sim_core_read_aligned_1(scpu, CPU_PC_GET(scpu), read_map, vma);
      break;
    case 16:
      if ((vma & 1) != 0)
        return BREW_EXCEPTION_UNALIGNED;
      *value = sim_core_read_aligned_2(scpu, CPU_PC_GET(scpu), read_map, vma);
      break;
    case 32:
      if ((vma & 3) != 0)
        return BREW_EXCEPTION_UNALIGNED;
      *value = sim_core_read_aligned_4(scpu, CPU_PC_GET(scpu), read_map, vma);
      break;
    default:
      SIM_ASSERT(false);
    }
  record_mem_trace(scpu, vma, length,  false, *value);
  return BREW_EXCEPTION_NONE;
}

static brew_exception_type
write_mem(void *context, uint32_t vma, int length, uint32_t value)
{
  sim_cpu *scpu = (sim_cpu*)context;
  SIM_DESC sd = CPU_STATE(scpu);

  switch (length)
    {
    case 8:
      sim_core_write_aligned_1(scpu, CPU_PC_GET(scpu), write_map, vma, value);
      break;
    case 16:
      if ((vma & 1) != 0)
        return BREW_EXCEPTION_UNALIGNED;
      sim_core_write_aligned_2(scpu, CPU_PC_GET(scpu), write_map, vma, value);
      break;
    case 32:
      if ((vma & 3) != 0)
        return BREW_EXCEPTION_UNALIGNED;
      sim_core_write_aligned_4(scpu, CPU_PC_GET(scpu), write_map, vma, value);
      break;
    default:
      SIM_ASSERT(false);
    }
  record_mem_trace(scpu, vma, length, true, value);
  return BREW_EXCEPTION_NONE;
}

static void
handle_stu(void *context, uint32_t pc, bool is_task_mode ATTRIBUTE_UNUSED)
{
  sim_cpu *scpu = (sim_cpu*)context;
  scpu->sim_state.is_task_mode = true;
}

static void
reset_cpu(brew_sim_state *sim_state, bool is_user_mode_sim)
{
  sim_state->dirty_map = 0;
  sim_state->spc = 0;
  sim_state->tpc = 0;
  sim_state->is_task_mode = is_user_mode_sim;
  /* FIXME: do we want to get some other state in the registers? */
  /* Such as:
     - explicit randomization
     - results of POST
     - version/revision info
  */
}

static void
setup_sim_state(sim_cpu *scpu, bool is_user_mode_sim)
{
  scpu->sim_state.read_mem = read_mem;
  scpu->sim_state.write_mem = write_mem;
  scpu->sim_state.handle_stu = handle_stu;
  scpu->sim_state.floor = sim_floor;
  scpu->sim_state.rsqrt = rsqrt;
  if (TRACE_P(scpu, TRACE_INSN_IDX))
    scpu->sim_state.tracer = (fprintf_ftype)sprintf;
  else
    scpu->sim_state.tracer = NULL;
  scpu->sim_state.tracer_strm = scpu->decode_buf;
  reset_cpu(&scpu->sim_state, is_user_mode_sim);
}

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

// Copies a string from SIM memory to host memory.
// Returns a pointer to the string, WHICH NEEDS TO BE FREE-d
static char *sim_core_read_str(sim_cpu *scpu, uint32_t vma)
{
  size_t alloc_size = 1024;
  char *str = (char *)malloc(alloc_size);
  size_t str_len = 0;
  char *c = str;
  SIM_DESC sd = CPU_STATE(scpu);

  SIM_ASSERT(str != NULL);
  do
    {
      *c = sim_core_read_1(scpu, CPU_PC_GET(scpu), read_map, vma);
      if (*c == 0)
        break;
      ++c;
      ++str_len;
      ++vma;
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

#define SET_ERRNO(value) { if (errno_addr != 0) { write_mem(scpu, errno_addr, 32, value); } }

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

#define SIM_REG_T(idx) scpu->sim_state.dirty_map |= (1<<idx); scpu->sim_state.reg[idx]

static void handle_syscall(SIM_DESC sd, sim_cpu *scpu, uint32_t pc)
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
  //
  // NOTE: there's actually a generic syscall interface in the simulator framework.
  //       This is how it's called:
  //  /* XXX: We don't pass back the actual errno value.  */
  //  gr[RET1] = sim_syscall (cpu, gr[TRAPCODE], gr[PARM1], gr[PARM2], gr[PARM3], gr[PARM4]);
  //
  // I'm not sure how it handles mapping of structures and constant between host
  // and sim. It probably assumes they're the same, which is incorrect with newlib.
  // So, for now, I'll leave this code here.

  uint16_t syscall_no;
  uint32_t errno_addr = scpu->sim_state.reg[BREW_REG_LINK];
  uint32_t arg1 = scpu->sim_state.reg[BREW_REG_ARG0];
  uint32_t arg2 = scpu->sim_state.reg[BREW_REG_ARG1];
  uint32_t arg3 = scpu->sim_state.reg[BREW_REG_ARG2];
  uint32_t arg4 = scpu->sim_state.reg[BREW_REG_ARG3];
  char *str1;
  char *str2;
  int ret_val;
  int flags;

  syscall_no = sim_core_read_aligned_2(scpu, pc, exec_map, pc+2);
  scpu->sim_state.ntpc += 4; // skip over syscall insn as well as the syscal number before returning.
  switch (syscall_no) {
    case SYS_exit:
      // simulated process terminating: let's terminate too!
      sim_engine_halt(sd, scpu, NULL, sim_pc_get(scpu), sim_exited, arg1);
      //exit(arg1);
      break;
    case SYS_open:
    {
      uint32_t mode;
      str1 = sim_core_read_str(scpu, arg1);
      // We need to map some flags
      flags = marshal_o_flags_from_sim(arg2);
      // mode is a VAARG argument. Even so, those come through registers as well, just as regular arguments.
      mode = arg3;

      fflush(stdout);

      //printf("SIM OPEN: %s with flags: %x, local flags are: %x and mode 0%o\n", str1, arg2, flags, mode);
      ret_val = open(str1, flags, mode);
      //printf("SIM OPEN returned: %d, errno: %d\n", ret_val, errno);
      free(str1);
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    }
    case SYS_close:
      if (arg1 > 2)
        ret_val = close(arg1);
      else
        ret_val = 0;
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_read: {
      void *buffer = malloc(arg3);
      ret_val = read(arg1, buffer, arg3);
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
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
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      free(buffer);
      break;
    }
    case SYS_lseek:
      ret_val = lseek(arg1, arg2, arg3);
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_unlink:
      str1 = sim_core_read_str(scpu, arg1);
      ret_val = unlink(str1);
      free(str1);
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_getpid:
      // Since we don't simulate multiple processes, we simply return the PID of the simulator.
      SIM_REG_T(BREW_REG_ARG0) = getpid();
      break;
    case SYS_kill:
      ret_val = kill(arg1, arg2);
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
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
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
      SET_ERRNO(EOVERFLOW);
      //SIM_ASSERT(false);
      break;
    }

    // These are not implemented
    case SYS_argvlen:
    case SYS_argv:
      TRACE_EVENTS (scpu, "Unknown SYSCALL: %d", syscall_no);
      sim_engine_halt(sd, scpu, NULL, sim_pc_get(scpu), sim_stopped, SIM_SIGTRAP);
      break;
    case SYS_chdir:
      str1 = sim_core_read_str(scpu, arg1);
      ret_val = chdir(str1);
      free(str1);
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
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
      str1 = sim_core_read_str(scpu, arg1);
      ret_val = chmod(str1, arg2);
      free(str1);
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;
    case SYS_utime: {
      struct utimbuf times;
      str1 = sim_core_read_str(scpu, arg1);
      ret_val = utime(str1, &times);
      free(str1);
      // We actually have to translate the stat structure: we can't just assume it has the same layout
      // on the host as on the simulator.
      SIM_ASSERT(false);
      break;
    }
    case SYS_time: {
      time_t now = time(NULL);
      SIM_REG_T(BREW_REG_ARG0) = now;
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
      str1 = sim_core_read_str(scpu, arg1);
      str2 = sim_core_read_str(scpu, arg2);
      ret_val = link(str1, str2);
      free(str1);
      free(str2);
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
      if (ret_val == -1)
        SET_ERRNO(errno);
      break;

    // These are not implemented
    case SYS_argc:
    case SYS_argnlen:
    case SYS_argn:
    case SYS_reconfig:
      TRACE_EVENTS (scpu, "Unknown SYSCALL: %d", syscall_no);
      sim_engine_halt(sd, scpu, NULL, sim_pc_get(scpu), sim_stopped, SIM_SIGTRAP);
      break;
    // Brew-specific calls:
    case BREW_isatty:
      ret_val = isatty(arg1);
      SIM_REG_T(BREW_REG_ARG0) = ret_val;
      if (ret_val == 0)
        SET_ERRNO(errno);
      break;

    default:
      TRACE_EVENTS (scpu, "Unknown SYSCALL: %d", syscall_no);
      sim_engine_halt(sd, scpu, NULL, sim_pc_get(scpu), sim_stopped, SIM_SIGTRAP);
      break;
  }
}

static INLINE void
str_append(char *dst, int dst_size, const char *src)
{
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wstringop-truncation"
  strncat(dst, src, dst_size - strlen(dst) - 1);
  #pragma GCC diagnostic pop
  dst[dst_size-1] = 0;
}
#define STR_APPEND(dst, src) str_append(dst, sizeof(dst), src)


static INLINE void
side_effect_indent(char *message, int message_size)
{
  static const char *side_effect_delim = "                                                                                                                ";
  int size_so_far = strlen(message);
  int side_effect_indent = 80 - size_so_far;
  if (side_effect_indent < 0) side_effect_indent = 0;
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wstringop-truncation"
  strncat(message, side_effect_delim, MIN(message_size - strlen(message) - 1, side_effect_indent));
  #pragma GCC diagnostic pop
}

#define SIDE_EFFECT_INDENT                        \
  if (first) {                                    \
    side_effect_indent(message, sizeof(message)); \
    first = false;                                \
  }

static INLINE void pre_exec(sim_cpu *scpu, uint16_t insn_code ATTRIBUTE_UNUSED)
{
  int insn_length = brew_insn_len(insn_code);
  scpu->sim_state.dirty_map = 0;
  scpu->sim_state.ntpc = scpu->sim_state.tpc + (scpu->sim_state.is_task_mode ? insn_length : 0);
  scpu->sim_state.nspc = scpu->sim_state.spc + (scpu->sim_state.is_task_mode ? 0 : insn_length);
  scpu->sim_state.nis_task_mode = scpu->sim_state.is_task_mode;
  scpu->sim_state.insn_class = BREW_INSN_CLS_UNKNOWN;
  scpu->sim_state.insn_exception = BREW_EXCEPTION_NONE;
  scpu->decode_buf[0] = 0;
}

static INLINE void post_exec(sim_cpu *scpu, uint16_t insn_code, uint32_t non_branch_tpc, uint32_t non_branch_spc)
{
  SIM_DESC sd = CPU_STATE(scpu);
  PROFILE_DATA *profile_data = CPU_PROFILE_DATA(scpu);

  uint32_t pc;

  pc = scpu->sim_state.is_task_mode ? scpu->sim_state.tpc : scpu->sim_state.spc;

  // Handle tracing
  if (TRACE_P(scpu, TRACE_INSN_IDX))
    {
      char *pc_name;
      char message[256];
      char fragment[256];
      bool first = true;

      pc_name = scpu->sim_state.is_task_mode ? "$tpc": "$spc";

      snprintf(message, sizeof(message), "%s: 0x%x ", pc_name, pc);
      STR_APPEND(message, scpu->decode_buf);
      for (int i=0; i<sizeof(scpu->sim_state.reg)/sizeof(scpu->sim_state.reg[0]); ++i)
        {
          if (scpu->sim_state.dirty_map & (1 << i))
            {
              SIDE_EFFECT_INDENT;
              snprintf(fragment, sizeof(fragment)-1, " %s <- 0x%x", brew_reg_names[i], scpu->sim_state.reg[i]);
              STR_APPEND(message, fragment);
            }
        }
      if (scpu->sim_state.ntpc != non_branch_tpc)
        {
          SIDE_EFFECT_INDENT;
          snprintf(fragment, sizeof(fragment)-1, " $tpc <- 0x%x", scpu->sim_state.ntpc);
          STR_APPEND(message, fragment);
        }
      if (scpu->sim_state.nspc != non_branch_spc)
        {
          SIDE_EFFECT_INDENT;
          snprintf(fragment, sizeof(fragment)-1, " $spc <- 0x%x", scpu->sim_state.nspc);
          STR_APPEND(message, fragment);
        }
      mem_trace_to_str(&scpu->mem_trace, fragment);
      if (fragment[0] != 0)
        {
          SIDE_EFFECT_INDENT;
          STR_APPEND(message, " | ");
          STR_APPEND(message, fragment);
        }
      TRACE_INSN(scpu, "%s", message);
    }
  // Handle profiling
  {
    uint16_t profile_idx;
    int insn_length_cls = (brew_insn_len(insn_code) - 2) / 2; // 0 for 2-byte, 1 for 4-byte, 2 for 6-byte instructions.
    profile_idx = scpu->sim_state.insn_class + BREW_INSN_CLS_MAX * insn_length_cls;
    PROFILE_COUNT_INSN(scpu, NULL, profile_idx);

    if (non_branch_spc != scpu->sim_state.nspc || non_branch_tpc != scpu->sim_state.ntpc)
      ++PROFILE_MODEL_TAKEN_COUNT(profile_data);
    else if (
      scpu->sim_state.insn_class == BREW_INSN_CLS_CBRANCH ||
      scpu->sim_state.insn_class == BREW_INSN_CLS_CBRANCH0 ||
      scpu->sim_state.insn_class == BREW_INSN_CLS_CBRANCHFP ||
      scpu->sim_state.insn_class == BREW_INSN_CLS_CBRANCH0FP ||
      scpu->sim_state.insn_class == BREW_INSN_CLS_CBRANCHBIT
    )
      ++PROFILE_MODEL_UNTAKEN_COUNT(profile_data);
    ++PROFILE_TOTAL_INSN_COUNT(profile_data);
  }
  // Handle exceptions
  if (scpu->sim_state.insn_exception != BREW_EXCEPTION_NONE)
    {
      // TODO: don't forget to set ntpc, nspc, nis_task_mode as appropriate
      if (!scpu->sim_state.is_task_mode)
        {
          sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGTRAP);
        }
      switch (STATE_ENVIRONMENT(sd))
        {
        case USER_ENVIRONMENT:
        case VIRTUAL_ENVIRONMENT:
          // All but the SYSCALL exception are fatal in user-mode sim
          switch(scpu->sim_state.insn_exception)
            {
            case BREW_EXCEPTION_FILL:              sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); break;
            case BREW_EXCEPTION_BREAK:             sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGTRAP); break;
            case BREW_EXCEPTION_SYSCALL:           handle_syscall(sd, scpu, pc); break;
            case BREW_EXCEPTION_SWI3:              sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); break;
            case BREW_EXCEPTION_SWI4:              sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); break;
            case BREW_EXCEPTION_SWI5:              sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); break;
            case BREW_EXCEPTION_SII:               sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); break;
            case BREW_EXCEPTION_HWI:               sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGILL); break;
            case BREW_EXCEPTION_UNALIGNED:         sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGSEGV); break;
            case BREW_EXCEPTION_ACCESS_VIOLATION:  sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGSEGV); break;
            case BREW_EXCEPTION_F_DIV_BY_ZERO:     sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGFPE); break;
            case BREW_EXCEPTION_F_NEG_RSQRT:       sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGFPE); break;
            default:                               sim_engine_halt(sd, scpu, NULL, pc, sim_stopped, SIM_SIGNONE); break;
            }
          break;
        case OPERATING_ENVIRONMENT:
          // In system mode simulations, we simply switch to scheduler mode
          scpu->sim_state.nis_task_mode = false;
        default:
          SIM_ASSERT(false);
        }
    }
  // Update PC
  scpu->sim_state.tpc = scpu->sim_state.ntpc;
  scpu->sim_state.spc = scpu->sim_state.nspc;
  scpu->sim_state.is_task_mode = scpu->sim_state.nis_task_mode;
}

void
sim_engine_run(
  SIM_DESC sd,
  int next_cpu_nr, /* the CPU to run  */
  int nr_cpus,     /* ignore  */
  int signal       /* ignore  */
) {
  uint16_t insn_code;
  uint32_t field_e = 0xdeadbeef;
  int length;
  sim_cpu *scpu = STATE_CPU(sd, next_cpu_nr);

  uint32_t non_branch_tpc;
  uint32_t non_branch_spc;
  do
    {
      {
        // scoping pc to make sure we don't even accidentally use that for anything
        uint32_t pc = CPU_PC_GET(scpu) & ~1; // PC is always 16-bit aligned, the LSB is ignored

        insn_code = sim_core_read_aligned_2(scpu, pc, exec_map, pc);
        length = brew_insn_len(insn_code);
        switch (length)
          {
            case 2:
              field_e = sim_core_read_aligned_2(scpu, pc, exec_map, pc+2);
              field_e = (int32_t)field_e << 16 >> 16;
            break;
            case 4:
            break;
            case 6:
              field_e = sim_core_read_unaligned_4(scpu, pc, exec_map, pc+2);
            break;
            default:
              SIM_ASSERT(false);
            break;
          }
      }
      pre_exec(scpu, insn_code);

      non_branch_spc = scpu->sim_state.nspc;
      non_branch_tpc = scpu->sim_state.ntpc;

      brew_sim_insn(scpu, &scpu->sim_state, insn_code, field_e);
      post_exec(scpu, insn_code, non_branch_tpc, non_branch_spc);
      if (sim_events_tick (sd))
        sim_events_process (sd);
    }
  while (1);
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

/* Store register (rn) at memory location (memory). Length is required to be 4 (32-bits) */
/* Returns the number of bytes stored in memory */
static int
brew_reg_fetch(sim_cpu *scpu, int rn, unsigned char *memory, int length)
{
  if (rn > BREW_GDB_NUM_REGS && rn < 0)
    return 0;
  if (length != 4)
    // Report back proper register size, but do no transfer.
    return 4;

  if (rn == BREW_GDB_REG_TPC)
    write_mem_unaligned(memory, scpu->sim_state.tpc);
  else if (rn == BREW_GDB_REG_SPC)
    write_mem_unaligned(memory, scpu->sim_state.spc);
  else
    write_mem_unaligned(memory, scpu->sim_state.reg[rn]);

  return 4;
}

/* Read a memory location (memory) and store it in register (rn). Length is required to be 4 (32-bits) */
/* Returns the number of bytes read from memory, 0 if no store is performed and negative if an error occured */
/* This apparently is called by GDB when storing all registers in memory for it's ... reasons */
static int
brew_reg_store(sim_cpu *scpu, int rn, unsigned char *memory, int length)
{
  if (rn > BREW_GDB_NUM_REGS && rn < 0)
    return 0;
  if (length != 4)
    return -1;

  if (rn == BREW_GDB_REG_TPC)
    {
      scpu->sim_state.tpc = read_mem_unaligned(memory);
      scpu->sim_state.ntpc = scpu->sim_state.tpc;
    }
  else if (rn == BREW_GDB_REG_SPC)
    {
      scpu->sim_state.spc = read_mem_unaligned(memory);
      scpu->sim_state.nspc = scpu->sim_state.spc;
    }
  else
    scpu->sim_state.reg[rn] = read_mem_unaligned(memory);

  return 4;
}

/* Returns the current PC. Used in tracing and generic syscalls. */
static sim_cia
brew_pc_get (sim_cpu *scpu)
{
  return scpu->sim_state.is_task_mode ? scpu->sim_state.tpc : scpu->sim_state.spc;
}

/* Sets the PC to the required value.
   I don't think anybody calls this.
*/
static void
brew_pc_set (sim_cpu *scpu, sim_cia pc)
{
  SIM_DESC sd = CPU_STATE(scpu);

  SIM_ASSERT(false);

  //if (scpu->sim_state.is_task_mode)
  //  scpu->sim_state.ntpc = pc;
  //else
  //  scpu->sim_state.nspc = pc;
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

/* Given the instruction class code, return a descriptive string */
static const char *
full_insn_class_name(SIM_CPU *scpu, int insn_class_code)
{
  static const char *insn_length_names[] = {
    "",
    " short",
    " long"
  };
  static char insn_name[200];
  brew_insn_classes base_class = insn_class_code % BREW_INSN_CLS_MAX;
  int insn_length_code = (insn_class_code / BREW_INSN_CLS_MAX);

  sprintf(insn_name, "%s%s", brew_insn_class_name(base_class), insn_length_names[insn_class_code]);
  return insn_name;
}

static void
brew_model_init(SIM_CPU *cpu)
{
  CPU_MODEL_DATA(cpu) = NULL; // TODO: Not sure what this is, but hopefully it could be set to NULL. It appears to be something that's target specific.
}

static void
brew_init_cpu(SIM_CPU *scpu)
{
  SIM_DESC sd = CPU_STATE(scpu);

  CPU_REG_FETCH(scpu) = brew_reg_fetch;
  CPU_REG_STORE(scpu) = brew_reg_store;
  CPU_PC_FETCH(scpu) = brew_pc_get;
  CPU_PC_STORE(scpu) = brew_pc_set;
  CPU_MAX_INSNS(scpu) = 0x10000; // To make is simple, every instruction code is counted separately (for now)
  CPU_INSN_NAME(scpu) = full_insn_class_name;
  switch (STATE_ENVIRONMENT(CPU_STATE(scpu)))
    {
    case USER_ENVIRONMENT:
    case VIRTUAL_ENVIRONMENT:
      setup_sim_state(scpu, true);
      break;
    case OPERATING_ENVIRONMENT:
      setup_sim_state(scpu, false);
      break;
    default:
      SIM_ASSERT(false);
    }
}

static void
brew_prepare_run(SIM_CPU *cpu)
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

// Called after loading a program into simulation memory.
// It has to ready the sim for execution.
SIM_RC
sim_create_inferior (SIM_DESC sd, struct bfd *prog_bfd,
                     char * const *argv, char * const *env)
{
  char * const *avp;
  int l, argc, i, tp;
  sim_cpu *scpu = STATE_CPU(sd, 0); // FIXME: Which CPU to use? For us, we only have one, but is that forever the case?

  if (prog_bfd != NULL)
    {
      uint32_t start_addr = bfd_get_start_address(prog_bfd);
      switch (STATE_ENVIRONMENT(CPU_STATE(scpu)))
        {
        case USER_ENVIRONMENT:
        case VIRTUAL_ENVIRONMENT:
          scpu->sim_state.tpc = start_addr;
          scpu->sim_state.ntpc = start_addr;
          scpu->sim_state.is_task_mode = true;
          break;
        case OPERATING_ENVIRONMENT:
          scpu->sim_state.spc = start_addr;
          scpu->sim_state.nspc = start_addr;
          scpu->sim_state.is_task_mode = false;
          break;
        default:
          SIM_ASSERT(false);
        }
    }
  // This is the old Moxie code to transfer argv into simulated memory
#if 0
  avp = argv;
  for (argc = 0; avp && *avp; avp++)
    argc++;

  // Target memory looks like this:
  //    0x00000000 zero word
  //    0x00000004 argc word
  //    0x00000008 start of argv
  //    .
  //    0x0000???? end of argv
  //    0x0000???? zero word
  //    0x0000???? start of data pointed to by argv

  write_uint32(scpu, 0, 0);
  write_uint32(scpu, 4, argc);

  // tp is the offset of our first argv data.
  tp = 4 + 4 + argc * 4 + 4;

  for (i = 0; i < argc; i++)
    {
      // Set the argv value.
      write_uint32(scpu, 4 + 4 + i * 4, tp);

      // Store the string.
      sim_core_write_buffer(sd, scpu, write_map, argv[i], tp, strlen(argv[i])+1);
      tp += strlen (argv[i]) + 1;
    }

  write_uint32(scpu, 4 + 4 + i * 4, 0);

  //load_dtb(sd, DTB);
#endif

  return SIM_RC_OK;
}
