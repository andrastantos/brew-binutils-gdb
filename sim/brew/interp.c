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
#include "safe-ctype.h"
#include "target-newlib-syscall.h"

#include "opcode/brew-abi.h"
#include "opcode/brew.h"
#include "gdb-if.h"
#include "models.h"
#include "espresso-timing.h"

#include <inttypes.h>


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
setup_sim_state(sim_cpu *scpu)
{
  if (TRACE_P(scpu, TRACE_INSN_IDX))
    BREW_SIM_CPU (scpu)->sim_state.tracer = (fprintf_ftype)sprintf;
  else
    BREW_SIM_CPU (scpu)->sim_state.tracer = NULL;
  BREW_SIM_CPU (scpu)->sim_state.tracer_strm = BREW_SIM_CPU (scpu)->decode_buf;
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

static INLINE void trace_exception(sim_cpu *scpu, char *message, size_t message_size)
{
  SIM_DESC sd = CPU_STATE(scpu);

  switch(BREW_SIM_CPU (scpu)->sim_state.ecause)
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

#if WITH_PROFILE_INSN_P

/* Instruction profile (--profile-insn; 'run' prints it with -v).

   Instructions are counted per first instruction word: on the current ISA
   that word alone selects the instruction form.  (The extension groups,
   which decode FIELD_E, are disabled in brew-decode.c; enabling them needs
   FIELD_E in the key.)  The report disassembles one sample of each word
   that ran, turns its operands into placeholders and merges the words that
   give the same form.  The class summary groups them by the instruction
   class the decoder reports, split by instruction length.  */

#define BREW_INSN_WORDS 0x10000
#define BREW_INSN_LENGTHS 3 /* 16, 32 and 48 bits */

struct brew_insn_profile
{
  uint64_t count[BREW_INSN_WORDS];
  uint32_t sample_field_e[BREW_INSN_WORDS];
  uint8_t insn_class[BREW_INSN_WORDS];
};

static INLINE void
profile_count_insn_form(sim_cpu *scpu, uint16_t insn_code, uint32_t field_e)
{
  struct brew_insn_profile *prof = BREW_SIM_CPU (scpu)->insn_profile;

  if (prof == NULL)
    prof = BREW_SIM_CPU (scpu)->insn_profile = xcalloc(1, sizeof(*prof));
  if (prof->count[insn_code]++ == 0)
    {
      prof->sample_field_e[insn_code] = field_e;
      prof->insn_class[insn_code] = BREW_SIM_CPU (scpu)->sim_state.insn_class;
    }
}

static bool
is_gpr_name(const char *name, size_t len)
{
  for (int i = 0; i < 15; ++i)
    if (strncmp(brew_reg_names[i], name, len) == 0 && brew_reg_names[i][len] == 0)
      return true;
  return false;
}

/* Turn a disassembled instruction into its form: general registers become
   $r, numbers N (the disassembler's " (0x...)" repeats are dropped), and
   the sign the disassembler picks for an offset or immediate ("+ N",
   "- N", "- tiny N") becomes "+/-".  A subtraction keeps its "-": its
   right operand is a register, or the immediate is on the left.  */
static void
insn_form(const char *in, char *out, size_t out_size)
{
  char tmp[256];
  const char *start = in;
  size_t o = 0;
  char prev = ' ';

  while (*in != 0 && o + 4 < sizeof(tmp))
    {
      if (*in == '$')
        {
          const char *end = in + 1;
          while (ISALNUM(*end))
            ++end;
          if (is_gpr_name(in, end - in))
            {
              tmp[o++] = '$';
              tmp[o++] = 'r';
            }
          else
            while (in < end && o + 4 < sizeof(tmp))
              tmp[o++] = *in++;
          in = end;
          prev = 'r';
          continue;
        }
      if (in[0] == ' ' && in[1] == '(' && in[2] == '0' && in[3] == 'x')
        {
          const char *end = strchr(in, ')');
          if (end != NULL)
            {
              in = end + 1;
              continue;
            }
        }
      if (!ISALNUM(prev) && prev != '_'
          && (ISDIGIT(in[0]) || (in[0] == '-' && ISDIGIT(in[1]))))
        {
          // "if $rA == 0 ...": the 0 is part of the instruction (branches
          // compare with zero or a register, never an immediate)
          if (in[0] == '0' && !ISALNUM(in[1]) && strncmp(start, "if ", 3) == 0
              && o >= 2 && tmp[o - 1] == ' '
              && strchr("=<>", tmp[o - 2]) != NULL)
            {
              prev = *in;
              tmp[o++] = *in++;
              continue;
            }
          if (*in == '-')
            ++in;
          if (in[0] == '0' && (in[1] == 'x' || in[1] == 'X'))
            {
              in += 2;
              while (ISXDIGIT(*in))
                ++in;
            }
          else
            while (ISDIGIT(*in))
              ++in;
          tmp[o++] = 'N';
          prev = 'N';
          continue;
        }
      prev = *in;
      tmp[o++] = *in++;
    }
  tmp[o] = 0;

  /* Fold the signs.  */
  o = 0;
  for (const char *t = tmp; *t != 0 && o + 4 < out_size; )
    {
      if ((t[0] == '+' || t[0] == '-') && t[1] == ' '
          && (t == tmp || t[-1] == ' ')
          && ((t[2] == 'N' && !ISALNUM(t[3]))
              || (strncmp(t + 2, "tiny N", 6) == 0 && !ISALNUM(t[8]))))
        {
          memcpy(out + o, "+/-", 3);
          o += 3;
          ++t;
          continue;
        }
      out[o++] = *t++;
    }
  out[o] = 0;
}

struct insn_form_count
{
  char *name;
  uint64_t count;
};

static int
cmp_form_name(const void *a, const void *b)
{
  return strcmp(((const struct insn_form_count *)a)->name,
                ((const struct insn_form_count *)b)->name);
}

static int
cmp_form_count(const void *a, const void *b)
{
  const struct insn_form_count *fa = a, *fb = b;

  if (fa->count != fb->count)
    return fa->count < fb->count ? 1 : -1;
  return strcmp(fa->name, fb->name);
}

struct class_count
{
  int insn_class;
  uint64_t count[BREW_INSN_LENGTHS];
  uint64_t total;
};

static int
cmp_class_count(const void *a, const void *b)
{
  const struct class_count *ca = a, *cb = b;

  if (ca->total != cb->total)
    return ca->total < cb->total ? 1 : -1;
  return ca->insn_class - cb->insn_class;
}

/* Like sim-profile.c's profile_printf: to --profile-file if given.  */
static void ATTRIBUTE_PRINTF (2, 3)
brew_profile_printf(sim_cpu *scpu, const char *fmt, ...)
{
  FILE *fp = PROFILE_FILE (CPU_PROFILE_DATA (scpu));
  va_list ap;

  va_start(ap, fmt);
  if (fp != NULL)
    vfprintf(fp, fmt, ap);
  else
    sim_io_evprintf(CPU_STATE (scpu), fmt, ap);
  va_end(ap);
}

#define COUNT_STR(buf, n) sim_add_commas((buf), sizeof(buf), (n))

/* The cycle model's report (--timing=espresso with --profile-model).  */
static void
brew_timing_profile_info(sim_cpu *scpu)
{
  espresso_timing_stats st;
  char b0[32];
  double total;

  if (!BREW_SIM_CPU (STATE_CPU (CPU_STATE (scpu), 0))->timing_espresso || !PROFILE_MODEL_P (scpu))
    return;
  brew_profile_printf(scpu, "Espresso Timing (--timing=espresso)\n\n");
  if (BREW_SIM_CPU (scpu)->espresso_timing == NULL)
    {
      brew_profile_printf(scpu, "  Total: 0 cycles\n\n");
      return;
    }
  st = *espresso_timing_get_stats(BREW_SIM_CPU (scpu)->espresso_timing);
  total = st.cycles ? (double)st.cycles : 1.0;
#define TIMING_LINE(label, n) \
  brew_profile_printf(scpu, "  %-34s %14s %7.2f%%\n", label, COUNT_STR(b0, n), 100.0 * (n) / total)
  brew_profile_printf(scpu, "  %-34s %14s\n", "Total cycles:", COUNT_STR(b0, st.cycles));
  brew_profile_printf(scpu, "  %-34s %14s\n", "Instructions entering execute:", COUNT_STR(b0, st.insns));
  brew_profile_printf(scpu, "  %-34s %14.3f\n", "Cycles per instruction:", st.insns ? st.cycles / (double)st.insns : 0.0);
  brew_profile_printf(scpu, "\n  Where the cycles went (one reason per cycle):\n");
  TIMING_LINE("An instruction entered execute:", st.insns);
  TIMING_LINE("Refill after a taken branch:", st.stall_branch);
  TIMING_LINE("Fetch starved decode:", st.stall_fetch);
  TIMING_LINE("Waiting for a register:", st.stall_dep);
  TIMING_LINE("Waiting for memory or the bus:", st.stall_mem);
  TIMING_LINE("Multiply lockout:", st.stall_mult);
#undef TIMING_LINE
  brew_profile_printf(scpu, "\n  Bus:\n");
  brew_profile_printf(scpu, "  %-34s %14s\n", "Instruction words fetched:", COUNT_STR(b0, st.fetch_beats));
  brew_profile_printf(scpu, "  %-34s %14s\n", "  thrown away by branches:", COUNT_STR(b0, st.fetch_dropped));
  brew_profile_printf(scpu, "  %-34s %14s\n", "Data transfers (16-bit):", COUNT_STR(b0, st.mem_beats));
  brew_profile_printf(scpu, "  %-34s %14s\n", "DRAM refresh cycles:", COUNT_STR(b0, st.refresh));
  if (st.resyncs != 0)
    brew_profile_printf(scpu, "  %-34s %14s\n", "Model resynchronisations:", COUNT_STR(b0, st.resyncs));
  brew_profile_printf(scpu, "\n");
}

static void
brew_insn_profile_info(sim_cpu *scpu, bool verbose)
{
  struct brew_insn_profile *prof = BREW_SIM_CPU (scpu)->insn_profile;
  struct class_count classes[BREW_INSN_CLS_MAX];
  struct insn_form_count *forms;
  size_t nr_forms = 0;
  uint64_t total = 0;
  int width = 20;
  char b0[32], b1[32], b2[32], b3[32];

  if (!PROFILE_INSN_P (scpu))
    return;

  brew_profile_printf(scpu, "Instruction Statistics\n\n");
  if (prof == NULL)
    {
      brew_profile_printf(scpu, "  Total: 0 insns\n\n");
      return;
    }

  memset(classes, 0, sizeof(classes));
  for (int c = 0; c < BREW_INSN_CLS_MAX; ++c)
    classes[c].insn_class = c;
  forms = xmalloc(sizeof(*forms) * BREW_INSN_WORDS);
  for (unsigned code = 0; code < BREW_INSN_WORDS; ++code)
    {
      char text[256], form[256];
      int cls;

      if (prof->count[code] == 0)
        continue;
      total += prof->count[code];
      cls = prof->insn_class[code] < BREW_INSN_CLS_MAX ? prof->insn_class[code] : BREW_INSN_CLS_UNKNOWN;
      classes[cls].count[(brew_insn_len(code) - 2) / 2] += prof->count[code];
      classes[cls].total += prof->count[code];

      text[0] = 0;
      brew_print_insn((fprintf_ftype)sprintf, text, code, prof->sample_field_e[code]);
      insn_form(text, form, sizeof(form));
      forms[nr_forms].name = xstrdup(form);
      forms[nr_forms].count = prof->count[code];
      ++nr_forms;
    }

  /* Merge the words that give the same form.  */
  qsort(forms, nr_forms, sizeof(*forms), cmp_form_name);
  {
    size_t out = 0;
    for (size_t i = 0; i < nr_forms; ++i)
      {
        if (out > 0 && strcmp(forms[out - 1].name, forms[i].name) == 0)
          {
            forms[out - 1].count += forms[i].count;
            free(forms[i].name);
          }
        else
          forms[out++] = forms[i];
      }
    nr_forms = out;
  }
  qsort(forms, nr_forms, sizeof(*forms), cmp_form_count);
  qsort(classes, BREW_INSN_CLS_MAX, sizeof(classes[0]), cmp_class_count);
  for (size_t i = 0; i < nr_forms; ++i)
    if ((int)strlen(forms[i].name) > width)
      width = strlen(forms[i].name);

  brew_profile_printf(scpu, "  Total: %s insns\n\n", COUNT_STR(b0, total));
  brew_profile_printf(scpu, "  %-*s %12s %7s %12s %12s %12s\n", width, "Class",
                      "Count", "%", "16-bit", "32-bit", "48-bit");
  for (int c = 0; c < BREW_INSN_CLS_MAX && classes[c].total != 0; ++c)
    brew_profile_printf(scpu, "  %-*s %12s %7.2f %12s %12s %12s\n", width,
                        brew_insn_class_name(classes[c].insn_class),
                        COUNT_STR(b0, classes[c].total),
                        100.0 * classes[c].total / total,
                        COUNT_STR(b1, classes[c].count[0]),
                        COUNT_STR(b2, classes[c].count[1]),
                        COUNT_STR(b3, classes[c].count[2]));
  brew_profile_printf(scpu, "\n  %-*s %12s %7s\n", width, "Form", "Count", "%");
  for (size_t i = 0; i < nr_forms; ++i)
    {
      brew_profile_printf(scpu, "  %-*s %12s %7.2f\n", width, forms[i].name,
                          COUNT_STR(b0, forms[i].count),
                          100.0 * forms[i].count / total);
      free(forms[i].name);
    }
  brew_profile_printf(scpu, "\n");
  free(forms);
}

static void
brew_profile_info(sim_cpu *scpu, bool verbose)
{
  brew_insn_profile_info(scpu, verbose);
  brew_timing_profile_info(scpu);
}

#endif /* WITH_PROFILE_INSN_P */

/* Feeds an executed instruction to the cycle model (--timing=espresso) and
   returns the cycle it entered execute.  */
static uint64_t
timing_step(sim_cpu *scpu, uint32_t pc, uint16_t insn_code)
{
  struct brew_sim_cpu *cpu = BREW_SIM_CPU (scpu);
  brew_sim_state *sim_state = &cpu->sim_state;
  espresso_timing_insn insn;
  uint64_t cycle;

  if (cpu->espresso_timing == NULL)
    cpu->espresso_timing = espresso_timing_new(brew_timing_read_insn_word, scpu,
                                               brew_model_has_espresso_map(scpu));
  insn.pc = pc;
  insn.len = brew_insn_len(insn_code);
  // As the core's do_branch: a written PC, even if it is the next instruction's
  insn.taken = sim_state->pc_written || sim_state->nis_task_mode != sim_state->is_task_mode
               || sim_state->ecause != BREW_EXCEPTION_NONE;
  insn.next_pc = sim_state->nis_task_mode ? sim_state->ntpc : sim_state->nspc;
  // The memory trace holds the instruction's data access, or its fetch.
  insn.has_mem = cpu->mem_trace.is_valid
                 && (cpu->mem_trace.addr < pc || cpu->mem_trace.addr >= pc + insn.len);
  insn.mem_addr = cpu->mem_trace.addr;
  cycle = espresso_timing_step(cpu->espresso_timing, &insn);
  {
    const espresso_timing_stats *st = espresso_timing_get_stats(cpu->espresso_timing);
    PROFILE_DATA *data = CPU_PROFILE_DATA (scpu);

    PROFILE_MODEL_TOTAL_CYCLES (data) = st->cycles;
    PROFILE_MODEL_CTI_STALL_CYCLES (data) = st->stall_branch;
    PROFILE_MODEL_LOAD_STALL_CYCLES (data) = st->stall_mem;
  }
  return cycle;
}

static INLINE void post_exec(sim_cpu *scpu, uint16_t insn_code, uint32_t field_e, bool fetched, uint32_t non_branch_tpc, uint32_t non_branch_spc)
{
  SIM_DESC sd = CPU_STATE(scpu);
  PROFILE_DATA *profile_data = CPU_PROFILE_DATA(scpu);

  uint32_t pc;
  bool timed = false;
  uint64_t timed_cycle = 0;

  pc = BREW_SIM_CPU (scpu)->sim_state.is_task_mode ? BREW_SIM_CPU (scpu)->sim_state.tpc : BREW_SIM_CPU (scpu)->sim_state.spc;

  // Cycle model (the option is stored on the first CPU)
  if (fetched && BREW_SIM_CPU (STATE_CPU (sd, 0))->timing_espresso)
    {
      timed_cycle = timing_step(scpu, pc, insn_code);
      timed = true;
    }

  // Handle tracing
  if (TRACE_P(scpu, TRACE_INSN_IDX))
    {
      char *pc_name;
      char message[256];
      char fragment[256];
      bool first = true;

      pc_name = BREW_SIM_CPU (scpu)->sim_state.is_task_mode ? "$tpc": "$spc";

      if (timed)
        snprintf(message, sizeof(message), "[%" PRIu64 "] %s: 0x%x ", timed_cycle, pc_name, pc);
      else
        snprintf(message, sizeof(message), "%s: 0x%x ", pc_name, pc);
      STR_APPEND(message, BREW_SIM_CPU (scpu)->decode_buf);
      for (int i=0; i<sizeof(BREW_SIM_CPU (scpu)->sim_state.reg)/sizeof(BREW_SIM_CPU (scpu)->sim_state.reg[0]); ++i)
        {
          if (BREW_SIM_CPU (scpu)->sim_state.dirty_map & (1 << i))
            {
              SIDE_EFFECT_INDENT;
              snprintf(fragment, sizeof(fragment)-1, " %s <- 0x%x (%s)", brew_reg_names[i], BREW_SIM_CPU (scpu)->sim_state.reg[i].val, brew_reg_type_name(BREW_SIM_CPU (scpu)->sim_state.reg[i].type));
              STR_APPEND(message, fragment);
            }
        }
      if (BREW_SIM_CPU (scpu)->sim_state.ntpc != non_branch_tpc)
        {
          SIDE_EFFECT_INDENT;
          snprintf(fragment, sizeof(fragment)-1, " $tpc <- 0x%x", BREW_SIM_CPU (scpu)->sim_state.ntpc);
          STR_APPEND(message, fragment);
        }
      if (BREW_SIM_CPU (scpu)->sim_state.nspc != non_branch_spc)
        {
          SIDE_EFFECT_INDENT;
          snprintf(fragment, sizeof(fragment)-1, " $spc <- 0x%x", BREW_SIM_CPU (scpu)->sim_state.nspc);
          STR_APPEND(message, fragment);
        }
      mem_trace_to_str(&BREW_SIM_CPU (scpu)->mem_trace, fragment);
      if (fragment[0] != 0)
        {
          SIDE_EFFECT_INDENT;
          STR_APPEND(message, " | ");
          STR_APPEND(message, fragment);
        }
      if (BREW_SIM_CPU (scpu)->sim_state.ecause != BREW_EXCEPTION_NONE)
        {
          SIDE_EFFECT_INDENT;
          trace_exception(scpu, message, sizeof(message));
        }
      TRACE_INSN(scpu, "%s", message);
    }
  // Handle profiling
  {
#if WITH_PROFILE_INSN_P
    // An instruction that failed to fetch has no form (insn_code is stale)
    if (fetched && PROFILE_INSN_P (scpu))
      profile_count_insn_form(scpu, insn_code, field_e);
#endif

    if (non_branch_spc != BREW_SIM_CPU (scpu)->sim_state.nspc || non_branch_tpc != BREW_SIM_CPU (scpu)->sim_state.ntpc)
      ++PROFILE_MODEL_TAKEN_COUNT(profile_data);
    else if (
      BREW_SIM_CPU (scpu)->sim_state.insn_class == BREW_INSN_CLS_CBRANCH ||
      BREW_SIM_CPU (scpu)->sim_state.insn_class == BREW_INSN_CLS_CBRANCH0 ||
      BREW_SIM_CPU (scpu)->sim_state.insn_class == BREW_INSN_CLS_CBRANCHBIT
    )
      ++PROFILE_MODEL_UNTAKEN_COUNT(profile_data);
    ++PROFILE_TOTAL_INSN_COUNT(profile_data);
  }
  // Handle exceptions
  if (BREW_SIM_CPU (scpu)->sim_state.ecause != BREW_EXCEPTION_NONE)
    {
      brew_sim_state *sim_state = &BREW_SIM_CPU (scpu)->sim_state;

      // Record the exception in the CSRs (docs_src/exceptions.rst): csr_eaddr is the
      // target address for memory-related exceptions (whoever raised those set it
      // already), the excepting instruction's address otherwise.
      sim_state->csr_ecause = sim_state->ecause;
      if (sim_state->ecause != BREW_EXCEPTION_UNALIGNED &&
          sim_state->ecause != ESPRESSO_EXCEPTION_MEM_AV &&
          sim_state->ecause != ESPRESSO_EXCEPTION_INST_AV)
        sim_state->csr_eaddr = sim_state->is_task_mode ? sim_state->tpc : sim_state->spc;
      sim_state->ntpc = sim_state->tpc; // Undo any pending changes to $tpc
      sim_state->model_functions.handle_exception(scpu);
      // The cycle model times an exception as a taken branch to where it went
      if (timed)
        espresso_timing_set_next_pc(BREW_SIM_CPU (scpu)->espresso_timing, pc,
                                    sim_state->nis_task_mode ? sim_state->ntpc : sim_state->nspc);
    }
  // Update PC
  BREW_SIM_CPU (scpu)->sim_state.tpc = BREW_SIM_CPU (scpu)->sim_state.ntpc;
  BREW_SIM_CPU (scpu)->sim_state.spc = BREW_SIM_CPU (scpu)->sim_state.nspc;
  // A change of execution mode (exception, interrupt, stm) breaks a load-lock.
  if (BREW_SIM_CPU (scpu)->sim_state.is_task_mode != BREW_SIM_CPU (scpu)->sim_state.nis_task_mode)
    BREW_SIM_CPU (scpu)->sim_state.ll_valid = false;
  BREW_SIM_CPU (scpu)->sim_state.is_task_mode = BREW_SIM_CPU (scpu)->sim_state.nis_task_mode;
}

void
sim_engine_run(
  SIM_DESC sd,
  int next_cpu_nr, /* the CPU to run  */
  int nr_cpus,     /* ignore  */
  int signal       /* signal to deliver on resume; see below  */
) {
  uint16_t insn_code;
  uint32_t field_e = 0xdeadbeef;
  int insn_length;
  sim_cpu *scpu = STATE_CPU(sd, next_cpu_nr);

  uint32_t non_branch_tpc;
  uint32_t non_branch_spc;

  // In the operating environment, 'run' resumes after every stop and hands
  // the stop signal back (gdb does the same for signals it passes, such as
  // SIGSEGV). The simulated machine has nowhere to deliver a host signal (it
  // comes from a fault the model couldn't turn into an exception, e.g. one in
  // SCHEDULER mode), and resuming would just repeat the fault forever. So treat
  // delivery as fatal, like an unhandled signal in a native process: report
  // the signal the simulation stopped with. (The number passed in is a host
  // signal from 'run' but a gdb_signal from gdb, so don't use it.)
  if (signal != 0 && STATE_ENVIRONMENT(sd) == OPERATING_ENVIRONMENT)
    {
      sim_engine *engine = STATE_ENGINE(sd);
      int sig = (engine->reason == sim_stopped) ? engine->sigrc : SIM_SIGABRT;
      sim_engine_halt(sd, scpu, NULL, CPU_PC_GET(scpu), sim_signalled, sig);
    }

  do
    {
      bool fetched = false;

      BREW_SIM_CPU (scpu)->sim_state.ecause = BREW_EXCEPTION_NONE;
      {
        // scoping pc to make sure we don't even accidentally use that for anything
        uint32_t pc = CPU_PC_GET(scpu) & ~1; // PC is always 16-bit aligned, the LSB is ignored

        insn_code = BREW_SIM_CPU (scpu)->sim_state.model_functions.read_inst(scpu, pc, 16);
        insn_length = brew_insn_len(insn_code);
        switch (insn_length)
          {
            case 2:
            break;
            case 4:
              field_e = BREW_SIM_CPU (scpu)->sim_state.model_functions.read_inst(scpu, pc+2, 16);
            break;
            case 6:
              field_e = BREW_SIM_CPU (scpu)->sim_state.model_functions.read_inst(scpu, pc+2, 32);
            break;
            default:
              SIM_ASSERT(false);
            break;
          }
      }
      // Make sure we don't actually change the state if there was an exception fetching the instruction
      if (BREW_SIM_CPU (scpu)->sim_state.ecause == BREW_EXCEPTION_NONE)
        {
          BREW_SIM_CPU (scpu)->sim_state.dirty_map = 0;
          BREW_SIM_CPU (scpu)->sim_state.pc_written = false;
          BREW_SIM_CPU (scpu)->sim_state.ntpc = BREW_SIM_CPU (scpu)->sim_state.tpc + (BREW_SIM_CPU (scpu)->sim_state.is_task_mode ? insn_length : 0);
          BREW_SIM_CPU (scpu)->sim_state.nspc = BREW_SIM_CPU (scpu)->sim_state.spc + (BREW_SIM_CPU (scpu)->sim_state.is_task_mode ? 0 : insn_length);
          BREW_SIM_CPU (scpu)->sim_state.nis_task_mode = BREW_SIM_CPU (scpu)->sim_state.is_task_mode;
          BREW_SIM_CPU (scpu)->sim_state.insn_class = BREW_INSN_CLS_UNKNOWN;
          BREW_SIM_CPU (scpu)->decode_buf[0] = 0;

          non_branch_spc = BREW_SIM_CPU (scpu)->sim_state.nspc;
          non_branch_tpc = BREW_SIM_CPU (scpu)->sim_state.ntpc;

          brew_sim_insn(scpu, &BREW_SIM_CPU (scpu)->sim_state, insn_code, field_e);
          fetched = true;
        } else {
          // If we had a fetch exception, what should the non-branch PCs be? We branch no matter what???
          // I actually don't think it matters all that much, we use it only for reporting below, but still...
          non_branch_spc = BREW_SIM_CPU (scpu)->sim_state.spc;
          non_branch_tpc = BREW_SIM_CPU (scpu)->sim_state.tpc;
        }

      post_exec(scpu, insn_code, field_e, fetched, non_branch_tpc, non_branch_spc);
      if (sim_events_tick (sd))
        sim_events_process (sd);
    }
  while (1);
}


static INLINE int read_mem_unaligned(const uint8_t *memory)
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
// TODO: actually, for now, let's disregard type and return only a 32-bit value
static int
brew_reg_fetch(sim_cpu *scpu, int rn, void *buf, int length)
{
  uint8_t *memory = (uint8_t *) buf;

  if (rn < 0 || rn >= BREW_GDB_NUM_REGS)
    return 0;
  if (length != 4)
    // Report back proper register size, but do no transfer.
    return 4;

  // Override PC to the appropriate one, before fetching the value.
  if (rn == BREW_GDB_REG_PC)
    {
      if (BREW_SIM_CPU (scpu)->sim_state.is_task_mode)
        rn = BREW_GDB_REG_TPC;
      else
        rn = BREW_GDB_REG_SPC;
    }

  if (rn == BREW_GDB_REG_TPC)
    {
      write_mem_unaligned(memory, BREW_SIM_CPU (scpu)->sim_state.tpc);
      //write_mem_unaligned(memory+4, BREW_REG_TYPE_INT32);
    }
  else if (rn == BREW_GDB_REG_SPC)
    {
      write_mem_unaligned(memory, BREW_SIM_CPU (scpu)->sim_state.spc);
      //write_mem_unaligned(memory+4, BREW_REG_TYPE_INT32);
    }
  else if (rn == BREW_GDB_REG_TASK_MODE)
    {
      write_mem_unaligned(memory, BREW_SIM_CPU (scpu)->sim_state.is_task_mode ? 1 : 0);
    }
  else
    {
      write_mem_unaligned(memory, BREW_SIM_CPU (scpu)->sim_state.reg[rn].val);
      //write_mem_unaligned(memory+4, BREW_SIM_CPU (scpu)->sim_state.reg[rn].type);
    }

  return 4;
}

/* Read a memory location (memory) and store it in register (rn). Length is required to be 8 (32-bit value, 32-bit type) */
/* Returns the number of bytes read from memory, 0 if no store is performed and negative if an error occured */
/* This apparently is called by GDB when storing all registers in memory for it's ... reasons */
// TODO: actually, for now, let's disregard type and return only a 32-bit value
static int
brew_reg_store(sim_cpu *scpu, int rn, const void *buf, int length)
{
  const uint8_t *memory = (const uint8_t *) buf;

  if (rn < 0 || rn >= BREW_GDB_NUM_REGS)
    return 0;
  if (length != 4)
    return -1;

  // Override PC to the appropriate one, before doing the update.
  if (rn == BREW_GDB_REG_PC)
    {
      if (BREW_SIM_CPU (scpu)->sim_state.is_task_mode)
        rn = BREW_GDB_REG_TPC;
      else
        rn = BREW_GDB_REG_SPC;
    }

  if (rn == BREW_GDB_REG_TPC)
    {
      BREW_SIM_CPU (scpu)->sim_state.tpc = read_mem_unaligned(memory);
      BREW_SIM_CPU (scpu)->sim_state.ntpc = BREW_SIM_CPU (scpu)->sim_state.tpc;
    }
  else if (rn == BREW_GDB_REG_SPC)
    {
      BREW_SIM_CPU (scpu)->sim_state.spc = read_mem_unaligned(memory);
      BREW_SIM_CPU (scpu)->sim_state.nspc = BREW_SIM_CPU (scpu)->sim_state.spc;
    }
  else if (rn == BREW_GDB_REG_TASK_MODE)
    {
      // We don't support changing execution mode from debugger
      return -1;
    }
  else
    {
      BREW_SIM_CPU (scpu)->sim_state.reg[rn].val = read_mem_unaligned(memory);
      //BREW_SIM_CPU (scpu)->sim_state.reg[rn].type = read_mem_unaligned(memory+4);
    }

  return 4;
}

/* Returns the current PC. Used in tracing and generic syscalls. */
static sim_cia
brew_pc_get (sim_cpu *scpu)
{
  return BREW_SIM_CPU (scpu)->sim_state.is_task_mode ? BREW_SIM_CPU (scpu)->sim_state.tpc : BREW_SIM_CPU (scpu)->sim_state.spc;
}

/* Sets the PC to the required value.
   I don't think anybody calls this.
*/
static void
brew_pc_set (sim_cpu *scpu, sim_cia pc)
{
  SIM_DESC sd = CPU_STATE(scpu);

  SIM_ASSERT(false);

  //if (BREW_SIM_CPU (scpu)->sim_state.is_task_mode)
  //  BREW_SIM_CPU (scpu)->sim_state.ntpc = pc;
  //else
  //  BREW_SIM_CPU (scpu)->sim_state.nspc = pc;
}

static void brew_sim_uninstall(SIM_DESC sd)
{
  int c;
  for (c = 0; c < MAX_NR_PROCESSORS; ++c) {
    SIM_CPU *scpu = STATE_CPU(sd, c);
    if (BREW_SIM_CPU (scpu)->model_info != NULL) {
      // A model is set up while the options are parsed (--model), before
      // sim_post_argv_init links the CPU to its state; if sim_open fails
      // in between (e.g. the program can't be opened), the model's done
      // hook would read a NULL CPU_STATE.
      if (CPU_STATE (scpu) == NULL)
        CPU_STATE (scpu) = sd;
      BREW_SIM_CPU (scpu)->sim_state.model_functions.free_model_info(scpu);
    }
    BREW_SIM_CPU (scpu)->model_info = NULL;
    free(BREW_SIM_CPU (scpu)->insn_profile);
    BREW_SIM_CPU (scpu)->insn_profile = NULL;
    if (BREW_SIM_CPU (scpu)->espresso_timing != NULL)
      espresso_timing_free(BREW_SIM_CPU (scpu)->espresso_timing);
    BREW_SIM_CPU (scpu)->espresso_timing = NULL;
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

static const SIM_MODEL brew_models[];

static void
brew_init_cpu(sim_cpu *scpu)
{
  CPU_REG_FETCH(scpu) = brew_reg_fetch;
  CPU_REG_STORE(scpu) = brew_reg_store;
  CPU_PC_FETCH(scpu) = brew_pc_get;
  CPU_PC_STORE(scpu) = brew_pc_set;
#if WITH_PROFILE_INSN_P
  // The common instruction profile (CPU_MAX_INSNS, CPU_INSN_NAME) is unused:
  // brew_profile_info prints Brew's own.
  PROFILE_INFO_CPU_CALLBACK (CPU_PROFILE_DATA (scpu)) = brew_profile_info;
#endif
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
  //{ "anachron",   &brew_mach,       2,          NULL,           anachron_model_init},
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
  STATE_MACHS (sd) = brew_sim_machs;
  STATE_MODEL_NAME (sd) = "brew";
  current_target_byte_order = BFD_ENDIAN_LITTLE;

  /* The cpu data is kept in a separately allocated chunk of memory.  */
  SIM_ASSERT(MAX_NR_PROCESSORS == 1);
  if (sim_cpu_alloc_all_extra (sd, MAX_NR_PROCESSORS, sizeof (struct brew_sim_cpu)) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  if (sim_pre_argv_init (sd, argv[0]) != SIM_RC_OK)
    {
      free_state (sd);
      return 0;
    }

  /* Install our command-line options */
  if (brew_options_init(sd) != SIM_RC_OK)
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

  // If model was specified on the command line, we'll have to setup the simulation here.
  // If not, the model functions would not be set up, so we'll have to defer the call
  // to the point where the model call-backs are initialized (in brew_model_init or espresso_model_init).
  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
    {
      sim_cpu *scpu = STATE_CPU (sd, i);
      if (BREW_SIM_CPU (scpu)->sim_state.model_functions.setup_sim != NULL)
        BREW_SIM_CPU (scpu)->sim_state.model_functions.setup_sim(sd, scpu);
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

      setup_sim_state(scpu);
      BREW_SIM_CPU (scpu)->sim_state.model_functions.reset_cpu(scpu, is_user_mode);
    }

  sim_module_add_uninstall_fn(sd, brew_sim_uninstall);
  //sim_module_add_init_fn (sd, sim_core_init);

  /* Do any simulator-wide setup, such as loading of firmware */
  BREW_SIM_CPU (STATE_CPU(sd, 0))->sim_state.model_functions.setup_sd(sd);

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
      switch (STATE_ENVIRONMENT(CPU_STATE(scpu)))
        {
        case USER_ENVIRONMENT:
        case VIRTUAL_ENVIRONMENT:
          BREW_SIM_CPU (scpu)->sim_state.tpc = STATE_START_ADDR(sd);
          BREW_SIM_CPU (scpu)->sim_state.ntpc = STATE_START_ADDR(sd);
          BREW_SIM_CPU (scpu)->sim_state.is_task_mode = true;
          break;
        case OPERATING_ENVIRONMENT:
          // We're starting the processor from the reset vector, no matter what the BFD was linked to.
          BREW_SIM_CPU (scpu)->sim_state.spc = 0;
          BREW_SIM_CPU (scpu)->sim_state.nspc = 0;
          BREW_SIM_CPU (scpu)->sim_state.is_task_mode = false;
          break;
        default:
          SIM_ASSERT(false);
        }
    }

  // Keep the program's arguments for the SYS_argc/SYS_argnlen/SYS_argn
  // syscalls, which crt0 uses to build argv. Standalone, argv already is
  // STATE_PROG_ARGV (set up by sim_parse_args); under gdb, 'run' can pass
  // new arguments each time.
  if (STATE_PROG_ARGV (sd) != argv)
    {
      freeargv (STATE_PROG_ARGV (sd));
      STATE_PROG_ARGV (sd) = dupargv (argv);
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

