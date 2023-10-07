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

#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "bfd.h"
#include "libiberty.h"
#include "sim/sim.h"

#include "sim-main.h"
#include "sim-base.h"
#include "sim-options.h"
#include "sim-io.h"
#include "sim-signal.h"
#include "target-newlib-syscall.h"

#include "opcode/brew-abi.h"
#include "opcode/brew.h"
#include "gdb-if.h"
#include "models.h"


#ifndef MIN
#define MIN(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#endif // MIN


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



static void
setup_sim_state(sim_cpu *scpu, bool is_user_mode_sim)
{
  if (TRACE_P(scpu, TRACE_INSN_IDX))
    scpu->sim_state.tracer = (fprintf_ftype)sprintf;
  else
    scpu->sim_state.tracer = NULL;
  scpu->sim_state.tracer_strm = scpu->decode_buf;
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
  if (scpu->sim_state.ecause != BREW_EXCEPTION_NONE)
    scpu->sim_state.csr_ecause = scpu->sim_state.ecause;
  scpu->sim_state.ecause = BREW_EXCEPTION_NONE;
  scpu->decode_buf[0] = 0;
}

static INLINE void trace_exception(sim_cpu *scpu, char *message, size_t message_size)
{
  SIM_DESC sd = CPU_STATE(scpu);

  switch(scpu->sim_state.ecause)
    {
      case BREW_EXCEPTION_NONE: break;
      case BREW_EXCEPTION_HWI:               str_append(message, message_size, "exception: hwi"); break;
      case BREW_EXCEPTION_FILL:              str_append(message, message_size, "exception: fill"); break;
      case BREW_EXCEPTION_BREAK:             str_append(message, message_size, "exception: break"); break;
      case BREW_EXCEPTION_SYSCALL:           str_append(message, message_size, "exception: syscall"); break;
      case BREW_EXCEPTION_SWI_3:             str_append(message, message_size, "exception: swi 3"); break;
      case BREW_EXCEPTION_SWI_4:             str_append(message, message_size, "exception: swi 4"); break;
      case BREW_EXCEPTION_SWI_5:             str_append(message, message_size, "exception: swi 5"); break;
      case BREW_EXCEPTION_SWI_6:             str_append(message, message_size, "exception: swi 6"); break;
      case BREW_EXCEPTION_SWI_7:             str_append(message, message_size, "exception: swi 7"); break;
      case BREW_EXCEPTION_UNKNOWN_INST:      str_append(message, message_size, "exception: unknown instruction"); break;
      case BREW_EXCEPTION_TYPE:              str_append(message, message_size, "exception: type error"); break;
      case BREW_EXCEPTION_UNALIGNED:         str_append(message, message_size, "exception: unaligned access"); break;
      case ESPRESSO_EXCEPTION_INST_AV:       str_append(message, message_size, "exception: instruction fetch access violation"); break;
      case ESPRESSO_EXCEPTION_MEM_AV:        str_append(message, message_size, "exception: memory access violation"); break;
      default:                               SIM_ASSERT(false); break;
    }
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
              snprintf(fragment, sizeof(fragment)-1, " %s <- 0x%x (%s)", brew_reg_names[i], scpu->sim_state.reg[i].val, brew_reg_type_name(scpu->sim_state.reg[i].type));
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
      if (scpu->sim_state.ecause != BREW_EXCEPTION_NONE)
        {
          SIDE_EFFECT_INDENT;
          trace_exception(scpu, message, sizeof(message));
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
      scpu->sim_state.insn_class == BREW_INSN_CLS_CBRANCHBIT
    )
      ++PROFILE_MODEL_UNTAKEN_COUNT(profile_data);
    ++PROFILE_TOTAL_INSN_COUNT(profile_data);
  }
  // Handle exceptions
  if (scpu->sim_state.ecause != BREW_EXCEPTION_NONE)
    {
      scpu->sim_state.ntpc = scpu->sim_state.tpc; // Undo any pending changes to $tpc
      scpu->sim_state.model_functions.handle_exception(scpu);
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
            break;
            case 4:
              field_e = sim_core_read_aligned_2(scpu, pc, exec_map, pc+2);
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

/* Store register (rn) at memory location (memory). Length is required to be 8 (32-bit value, 32-bit type) */
/* Returns the number of bytes stored in memory */
static int
brew_reg_fetch(sim_cpu *scpu, int rn, unsigned char *memory, int length)
{
  if (rn > BREW_GDB_NUM_REGS && rn < 0)
    return 0;
  if (length != 8)
    // Report back proper register size, but do no transfer.
    return 8;

  if (rn == BREW_GDB_REG_TPC)
    {
      write_mem_unaligned(memory, scpu->sim_state.tpc);
      write_mem_unaligned(memory+4, BREW_REG_TYPE_INT32);
    }
  else if (rn == BREW_GDB_REG_SPC)
    {
      write_mem_unaligned(memory, scpu->sim_state.spc);
      write_mem_unaligned(memory+4, BREW_REG_TYPE_INT32);
    }
  else
    {
      write_mem_unaligned(memory, scpu->sim_state.reg[rn].val);
      write_mem_unaligned(memory+4, scpu->sim_state.reg[rn].type);
    }

  return 8;
}

/* Read a memory location (memory) and store it in register (rn). Length is required to be 8 (32-bit value, 32-bit type) */
/* Returns the number of bytes read from memory, 0 if no store is performed and negative if an error occured */
/* This apparently is called by GDB when storing all registers in memory for it's ... reasons */
static int
brew_reg_store(sim_cpu *scpu, int rn, unsigned char *memory, int length)
{
  if (rn > BREW_GDB_NUM_REGS && rn < 0)
    return 0;
  if (length != 8)
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
    {
      scpu->sim_state.reg[rn].val = read_mem_unaligned(memory);
      scpu->sim_state.reg[rn].type = read_mem_unaligned(memory+4);
    }

  return 8;
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

static void brew_sim_uninstall(SIM_DESC sd)
{
  int c;
  for (c = 0; c < MAX_NR_PROCESSORS; ++c) {
    SIM_CPU *scpu = STATE_CPU(sd, c);
    if (scpu->model_info != NULL)
      brew_free_model_info(scpu, sd);
    scpu->model_info = NULL;
  }
}

/* De-allocates any resources, allocated in sim_open, if there's an error during its execution */
static void
free_state (SIM_DESC sd)
{
  int c;

  if (STATE_MODULES(sd) != NULL)
    sim_module_uninstall(sd);

  brew_sim_uninstall(sd);

  sim_cpu_free_all(sd);
  sim_state_free(sd);
}

/* Given the instruction class code, return a descriptive string */
static const char *
full_insn_class_name(sim_cpu *scpu, int insn_class_code)
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

static const SIM_MODEL brew_models[];

static void
brew_init_cpu(sim_cpu *scpu)
{
  CPU_REG_FETCH(scpu) = brew_reg_fetch;
  CPU_REG_STORE(scpu) = brew_reg_store;
  CPU_PC_FETCH(scpu) = brew_pc_get;
  CPU_PC_STORE(scpu) = brew_pc_set;
  CPU_MAX_INSNS(scpu) = 0x10000; // To make is simple, every instruction code is counted separately (for now)
  CPU_INSN_NAME(scpu) = full_insn_class_name;
  setup_sim_state(scpu, true);
}

static void
brew_prepare_run(sim_cpu *cpu)
{
}

static const SIM_MACH_IMP_PROPERTIES brew_imp_properties =
{
  sizeof(sim_cpu),
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
  // MODEL_NAME   MODEL_MACH        MODEL_NUM   MODEL_TIMING    MODEL_INIT
  { "brew",       &brew_mach,       0,          NULL,           brew_model_init},
  { "espresso",   &brew_mach,       1,          NULL,           espresso_model_init},
  { "anachron",   &brew_mach,       2,          NULL,           anachron_model_init},
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
  bool is_user_mode;
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
  /*    ^----  this is not true anymore. That's done in brew_init_cpu. */
  /*  what we do here is set things up based on 'sd', such as initial operating mode */

  switch (STATE_ENVIRONMENT(sd))
    {
    case USER_ENVIRONMENT:
    case VIRTUAL_ENVIRONMENT:
      is_user_mode = true;
      break;
    case OPERATING_ENVIRONMENT:
      is_user_mode = false;
      break;
    default:
      SIM_ASSERT(false);
    }
  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
    {
      sim_cpu *scpu = STATE_CPU (sd, i);

      scpu->sim_state.model_functions.reset_cpu(scpu, is_user_mode);
      brew_model_setup_sim(scpu, sd);
    }

  sim_module_add_uninstall_fn(sd, brew_sim_uninstall);
  //sim_module_add_init_fn (sd, sim_core_init);

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
