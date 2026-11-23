/* Target-dependent code for brew.

   Copyright (C) 2009-2021 Free Software Foundation, Inc.

   This file is part of GDB.

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

#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "dwarf2/frame.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "cli/cli-cmds.h"
#include "gdbcore.h"
#include "extract-store-integer.h"
#include "value.h"
#include "inferior.h"
#include "symfile.h"
#include "objfiles.h"
#include "osabi.h"
#include "language.h"
#include "arch-utils.h"
#include "regcache.h"
#include "user-regs.h"
#include "trad-frame.h"
#include "dis-asm.h"
#include "record.h"
#include "record-full.h"

#include "brew-tdep.h"
#include "sim/brew/gdb-if.h"
extern "C" {
#include "opcode/brew-abi.h"
#include "opcode/brew.h"
}
#include <algorithm>

// Use an invalid address value as 'not available' marker.
enum { REG_UNAVAIL = 0 };

struct brew_frame_info
{
  bool valid;
  uint32_t prev_sp; // stack pointer of the caller
  uint32_t prev_fp; // Frame pointer for this frame
  uint32_t return_addr; // Return address
  uint32_t entry_point; // Address for the function entry point (including prolog)
  uint32_t prolog_end; // Address of the first instruction after the end of the prolog
  uint32_t framesize; // Number of bytes allocated for this frame. 0 means, the frame doesn't contain any saved registers
  uint32_t saved_regs[BREW_GDB_NUM_REGS]; // Offset for save location for all registers. Since the stack grows down, all offsets are negative. 0 and all positive values are reserved for 'unavailable'
};

static enum register_status regcache_read_unsigned_reg(
  struct regcache *regcache,
  int regnum,
  uint32_t &val
) {
  ULONGEST lval;
  auto ret_val = regcache_cooked_read_unsigned(regcache, regnum, &lval);
  val = (uint32_t(lval));
  return ret_val;
}

/* Implement the "frame_align" gdbarch method.  */

static CORE_ADDR
brew_frame_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  /* Align to the size of an instruction (so that they can safely be
     pushed onto the stack.  */
  return sp & ~1;
}

constexpr gdb_byte brew_break_insn[] = { 0x00, 0x10 }; /* BREAK instruction: 0x1000 (SWI 1)*/

typedef BP_MANIPULATION (brew_break_insn) brew_breakpoint;

// Register names as gdb spells them: without the assembler's '$' (gdb adds
// it in expressions, so "$r8" looks up "r8"), and one word each. $r0-$r14
// are the GPRs (brew_reg_names in opcodes has the assembler names); the
// rest are the virtual registers of sim/brew/gdb-if.h.
static const char * const brew_gdb_reg_names[BREW_GDB_NUM_REGS] = {
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "fp", "sp", "lr",
  "tpc", "spc", "pc", "task_mode"
};

// gdbarch 'method' implementations. The follow the name in gdbarch with the 'brew_' prefix

static const char *brew_register_name(struct gdbarch *gdbarch, int reg_nr)
{
  // gdb only asks for valid register numbers, and the result must never be NULL.
  if (reg_nr < 0 || reg_nr >= BREW_GDB_NUM_REGS)
    return "";
  return brew_gdb_reg_names[reg_nr];
}

// The numbered names of the registers that have an ABI name: $r12 is $fp,
// $r13 $sp and $r14 $lr.
static const struct
{
  const char *name;
  int regnum;
} brew_register_aliases[] = {
  { "r12", BREW_REG_FP },
  { "r13", BREW_REG_SP },
  { "r14", BREW_REG_LINK },
};

static struct value *
brew_value_of_alias(const frame_info_ptr &frame, const void *baton)
{
  return value_of_register((int) (intptr_t) baton,
                           get_next_frame_sentinel_okay(frame));
}

// Map a DWARF register number (gcc's hard register number, see REGISTER_NAMES
// in gcc/config/brew/brew.h) to a gdb register number. $r0-$r14 are the same
// in both. gcc's 15 and 16 are soft frame/arg pointers that are always
// eliminated, so they never appear in debug info, and gdb uses those numbers
// for $tpc/$spc. Anything else is invalid; returning -1 makes gdb report a bad
// register number instead of passing it on to the register callbacks.
static int brew_dwarf2_reg_to_regnum(struct gdbarch *gdbarch, int reg)
{
  if (reg >= 0 && reg < 15)
    return reg;
  if (reg == 17) // gcc's $pc
    return BREW_GDB_REG_PC;
  return -1;
}

// How each register is found in the caller's frame when the CFI
// (.debug_frame) doesn't say. The caller's $pc is the return address and
// its $sp is the CFA (the value of $sp before the call). $r0-$r7 are
// call-clobbered, and the mode-specific PCs and the task-mode flag can't be
// recovered, so those are undefined, as brew_frame_prev_register reports
// them. The call-saved $r8-$r14 keep the default: unchanged unless the CFI
// records a save.
static void brew_dwarf2_frame_init_reg(
  struct gdbarch *gdbarch,
  int regnum,
  struct dwarf2_frame_state_reg *reg,
  const frame_info_ptr &this_frame
) {
  if (regnum == BREW_GDB_REG_PC)
    reg->how = DWARF2_FRAME_REG_RA;
  else if (regnum == BREW_REG_SP)
    reg->how = DWARF2_FRAME_REG_CFA;
  else if (regnum <= BREW_REG_ARG3
           || regnum == BREW_GDB_REG_TPC
           || regnum == BREW_GDB_REG_SPC
           || regnum == BREW_GDB_REG_TASK_MODE)
    reg->how = DWARF2_FRAME_REG_UNDEFINED;
}

// TODO: we should handle true target register types here
static struct type *brew_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  if (reg_nr == BREW_GDB_REG_TPC || reg_nr == BREW_GDB_REG_SPC || reg_nr == BREW_GDB_REG_PC)
    return  builtin_type(gdbarch)->builtin_func_ptr;
  else if (reg_nr == BREW_REG_SP || reg_nr == BREW_REG_FP)
    return builtin_type(gdbarch)->builtin_data_ptr;
  else
    return builtin_type(gdbarch)->builtin_int32;
}

// Write into appropriate registers a function return value of type TYPE, given in virtual format.
static void brew_store_return_value(struct type *type, struct regcache *regcache, const gdb_byte *valbuf)
{
  //struct gdbarch *gdbarch = regcache->arch();
  CORE_ADDR regval;
  int len = type->length ();

  for (int ofs = 0; ofs < len; ofs+=4)
    {
      int regno = BREW_REG_ARG0 + ofs / 4;
      if (regno > BREW_REG_ARG3)
        {
          gdb_printf(gdb_stderr, "FIXME: how to handle return values that are on the stack???");
          // How to handle things that are returned on the stack??
          gdb_assert(false);
        }
      regval = extract_unsigned_integer(valbuf+ofs, (len-ofs) > 4 ? 4 : len-ofs, BFD_ENDIAN_LITTLE);
      regcache_cooked_write_unsigned(regcache, regno, regval);
    }
}


// Some stuff from brew.cc in gcc.
//
//// Order of registers saved on the stack. Largely arbitrary, but the first two entries are important:
////     brew_dynamic_chain_address assumes $fp is in the 1st save slot.
////     brew_return_addr_rtx assumes assumes $lr is in the 2nd save slot.
//static const size_t reg_frame_order[] = {
//  BREW_REG_FP,
//  BREW_REG_LINK,
//  BREW_REG_R0,
//  BREW_REG_R1,
//  BREW_REG_R2,
//  BREW_REG_R3,
//  BREW_REG_R4,
//  BREW_REG_R5,
//  BREW_REG_R6,
//  BREW_REG_R7,
//  BREW_REG_R8,
//  BREW_REG_R9,
//  BREW_REG_R10,
//  BREW_REG_R11,
//  BREW_REG_R13
//};



// Decode the instructions within the given address range.
// Decide when we must have reached the end of the function prologue.
// If a frame_info pointer is provided, fill in its saved_regs etc.
// Essentially we need to recognize whatever `brew_expand_prologue`
// in brew.cc generates.
//
// Returns the address of the first instruction after the prologue.

static inline uint8_t FIELD_A(uint16_t inst) { return (inst & 0x000f) >> 0; }
static inline uint8_t FIELD_B(uint16_t inst) { return (inst & 0x00f0) >> 4; }
static inline uint8_t FIELD_C(uint16_t inst) { return (inst & 0x0f00) >> 8; }
static inline uint8_t FIELD_D(uint16_t inst) { return (inst & 0xf000) >> 12; }

static inline int32_t sign_extend_short_to_32(uint16_t value) {
  if ((value & 0x8000) != 0) return value | 0xffff0000;
  return value;
}

static inline int32_t sign_extend_tiny_to_32(uint16_t value) {
  value = value & 0xf;
  if ((value & 0x8) != 0) return -(~value & 0xf);
  return value;
}

//struct brew_unwind_cache
//{
//  // The previous frame's inner most stack address. Used as this frame ID's stack_addr.
//  CORE_ADDR prev_sp;
//  // The frame's base, optionally used by the high-level debug info.
//  CORE_ADDR base;
//  int size;
//  // How far the $sp and $fp have been offset from the start of
//  // the stack frame (as defined by the previous frame's stack pointer).
//  LONGEST sp_offset;
//  LONGEST fp_offset;
//  int uses_frame;
//  // Table indicating the location of each and every register.
//  trad_frame_saved_reg *saved_regs;
//};


template <class data_type> static bool read_memory_uint(CORE_ADDR addr, enum bfd_endian byte_order, data_type &data) {
  ULONGEST raw_data;
  if (safe_read_memory_unsigned_integer(addr, sizeof(data), byte_order, &raw_data) == 0)
    return false;
  data = raw_data;
  return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION PROLOGUE HANDLING
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////


// Decode '$sp <- N + $sp' at ADDR (tiny, short or long immediate, as GCC
// emits it to allocate and free the frame). On success, *LEN is the
// instruction's length and *OFFSET its N.
static bool brew_sp_adjust(
  CORE_ADDR addr,
  enum bfd_endian byte_order,
  int *len,
  int32_t *offset
) {
  uint16_t inst;
  if (!read_memory_uint(addr, byte_order, inst))
    return false;
  if ((inst & 0xfff0) == 0xdbd0) {
    *len = 2;
    *offset = sign_extend_tiny_to_32(FIELD_A(inst));
    return true;
  }
  if (inst == 0xd4fd) {
    uint16_t short_offset;
    if (!read_memory_uint(addr + 2, byte_order, short_offset))
      return false;
    *len = 4;
    *offset = sign_extend_short_to_32(short_offset);
    return true;
  }
  if (inst == 0xd4df) {
    *len = 6;
    return read_memory_uint(addr + 2, byte_order, *offset);
  }
  return false;
}

// The reload of REG from the save slot at OFS: '$rD <- mem32[$sp - tiny N]'.
static uint16_t brew_slot_reload(size_t reg, uint32_t ofs)
{
  return (reg << 12) | 0x0d01 | (ofs & 0x1fc) >> 1;
}

// Is PC in the tail of an epilogue, after '$sp <- N + $sp' put $sp back at the
// entry value? There the epilogue reloads the saved registers from below $sp
// ('$rD <- mem32[$sp - tiny N]', the prologue's stores with the load opcode),
// the first save slot ($fp, or $lr in a function without a frame pointer)
// last, then returns or jumps (sibcall). Function bodies never load from
// below $sp, so a run of such reloads that match FRAME_INFO's save slots
// identifies the epilogue. With no reload left, a return ('$pc <- $lr') is
// in the epilogue; a sibcall jump is if it follows the last reload or, in a
// function that saves nothing, the '$sp <- N + $sp' that frees FRAMESIZE.
// On success, *PENDING has a bit for each register whose reload is ahead.
static bool brew_in_epilogue(
  CORE_ADDR pc,
  const struct brew_frame_info *frame_info,
  int32_t framesize,
  enum bfd_endian byte_order,
  uint32_t *pending
) {
  *pending = 0;
  uint16_t inst;
  CORE_ADDR addr = pc;
  while (read_memory_uint(addr, byte_order, inst)
         && FIELD_C(inst) == 0xd && (inst & 1) == 1 && FIELD_D(inst) != 0xf) {
    int32_t offset = (inst & 0xfe) << 1;
    offset |= (offset & 0x100) != 0 ? 0xfffffe00 : 0;
    size_t reg_idx = FIELD_D(inst);
    if (frame_info->saved_regs[reg_idx] == REG_UNAVAIL
        || (uint32_t) offset != frame_info->saved_regs[reg_idx])
      return false;
    *pending |= 1u << reg_idx;
    addr += 2;
  }
  if (*pending != 0)
    return true;
  if (!read_memory_uint(pc, byte_order, inst))
    return false;
  if (inst == 0xe002)
    return true;
  // A sibcall jump ('$pc <- $rN' or '$pc <- N'): what precedes it?
  if (inst != 0x2fef && (inst & 0x0fff) != 0x0002)
    return false;
  bool saves = false;
  for (size_t r = 0; r < BREW_GDB_NUM_REGS; ++r)
    if (frame_info->saved_regs[r] != REG_UNAVAIL) {
      saves = true;
      if (frame_info->saved_regs[r] == (uint32_t) -4)
        return pc >= 2 && read_memory_uint(pc - 2, byte_order, inst)
               && inst == brew_slot_reload(r, -4);
    }
  if (saves || framesize == 0)
    return false;
  for (int len = 2; len <= 6; len += 2) {
    int l;
    int32_t ofs;
    if (pc >= (CORE_ADDR) len && brew_sp_adjust(pc - len, byte_order, &l, &ofs)
        && l == len && ofs == framesize)
      return true;
  }
  return false;
}

// Analyze a function prolog and fill the associated frame_info record.
// This contains all the saved registers (or rather the memory offsets to them)
// and some minimal info about the PC/SP/FP associated with the frame.
//
// This function is called in two contexts: one is a static one, with the whole function
// enclosed in the start_addr...end_addr range. In this case, 'this_frame' is NULL, but all we
// really care about is the end of the prolog, i.e. the return value.
// The other is a dynamic context, when end_addr is set to the current PC and this_frame
// is actually valid. In this case, we don't care about the return address at all, but we
// should be accurate in filling in the struct.
//
// On return, we return the end of the prolog, or start_addr if we can't find the end of the prolog
// for some reason.
// Describe a frame that has no prologue (yet): the caller's $sp is the
// current one, its $pc is in $lr, and every register still holds the
// caller's value.
static bool brew_frameless_frame(
  CORE_ADDR start_addr,
  CORE_ADDR cur_fp,
  struct brew_frame_info *frame_info,
  const frame_info_ptr &this_frame
) {
  for (auto &ofs : frame_info->saved_regs)
    ofs = REG_UNAVAIL;
  if (this_frame != NULL) {
    frame_info->prev_sp = get_frame_register_unsigned(this_frame, BREW_REG_SP);
    frame_info->prev_fp = cur_fp;
    frame_info->return_addr = get_frame_register_unsigned(this_frame, BREW_REG_LINK);
  } else {
    frame_info->prev_sp = 0;
    frame_info->prev_fp = 0;
  }
  frame_info->prolog_end = start_addr;
  frame_info->framesize = 0;
  frame_info->valid = true;
  return true;
}

static bool brew_analyze_prolog(
  CORE_ADDR start_addr,
  CORE_ADDR cur_pc,
  CORE_ADDR cur_fp,
  struct brew_frame_info *frame_info,
  const frame_info_ptr &this_frame,
  struct gdbarch *gdbarch
) {
  enum bfd_endian byte_order = gdbarch_byte_order(gdbarch);
  CORE_ADDR inst_addr = start_addr;
  uint16_t inst;
  int32_t offset = 0;

  // We are looking for something like this:
  //
  // 8000224:  ff cc               mem32[$sp - tiny 4 (0xfffffffc)] <- $fp
  // 8000226:  fd ec               mem32[$sp - tiny 8 (0xfffffff8)] <- $lr
  // 8000228:  fb 8c               mem32[$sp - tiny 12 (0xfffffff4)] <- $r8
  // 800022a:  f9 9c               mem32[$sp - tiny 16 (0xfffffff0)] <- $r9
  // 800022c:  dd c2               $fp <- $sp
  // 800022e:  fd d4 e0 ff         $sp <- short -32 (0xffffffe0) + $sp
  //
  // Only functions that need a frame pointer (alloca, __builtin_eh_return)
  // have '$fp <- $sp'; the others address their frame through $sp, and
  // save $fp (an ordinary call-saved register there) only if they use it.
  // Each part is optional: a leaf may save nothing, or have no frame at all.

  frame_info->valid = false;
  frame_info->entry_point = start_addr;

  if (start_addr == 0)
    return false;

  gdb_assert(start_addr <= cur_pc);

  // find all the mem32[$sp - xxx] <- yyy instructions
  while (true) {
    if (!read_memory_uint(inst_addr, byte_order, inst))
      return false;
    // The pattern we're looking for the register save instructions is: 0x.c** with LSB set ($sp-based)
    if (FIELD_C(inst) == 0xc && (inst & 1) == 1 && FIELD_D(inst) != 0xf) {
      offset = (inst & 0xfe) << 1;
      // sign-extend offset:
      offset |= (offset & 0x100) != 0 ? 0xfffffe00 : 0;
      size_t reg_idx = FIELD_D(inst);
      // At cur_pc only the stores before it have been executed; a register
      // whose store is still ahead holds the caller's value.
      if (this_frame == NULL || inst_addr < cur_pc)
        frame_info->saved_regs[reg_idx] = offset;
      inst_addr += 2;
    } else {
      break;
    }
  }
  bool saves = inst_addr != start_addr;

  // The optional '$fp <- $sp'.
  bool has_fp = inst == 0xc2dd;
  bool fp_set = false;
  if (has_fp) {
    fp_set = this_frame == NULL || cur_pc > inst_addr;
    inst_addr += 2;
  }

  // The $sp adjustment. Saves are always followed by one.
  int adjust_len;
  if (brew_sp_adjust(inst_addr, byte_order, &adjust_len, &offset) && offset < 0) {
    inst_addr += adjust_len;
  } else {
    // A function that doesn't start with the standard prologue is a
    // frameless leaf, such as setjmp or other hand-written assembly without
    // CFI.
    if (!saves && !has_fp)
      return brew_frameless_frame(start_addr, cur_fp, frame_info, this_frame);
    return false;
  }
  int32_t framesize = -offset;
  // At a function's entry nothing has run yet, so any function is frameless
  // there.
  if (this_frame != NULL && cur_pc == start_addr)
    return brew_frameless_frame(start_addr, cur_fp, frame_info, this_frame);

  // If we've gotten this far, we managed to parse the prolog.
  if (this_frame != NULL) {
    CORE_ADDR cur_sp = get_frame_register_unsigned(this_frame, BREW_REG_SP);
    // The prologue stores below the entry $sp (the caller's). A frame
    // pointer is set to it; otherwise it lies FRAMESIZE above $sp once the
    // adjustment has run (the frame has a fixed size without one).
    if (fp_set)
      frame_info->prev_sp = cur_fp;
    else if (cur_pc >= inst_addr)
      frame_info->prev_sp = cur_sp + framesize;
    else
      frame_info->prev_sp = cur_sp;

    // In the epilogue $sp is back at the entry value and the saved registers
    // are being reloaded. Reloads still ahead keep their slots; the
    // registers already reloaded hold the caller's values again.
    uint32_t pending;
    if (cur_pc >= inst_addr
        && brew_in_epilogue(cur_pc, frame_info, framesize, byte_order, &pending)) {
      frame_info->prev_sp = cur_sp;
      for (int r = 0; r < BREW_GDB_NUM_REGS; ++r)
        if ((pending & (1u << r)) == 0)
          frame_info->saved_regs[r] = REG_UNAVAIL;
    }

    // A saved $fp is the caller's; otherwise $fp still holds it.
    if (frame_info->saved_regs[BREW_REG_FP] == REG_UNAVAIL) {
      frame_info->prev_fp = cur_fp;
    } else {
      int32_t fp_ofs = frame_info->saved_regs[BREW_REG_FP];
      uint32_t old_fp;
      if (!read_memory_uint(frame_info->prev_sp+fp_ofs, byte_order, old_fp))
        return false;
      frame_info->prev_fp = old_fp;
    }
    if (frame_info->saved_regs[BREW_REG_LINK] != REG_UNAVAIL) {
      int32_t lr_ofs = frame_info->saved_regs[BREW_REG_LINK];
      uint32_t return_addr;
      if (!read_memory_uint(frame_info->prev_sp+lr_ofs, byte_order, return_addr))
        return false;
      frame_info->return_addr = return_addr;
    } else {
      frame_info->return_addr = get_frame_register_unsigned(this_frame, BREW_REG_LINK);
    }
  } else {
    frame_info->prev_sp = 0;
    frame_info->prev_fp = 0;
  }

  frame_info->prolog_end = inst_addr;
  frame_info->framesize = framesize;
  frame_info->valid = true;

  return true;
}

// Find the end of function prologue.
static CORE_ADDR brew_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  CORE_ADDR func_addr = 0, func_end = 0;
  const char *func_name;

  // See if we can determine the end of the prologue via the symbol table.
  // If so, then return either PC, or the PC after the prologue, whichever
  // is greater.
  if (!find_pc_partial_function(pc, &func_name, &func_addr, &func_end))
    {
      // No function symbol -- just return the PC.
      return (CORE_ADDR) pc;
    }

  CORE_ADDR post_prologue_pc = skip_prologue_using_sal(gdbarch, func_addr);
  // skip_prologue_using_sal treats two line-table rows at the function's
  // start address as an empty prologue and returns FUNC_ADDR. Newer GCC
  // (16 for Brew, which writes its own line table; 13 did not) emits exactly
  // that for optimized code: the entry line as a statement and again as a
  // non-statement location view. Trusting it would make 'step' into a
  // function stop on its opening brace, so ask the instruction analyzer
  // instead. (The first-line fallback below is not safe here: when the first
  // statement starts at FUNC_ADDR too, its line can extend into the body.)
  bool empty_by_sal = post_prologue_pc == func_addr;
  if (post_prologue_pc != 0 && !empty_by_sal)
    {
      return std::max(pc, post_prologue_pc);
    }

  // Try to find line number info for the first line in the source code as a marker
  // for the end of prologue. That is, except for assembly, which has no such concept.
  struct symtab_and_line sal;
  struct symbol *sym;
  // Find the function.
  sym = lookup_symbol(func_name, nullptr, SEARCH_FUNCTION_DOMAIN, nullptr).symbol;
  if (sym && sym->language() == language_asm)
    {
      return func_addr;
    }
  sal = find_sal_for_pc(func_addr, 0);
  if (!empty_by_sal && sal.end && sal.end < func_end)
    {
      // Found a line number, use it as end of prologue.
      return sal.end;
    }

  // Can't determine prologue from the symbol table, need to examine instructions.
  struct brew_frame_info frame_info;
  memset(&frame_info, 0, sizeof frame_info);
  if (!brew_analyze_prolog(func_addr, func_end, 0, &frame_info, NULL, gdbarch)) {
    // couldn't parse the prolog, return the function start.
    return func_addr;
  }
  gdb_assert(frame_info.valid);
  return frame_info.prolog_end;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SINGLE STEPPING
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// Find where the current instruction might go and return a list of target addresses.
// These later will be used for inserting break-points for single-stepping.
static std::vector<CORE_ADDR> brew_software_single_step(struct regcache *regcache)
{
  struct gdbarch *gdbarch = regcache->arch();
  CORE_ADDR cur_pc;
  uint16_t inst;
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  std::vector<CORE_ADDR> next_pcs;
  uint32_t next_pc;

  cur_pc = regcache_read_pc(regcache);

  inst = (uint16_t)read_memory_unsigned_integer(cur_pc, 2, byte_order);
  int inst_len = brew_insn_len(inst);
  next_pc = cur_pc + inst_len;

  // Normally we simply return the address after the instruction. However, branches need special treatment.

  // 0x8000  STM         Enters TASK mode, enables interrupts
  if (brew_pattern_match(inst, "8000")) {
    uint32_t reg_val;
    if (regcache_read_unsigned_reg(regcache, BREW_GDB_REG_TPC, reg_val) == REG_VALID)
      next_pcs.push_back(reg_val);
    return next_pcs;
  }

  // 0x.002 $pc <- $rD     Indirect jump
  if (brew_pattern_match(inst, ".002")) {
    uint32_t reg_val;
    if (regcache_read_unsigned_reg(regcache, FIELD_D(inst), reg_val) == REG_VALID)
      next_pcs.push_back(reg_val);
    return next_pcs;
  }
  // 0x.003 $tpc <- $rD    Update $tpc
  if (brew_pattern_match(inst, ".003")) {
    uint32_t reg_val;
    if (regcache_read_unsigned_reg(regcache, FIELD_D(inst), reg_val) == REG_VALID)
    {
      next_pcs.push_back(reg_val);
      next_pcs.push_back(next_pc);
    }
    return next_pcs;
  }

  // 0x20ef 0x**** 0x****  $pc <- VALUE                Unconditional jump
  // 0x40ef 0x**** 0x****  call VALUE                  $pc <- VALUE; $lr <- $pc
  if (
    brew_pattern_match(inst, "20ef") ||
    brew_pattern_match(inst, "40ef")
  ) {
    uint32_t field_e = (uint32_t)read_memory_unsigned_integer(cur_pc + 2, 4, byte_order);
    next_pcs.push_back(field_e);
    return next_pcs;
  }
  // 0x30ef 0x**** 0x****  $tpc <- VALUE               Load immediate to $tpc
  if (brew_pattern_match(inst, "30ef")) {
    uint32_t field_e = (uint32_t)read_memory_unsigned_integer(cur_pc + 2, 4, byte_order);
    next_pcs.push_back(field_e);
    next_pcs.push_back(next_pc);
    return next_pcs;
  }


  // 0x20fe 0x**** $pc <- short VALUE          Immediate short jump (value is sign-extended)
  // 0x40fe 0x**** call short VALUE            $pc <- VALUE; $lr <- $pc
  if (
    brew_pattern_match(inst, "20fe") ||
    brew_pattern_match(inst, "40fe")
  ) {
    uint32_t field_e = (uint32_t)sign_extend_short_to_32((uint16_t)read_memory_unsigned_integer(cur_pc + 2, 2, byte_order));
    next_pcs.push_back(field_e);
    return next_pcs;
  }
  //0x30fe 0x**** $tpc <- short VALUE         Load sign-extended value into $tpc
  if (brew_pattern_match(inst, "30fe")) {
    uint32_t field_e = (uint32_t)sign_extend_short_to_32((uint16_t)read_memory_unsigned_integer(cur_pc + 2, 2, byte_order));
    next_pcs.push_back(field_e);
    next_pcs.push_back(next_pc);
    return next_pcs;
  }

  // 0x2ee.  $pc <- MEM32[$rA]               32-bit load from MEM[$rA] into $PC
  // 0x4ee.  call MEM32[$rA]                 32-bit load from MEM[$rA] into $PC; $LR <- $PC
  if (
    brew_pattern_match(inst, "2fe.") ||
    brew_pattern_match(inst, "4fe.")
  ) {
    uint32_t base;
    if (regcache_read_unsigned_reg(regcache, FIELD_A(inst), base) == REG_VALID)
      {
        uint32_t target = read_memory_unsigned_integer(base, 4, byte_order);
        next_pcs.push_back(target);
      }
    return next_pcs;
  }
  // 0x3ee.  $tpc <- MEM32[$rA]              32-bit load from MEM[$rA] into $TPC
  if (brew_pattern_match(inst, "3fe.")) {
    uint32_t base;
    if (regcache_read_unsigned_reg(regcache, FIELD_A(inst), base) == REG_VALID)
      {
        uint32_t target = read_memory_unsigned_integer(base, 4, byte_order);
        next_pcs.push_back(target);
        next_pcs.push_back(next_pc);
      }
    return next_pcs;
  }



  // 0x2fe. 0x**** $pc <- MEM32[$rA + VALUE]               32-bit load from MEM[$rA+VALUE] into $PC
  // 0x4fe. 0x**** call MEM32[$rA + VALUE]                 32-bit load from MEM[$rA+VALUE] into $PC; $LC <- $pc
  if (
    brew_pattern_match(inst, "2fe.") ||
    brew_pattern_match(inst, "4fe.")
  ) {
    uint32_t field_e = (uint32_t)sign_extend_short_to_32((uint16_t)read_memory_unsigned_integer(cur_pc + 2, 2, byte_order));
    uint32_t base;
    if (regcache_read_unsigned_reg(regcache, FIELD_A(inst), base) == REG_VALID)
      {
        uint32_t target = read_memory_unsigned_integer((uint32_t)(base + field_e), 4, byte_order);
        next_pcs.push_back(target);
      }
    return next_pcs;
  }
  // 0x3fe. 0x****  $tpc <- MEM32[$rA + VALUE]              32-bit load from MEM[$rA+VALUE] into $TPC
  if (brew_pattern_match(inst, "3fe.")) {
    uint32_t field_e = (uint32_t)sign_extend_short_to_32((uint16_t)read_memory_unsigned_integer(cur_pc + 2, 2, byte_order));
    uint32_t base;
    if (regcache_read_unsigned_reg(regcache, FIELD_A(inst), base) == REG_VALID)
      {
        uint32_t target = read_memory_unsigned_integer((uint32_t)(base + field_e), 4, byte_order);
        next_pcs.push_back(target);
        next_pcs.push_back(next_pc);
      }
    return next_pcs;
  }


  // 0x2fef 0x**** 0x****  $pc <- MEM32[VALUE]         32-bit load from MEM[VALUE] into $PC
  // 0x4fef 0x**** 0x****  call MEM32[VALUE]           32-bit load from MEM[VALUE] into $PC, $LR <- $PC
  if (
    brew_pattern_match(inst, "2fef") ||
    brew_pattern_match(inst, "4fef")
  ) {
    uint32_t field_e = (uint32_t)read_memory_unsigned_integer(cur_pc + 2, 4, byte_order);
    uint32_t target = read_memory_unsigned_integer(field_e, 4, byte_order);
    next_pcs.push_back(target);
    return next_pcs;
  }
  // 0x3fef 0x**** 0x****  $tpc <- MEM32[VALUE]        32-bit load from MEM[VALUE] into $TPC
  if (brew_pattern_match(inst, "3fef")) {
    uint32_t field_e = (uint32_t)read_memory_unsigned_integer(cur_pc + 2, 4, byte_order);
    uint32_t target = read_memory_unsigned_integer(field_e, 4, byte_order);
    next_pcs.push_back(target);
    next_pcs.push_back(next_pc);
    return next_pcs;
  }



  // 0xf00. 0x****  if any $rA == 0  $pc <- $pc + VALUE
  // 0xf01. 0x****  if any $rA != 0  $pc <- $pc + VALUE
  // 0xf02. 0x****  if any $rA < 0   $pc <- $pc + VALUE                signed compare
  // 0xf03. 0x****  if any $rA >= 0  $pc <- $pc + VALUE                signed compare
  // 0xf04. 0x****  if any $rA > 0   $pc <- $pc + VALUE                signed compare
  // 0xf05. 0x****  if any $rA <= 0  $pc <- $pc + VALUE                signed compare
  // 0xf08. 0x****  if all $rA == 0  $pc <- $pc + VALUE
  // 0xf09. 0x****  if all $rA != 0  $pc <- $pc + VALUE
  // 0xf0a. 0x****  if all $rA < 0   $pc <- $pc + VALUE                signed compare
  // 0xf0b. 0x****  if all $rA >= 0  $pc <- $pc + VALUE                signed compare
  // 0xf0c. 0x****  if all $rA > 0   $pc <- $pc + VALUE                signed compare
  // 0xf0d. 0x****  if all $rA <= 0  $pc <- $pc + VALUE                signed compare
  //
  // 0xf1.. 0x****  if any $rB == $rA   $pc <- $pc + VALUE
  // 0xf2.. 0x****  if any $rB != $rA   $pc <- $pc + VALUE
  // 0xf3.. 0x****  if any signed $rB < $rA  $pc <- $pc + VALUE
  // 0xf4.. 0x****  if any signed $rB >= $rA $pc <- $pc + VALUE
  // 0xf5.. 0x****  if any $rB < $rA    $pc <- $pc + VALUE
  // 0xf6.. 0x****  if any $rB >= $rA   $pc <- $pc + VALUE
  // 0xf9.. 0x****  if all $rB == $rA   $pc <- $pc + VALUE
  // 0xfa.. 0x****  if all $rB != $rA   $pc <- $pc + VALUE
  // 0xfb.. 0x****  if all signed $rB < $rA  $pc <- $pc + VALUE
  // 0xfc.. 0x****  if all signed $rB >= $rA $pc <- $pc + VALUE
  // 0xfd.. 0x****  if all $rB < $rA    $pc <- $pc + VALUE
  // 0xfe.. 0x****  if all $rB >= $rA   $pc <- $pc + VALUE
  //
  // 0xf.f. 0x****  if $rA[0]  == 1 $pc <- $pc + VALUE
  // 0xf..f 0x****  if $rB[0]  == 0 $pc <- $pc + VALUE
  if (
    (FIELD_B(inst) <= 0xd &&                         brew_pattern_match(inst, "f0..")) ||
    (FIELD_C(inst) <= 0xd && FIELD_C(inst) >= 0x1 && brew_pattern_match(inst, "f...")) ||
    (                                                brew_pattern_match(inst, "f.f.")) ||
    (                                                brew_pattern_match(inst, "f..f"))
  ) {
    int32_t offset = brew_unmunge_address((uint16_t)read_memory_unsigned_integer(cur_pc + 2, 2, byte_order));
    next_pcs.push_back(cur_pc + offset);
    next_pcs.push_back(next_pc);
    return next_pcs;
  }

  // Check SWI instructions too: these return to SCHEDULER mode if in TASK mode, otherwise reset the CPU
  if ((inst & 0x0fff) == 0 && FIELD_D(inst) <= 0x7) {
    uint32_t spc;
    if (regcache_read_unsigned_reg(regcache, BREW_GDB_REG_SPC, spc) == REG_VALID)
      {
        next_pcs.push_back(spc);
        next_pcs.push_back(0);
      }
    return next_pcs;
  }

  // This is not a branch instruction: simply continue
  next_pcs.push_back(next_pc);
  return next_pcs;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RETURN VALUE EXTRACTION
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// Extract the return value from the registers. We are given the return type in 'type'.
static void brew_extract_return_value(struct type *type, struct regcache *regcache, gdb_byte *dst)
{
  struct gdbarch *gdbarch = regcache->arch();
  enum bfd_endian byte_order = gdbarch_byte_order(gdbarch);
  int len = type->length (); // NOTE: must be signed: we're decrementing by 4 no matter how much we need, so len might turn negative.
  int reg_idx = 0;
  uint32_t tmp;

  // We don't care too much about errors here: worst case we lie about value...
  //
  // We can't get here if the return value is not in registers, so it must be
  // less than 16 bytes.
  //
  // This is determined by `brew_return_in_memory` in `brew.cc` inside gcc.
  //
  gdb_assert(len <= 16);
  while (len > 0 && reg_idx < 4) {
    regcache_read_unsigned_reg(regcache, BREW_REG_ARG0 + reg_idx, tmp);
    store_unsigned_integer(dst, (len > 4 ? 4 : len), byte_order, tmp);
    len -= 4;
    reg_idx += 1;
  }
}

// Implement the "return_value" gdbarch method.
static enum return_value_convention brew_return_value(
  struct gdbarch *gdbarch,
  struct value *function,
  struct type *valtype,
  struct regcache *regcache,
  gdb_byte *readbuf,
  const gdb_byte *writebuf
) {
  // We use 4 registers to store the return value. If the value doesn't fit, we
  // use memory.
  //
  // This is determined by `brew_return_in_memory` in `brew.cc` inside gcc.
  //
  if (valtype->length () > 16)
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
    {
      if (readbuf != NULL)
        brew_extract_return_value(valtype, regcache, readbuf);
      if (writebuf != NULL)
        brew_store_return_value(valtype, regcache, writebuf);
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FRAME INFO 'METHODS' (STACK WALKING)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// Allocate and initialize a brew_frame_info object.
static struct brew_frame_info *brew_alloc_frame_info (void)
{
  struct brew_frame_info *frame_info;

  frame_info = frame_obstack_zalloc<brew_frame_info>();

  frame_info->valid = false;

  return frame_info;
}

// Populate a brew_frame_info object for this_frame.
static struct brew_frame_info *brew_frame_info_fill(const frame_info_ptr &this_frame, void **this_cache)
{
  struct brew_frame_info *frame_info;

  if (*this_cache != NULL)
    return (struct brew_frame_info*) *this_cache;

  frame_info = brew_alloc_frame_info();
  *this_cache = frame_info;

  ULONGEST fp = get_frame_register_unsigned(this_frame, BREW_REG_FP);

  // The program's entry function (crt0's __start) is the outermost frame.
  // ($fp can't mark it: crt0 zeroes $fp, but functions without a frame
  // pointer leave it zero or use it as an ordinary register.)
  std::optional<CORE_ADDR> entry
    = current_program_space->entry_point_address_query();
  if (entry.has_value() && get_frame_func(this_frame) == *entry)
    return frame_info;

  bool success = brew_analyze_prolog(
    get_frame_func(this_frame),
    get_frame_pc(this_frame),
    fp,
    frame_info,
    this_frame,
    get_frame_arch(this_frame)
  );
  gdb_assert(success == frame_info->valid);

  return frame_info;
}

// Given a GDB frame, determine the address of the calling function's
// frame.  This will be used to create a new GDB frame struct.
// I think what's going on here, is the following:
// We are given:
// - the current PC (get_frame_pc(this_frame))
// - the entry point current function (get_frame_func(this_frame))
// - the current FP (get_frame_register_unsigned(this_frame, BREW_REG_FP))
// - the current SP (get_frame_register_unsigned(this_frame, BREW_REG_SP))
// Compute the following:
// - the SP of the calling function (that is our FP when within the function)
// - the entry-point of the current function
//
// These outputs are used to ID the frame, and are something that should not change
// during the execution of the function. That is, the same ID should be returned when
// we're in the prolog, function body or epilog of the function.
static void brew_frame_this_id(
  const frame_info_ptr &this_frame,
  void **this_prologue_cache,
  struct frame_id *this_id
) {
  struct brew_frame_info *frame_info = brew_frame_info_fill(this_frame, this_prologue_cache);

  //printf("=========== frame_info->prev_sp: 0x%08x;       frame_info->entry_point: 0x%08x,       frame_info->framesize: %d (%s)\n", frame_info->prev_sp, frame_info->entry_point, frame_info->framesize, frame_info->valid ? "valid" : "-");

  if (!frame_info->valid)
    return;

  *this_id = frame_id_build(frame_info->prev_sp, frame_info->entry_point);
}

// Get the value of register regnum in the previous stack frame.
static struct value *brew_frame_prev_register(
  const frame_info_ptr &this_frame,
  void **this_prologue_cache,
  int regnum
) {
  struct brew_frame_info *frame_info = brew_frame_info_fill(this_frame, this_prologue_cache);

  gdb_assert(regnum >= 0);
  // A frame the analyzer can't describe is the outermost one
  // (brew_frame_unwind_stop_reason): nothing of the caller is known.
  if (!frame_info->valid)
    return frame_unwind_got_optimized(this_frame, regnum);

  if (regnum == BREW_REG_SP) {
    return frame_unwind_got_constant(this_frame, regnum, frame_info->prev_sp);
  }
  if (regnum == BREW_REG_FP) {
    return frame_unwind_got_constant(this_frame, regnum, frame_info->prev_fp);
  }
  if (regnum == BREW_GDB_REG_PC) {
    // TODO: how to know what the *size* of the call instruction was????
    //       for now, we're returning the return address, but that might not be what we want
    // Apparently ARM (arm-tdep.c) does a similar number: returning the value of LR, i.e. the return address
    // instead of the call address. ARC (arc-tdep.c) does it too. AARCH64 (aarch64-tdep.c) does it as well.
    // So, for now, I'm going to do it without too much heartburn.
    return frame_unwind_got_constant(this_frame, regnum, frame_info->return_addr);
  }

  if (regnum < BREW_GDB_NUM_REGS && frame_info->saved_regs[regnum] != REG_UNAVAIL) {
    return frame_unwind_got_memory(this_frame, regnum, frame_info->prev_sp + frame_info->saved_regs[regnum]);
  }
  // Not saved (or already reloaded): a call-saved register still holds the
  // caller's value, while the call-clobbered ones, $lr and the PCs/mode
  // are lost (the same defaults as brew_dwarf2_frame_init_reg).
  if (regnum > BREW_REG_ARG3 && regnum < BREW_REG_LINK)
    return frame_unwind_got_register(this_frame, regnum, regnum);
  return frame_unwind_got_optimized(this_frame, regnum);
}

// Return the base address of this_frame.
static CORE_ADDR brew_frame_base_address (const frame_info_ptr &this_frame, void **this_cache)
{
  struct brew_frame_info *frame_info = brew_frame_info_fill(this_frame, this_cache);
  // Outermost frame or code the analyzer doesn't understand.
  if (!frame_info->valid)
    return get_frame_sp(this_frame);
  return frame_info->prev_sp;
}

// The outermost frame (the entry function) and code the analyzer doesn't understand
// end the backtrace.
static enum unwind_stop_reason brew_frame_unwind_stop_reason(
  const frame_info_ptr &this_frame,
  void **this_prologue_cache
) {
  struct brew_frame_info *frame_info = brew_frame_info_fill(this_frame, this_prologue_cache);

  return frame_info->valid ? UNWIND_NO_REASON : UNWIND_OUTERMOST;
}

static const struct frame_unwind_legacy brew_frame_unwind(
  "brew prologue",
  NORMAL_FRAME,
  FRAME_UNWIND_ARCH,
  brew_frame_unwind_stop_reason,
  brew_frame_this_id,
  brew_frame_prev_register,
  NULL,
  default_frame_sniffer
);

static const struct frame_base brew_frame_base = {
  &brew_frame_unwind,
  brew_frame_base_address,
  brew_frame_base_address,
  brew_frame_base_address
};





//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// STATE CHANGE DECODER
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// Parse the current instruction and record the values of the registers and
// memory that will be changed in current instruction to "record_arch_list".
// Return -1 if something wrong.

static int brew_process_record (struct gdbarch *gdbarch, struct regcache *regcache, CORE_ADDR addr)
{
  uint16_t inst;
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  if (record_debug > 1)
    gdb_printf (gdb_stdlog, "Process record: brew_process_record addr = 0x%s\n", paddress(current_inferior ()->arch (), addr));

  inst = (uint16_t)read_memory_unsigned_integer(addr, 2, byte_order);

  // Most instructions modify FIELD_D with very few exceptions. Stores of course store at some memory location.

  // TODO: should we handle SWI (SYSCALLs) in some special way? Such as properly registering all the registers that might get changed during the execution of the syscall?

  // Things without a target
  //   0x0000                                SWI 0                                              Used to fill unused code-pages
  //   0x1000                                SWI 1                                              Used for software breakpoints
  //   0x2000                                SWI 2                                              Used to implement system calls
  //   0x3000                                SWI 3
  //   0x4000                                SWI 4
  //   0x5000                                SWI 5
  //   0x6000                                SWI 6
  //   0x7000                                SWI 7
  //   0x8000                                STM                                                Enters TASK mode, enables interrupts
  //   0x9000                                WOI                                                Wake on interrupt. Waits for interrupt in both TASK and SCHEDULER mode
  //   0xa000                                PFLUSH                                             Flushes the pipeline
  //   0x.001                                FENCE_*                                            Fence reads/writes before reads/writes after
  //   0x.0f9 0x****                         CSR[ADDR] <- $rD                                   Store $rD in CSR
  if ((inst & 0x0fff) == 000 && FIELD_D(inst) <= 0xa) goto finish;
  if (brew_pattern_match(inst, ".001")) goto finish;
  if (brew_pattern_match(inst, ".0f9")) goto finish;

  // Type manipulation
  //   0x.0c.                                type $rD <- $rA                                    Sets type of $rD as denoted by $rA
  //   0x.0e.                                type $rD <- FIELD_A                                Sets type of $rD
  //   0x80ef 0x**** 0x****                  type $r0...$r7 <- VALUE                            Load immediate type values [#note_immedate_types]_
  //   0x90ef 0x**** 0x****                  type $r8...$r14 <- VALUE                           Load immediate type values [#note_immedate_types]_
  if (brew_pattern_match(inst, ".0c.")) goto finish;
  if (brew_pattern_match(inst, ".0e.")) goto finish;
  if (inst == 0x80ef || inst == 0x90ef) goto finish;

  // Calls
  //   0x40ef 0x**** 0x****                  call VALUE                                         $pc <- VALUE; $lr <- $pc
  //   0x40fe 0x****                         call short VALUE                                   $pc <- VALUE; $lr <- $pc
  //   0x4ee.                                call MEM32[$rA]                                    32-bit load from MEM[$rA] into $PC; $LR <- $PC
  //   0x4fe. 0x****                         call MEM32[$rA + VALUE]                            32-bit load from MEM[$rA+VALUE] into $PC; $LC <- $pc
  //   0x4fef 0x**** 0x****                  call MEM32[VALUE]                                  32-bit load from MEM[VALUE] into $PC, $LR <- $PC
  if (inst == 0x40ef || inst == 0x40fe || inst == 0x4fef || brew_pattern_match(inst, "4fe.") || brew_pattern_match(inst, "4ee."))
    {
      if (record_full_arch_list_add_reg(regcache, BREW_REG_LINK) != 0)
        return -1;
      goto finish;
    }

  // Branches
  //   0x.002                                $pc <- $rD                                         Indirect jump
  //   0x.003                                $tpc <- $rD                                        Update $tpc
  //   0x20ef 0x**** 0x****                  $pc <- VALUE                                       Unconditional jump
  //   0x30ef 0x**** 0x****                  $tpc <- VALUE                                      Load immediate to $tpc
  //   0x20fe 0x****                         $pc <- short VALUE                                 Immediate short jump (value is sign-extended)
  //   0x30fe 0x****                         $tpc <- short VALUE                                Load sign-extended value into $tpc
  //   0xf00. 0x****                         if any $rA == 0  $pc <- $pc + VALUE
  //   0xf01. 0x****                         if any $rA != 0  $pc <- $pc + VALUE
  //   0xf02. 0x****                         if any $rA < 0   $pc <- $pc + VALUE                signed compare
  //   0xf03. 0x****                         if any $rA >= 0  $pc <- $pc + VALUE                signed compare
  //   0xf04. 0x****                         if any $rA > 0   $pc <- $pc + VALUE                signed compare
  //   0xf05. 0x****                         if any $rA <= 0  $pc <- $pc + VALUE                signed compare
  //   0xf08. 0x****                         if all $rA == 0  $pc <- $pc + VALUE
  //   0xf09. 0x****                         if all $rA != 0  $pc <- $pc + VALUE
  //   0xf0a. 0x****                         if all $rA < 0   $pc <- $pc + VALUE                signed compare
  //   0xf0b. 0x****                         if all $rA >= 0  $pc <- $pc + VALUE                signed compare
  //   0xf0c. 0x****                         if all $rA > 0   $pc <- $pc + VALUE                signed compare
  //   0xf0d. 0x****                         if all $rA <= 0  $pc <- $pc + VALUE                signed compare
  //   0xf1.. 0x****                         if any $rB == $rA   $pc <- $pc + VALUE
  //   0xf2.. 0x****                         if any $rB != $rA   $pc <- $pc + VALUE
  //   0xf3.. 0x****                         if any signed $rB < $rA  $pc <- $pc + VALUE
  //   0xf4.. 0x****                         if any signed $rB >= $rA $pc <- $pc + VALUE
  //   0xf5.. 0x****                         if any $rB < $rA    $pc <- $pc + VALUE
  //   0xf6.. 0x****                         if any $rB >= $rA   $pc <- $pc + VALUE
  //   0xf9.. 0x****                         if all $rB == $rA   $pc <- $pc + VALUE
  //   0xfa.. 0x****                         if all $rB != $rA   $pc <- $pc + VALUE
  //   0xfb.. 0x****                         if all signed $rB < $rA  $pc <- $pc + VALUE
  //   0xfc.. 0x****                         if all signed $rB >= $rA $pc <- $pc + VALUE
  //   0xfd.. 0x****                         if all $rB < $rA    $pc <- $pc + VALUE
  //   0xfe.. 0x****                         if all $rB >= $rA   $pc <- $pc + VALUE
  //   0xf.f. 0x****                         if $rA[0]  == 1 $pc <- $pc + VALUE
  //   0xf..f 0x****                         if $rB[0]  == 0 $pc <- $pc + VALUE
  //   0x1ee.                                INV[$rA]                                           invalidate cache line for address $rA
  //   0x2ee.                                $pc <- MEM32[$rA]                                  32-bit load from MEM[$rA] into $PC
  //   0x3ee.                                $tpc <- MEM32[$rA]                                 32-bit load from MEM[$rA] into $TPC
  //   0x1fe. 0x****                         INV[$rA + VALUE]                                   invalidate cache line for address $rA+FIELD_E
  //   0x2fe. 0x****                         $pc <- MEM32[$rA + VALUE]                          32-bit load from MEM[$rA+VALUE] into $PC
  //   0x3fe. 0x****                         $tpc <- MEM32[$rA + VALUE]                         32-bit load from MEM[$rA+VALUE] into $TPC
  //   0x1fef 0x**** 0x****                  INV[VALUE]                                         invalidate cache line for address FIELD_E
  //   0x2fef 0x**** 0x****                  $pc <- MEM32[VALUE]                                32-bit load from MEM[VALUE] into $PC
  //   0x3fef 0x**** 0x****                  $tpc <- MEM32[VALUE]                               32-bit load from MEM[VALUE] into $TPC
  if (brew_pattern_match(inst, ".002")) goto finish;
  if (brew_pattern_match(inst, ".003")) goto finish;
  if (inst == 0x20ef || inst == 0x30ef) goto finish;
  if (inst == 0x20fe || inst == 0x30fe) goto finish;
  if (FIELD_B(inst) <= 0xd && brew_pattern_match(inst, "f0..")) goto finish;
  if (FIELD_C(inst) >= 1 && brew_pattern_match(inst, "f...")) goto finish;
  if (brew_pattern_match(inst, "f.f.")) goto finish;
  if (brew_pattern_match(inst, "f..f")) goto finish;
  if (brew_pattern_match(inst, "1ee.")) goto finish;
  if (brew_pattern_match(inst, "2ee.")) goto finish;
  if (brew_pattern_match(inst, "3ee.")) goto finish;
  if (brew_pattern_match(inst, "1fe.")) goto finish;
  if (brew_pattern_match(inst, "2fe.")) goto finish;
  if (brew_pattern_match(inst, "3fe.")) goto finish;
  if (inst == 0x1fef || inst == 0x2fef || inst == 0x3fef) goto finish;

  // Stores
  //   0x.c**                      MEM32[$rS + tiny OFFSET] <- $rD     Store $rD in memory
  if (brew_pattern_match(inst, ".c**")) {
    // Same decoding as the tiny store in opcodes/brew-decode.c: a 7-bit
    // signed word offset in bits 7..1; bit 0 selects $fp (0) or $sp (1).
    int32_t offset = ((int32_t)((inst >> 1) & 0x7f) ^ 0x40) - 0x40;
    offset *= 4;
    uint32_t base;
    if (regcache_read_unsigned_reg(regcache, (inst & 1) == 0 ? BREW_REG_FP : BREW_REG_SP, base) != REG_VALID)
      return -1;
    base += (uint32_t) offset;
    if (record_full_arch_list_add_mem(base, 4) != 0)
      return -1;
    goto finish;
  }

  //   0x.e8.                      MEM8[$rA] <- $rD                    8-bit store to MEM[$rA] from $rD
  //   0x.e9.                      MEM16[$rA] <- $rD                   16-bit store to MEM[$rA] from $rD
  //   0x.ea.                      MEM32[$rA] <- $rD                   32-bit store to MEM[$rA] from $rD
  //   0x.eb.                      MEMSC[$rA] <- $rD                   32-bit store-conditional (exclusive store)
  if (
    brew_pattern_match(inst, ".e8.") ||
    brew_pattern_match(inst, ".e9.") ||
    brew_pattern_match(inst, ".ea.") ||
    brew_pattern_match(inst, ".eb.")
  ) {
    uint32_t base;
    if (regcache_read_unsigned_reg(regcache, FIELD_A(inst), base) != REG_VALID)
      return -1;
    int length;
    switch (FIELD_B(inst)) {
      case 0x8: length = 1; break;
      case 0x9: length = 2; break;
      case 0xa: length = 4; break;
      case 0xb: length = 4; break;
      default: gdb_assert(false);
    }
    if (record_full_arch_list_add_mem(base, length) != 0)
      return -1;
    goto finish;
  }

  //   0x.f8. 0x****               MEM8[$rA + VALUE] <- $rD            8-bit store to MEM[$rA+VALUE] from $rD
  //   0x.f9. 0x****               MEM16[$rA + VALUE] <- $rD           16-bit store to MEM[$rA+VALUE] from $rD
  //   0x.fa. 0x****               MEM32[$rA + VALUE] <- $rD           32-bit store to MEM[$rA+VALUE] from $rD
  //   0x.fb. 0x****               MEMSC[$rA + VALUE] <- $rD           32-bit store-conditional (exclusive store)
  if (
    brew_pattern_match(inst, ".f8.") ||
    brew_pattern_match(inst, ".f9.") ||
    brew_pattern_match(inst, ".fa.") ||
    brew_pattern_match(inst, ".fb.")
  ) {
    uint32_t field_e = (uint32_t)sign_extend_short_to_32((uint16_t)read_memory_unsigned_integer(addr + 2, 2, byte_order));
    uint32_t base;
    if (regcache_read_unsigned_reg(regcache, FIELD_A(inst), base) != REG_VALID)
      return -1;
    base += field_e;
    int length;
    switch (FIELD_B(inst)) {
      case 0x8: length = 1; break;
      case 0x9: length = 2; break;
      case 0xa: length = 4; break;
      case 0xb: length = 4; break;
      default: gdb_assert(false);
    }
    if (record_full_arch_list_add_mem(base, length) != 0)
      return -1;
    goto finish;
  }

  //   0x.f8f 0x**** 0x****        MEM8[VALUE] <- $rD                  8-bit store to MEM[VALUE] from $rD
  //   0x.f9f 0x**** 0x****        MEM16[VALUE] <- $rD                 16-bit store to MEM[VALUE] from $rD
  //   0x.faf 0x**** 0x****        MEM32[VALUE] <- $rD                 32-bit store to MEM[VALUE] from $rD
  //   0x.fbf 0x**** 0x****        MEMSC[VALUE] <- $rD                 32-bit store-conditional (exclusive store)
  if (
    brew_pattern_match(inst, ".f8f") ||
    brew_pattern_match(inst, ".f9f") ||
    brew_pattern_match(inst, ".faf") ||
    brew_pattern_match(inst, ".fbf")
  ) {
    // 48-bit form: VALUE is a full 32-bit absolute address.
    uint32_t field_e = (uint32_t)read_memory_unsigned_integer(addr + 2, 4, byte_order);
    int length;
    switch (FIELD_B(inst)) {
      case 0x8: length = 1; break;
      case 0x9: length = 2; break;
      case 0xa: length = 4; break;
      case 0xb: length = 4; break;
      default: gdb_assert(false);
    }
    if (record_full_arch_list_add_mem(field_e, length) != 0)
      return -1;
    goto finish;
  }

  // No special case, handle FIELD_D as target
  if (record_full_arch_list_add_reg(regcache, FIELD_D(inst)))
    return -1;

finish:
  if (record_full_arch_list_add_reg(regcache, BREW_GDB_REG_PC))
    return -1;
  return 0;
}











//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MAIN STRUCTURE SETUP
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// Where longjmp will resume: newlib's setjmp (libc/machine/brew/setjmp.S)
// stores the return address at offset 8 of the jmp_buf, whose address is
// longjmp's first argument. gdb calls this at longjmp's entry.
static bool brew_get_longjmp_target(const frame_info_ptr &frame, CORE_ADDR *pc)
{
  struct gdbarch *gdbarch = get_frame_arch(frame);
  CORE_ADDR jb_addr = get_frame_register_unsigned(frame, BREW_REG_ARG0);
  gdb_byte buf[4];

  if (target_read_memory(jb_addr + 8, buf, sizeof(buf)) != 0)
    return false;
  *pc = extract_unsigned_integer(buf, sizeof(buf), gdbarch_byte_order(gdbarch));
  return true;
}

// Allocate and initialize the brew gdbarch object.
static struct gdbarch *brew_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  // If there is already a candidate, use it.
  arches = gdbarch_list_lookup_by_info(arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  // Allocate space for the new architecture.
  gdbarch = gdbarch_alloc(&info, gdbarch_tdep_up (new brew_gdbarch_tdep));

  set_gdbarch_wchar_bit(gdbarch, 32);
  set_gdbarch_wchar_signed(gdbarch, false);

  set_gdbarch_num_regs(gdbarch, BREW_GDB_NUM_REGS);
  set_gdbarch_sp_regnum(gdbarch, BREW_REG_SP);
  set_gdbarch_pc_regnum(gdbarch, BREW_GDB_REG_PC);
  set_gdbarch_register_name(gdbarch, brew_register_name);
  for (const auto &alias : brew_register_aliases)
    user_reg_add(gdbarch, alias.name, brew_value_of_alias,
                 (const void *) (intptr_t) alias.regnum);
  set_gdbarch_register_type(gdbarch, brew_register_type);
  set_gdbarch_dwarf2_reg_to_regnum(gdbarch, brew_dwarf2_reg_to_regnum);

  set_gdbarch_return_value(gdbarch, brew_return_value);

  set_gdbarch_skip_prologue(gdbarch, brew_skip_prologue);
  set_gdbarch_inner_than(gdbarch, core_addr_lessthan);
  set_gdbarch_breakpoint_kind_from_pc(gdbarch, brew_breakpoint::kind_from_pc);
  set_gdbarch_sw_breakpoint_from_kind(gdbarch, brew_breakpoint::bp_from_kind);
  set_gdbarch_frame_align(gdbarch, brew_frame_align);

  // Frame bases and unwinders: the CFI GCC writes (.debug_frame) first,
  // then the prologue analyzer for code without it (crt0, hand-written
  // assembly). The DWARF unwinder also lets gdb rebuild tail-call frames
  // from DW_TAG_call_site and find DW_OP_entry_value values.
  dwarf2_frame_set_init_reg(gdbarch, brew_dwarf2_frame_init_reg);
  frame_base_append_sniffer(gdbarch, dwarf2_frame_base_sniffer);
  frame_base_set_default(gdbarch, &brew_frame_base);

  // Hook in ABI-specific overrides, if they have been registered.
  gdbarch_init_osabi(info, gdbarch);

  // Hook in the default unwinders.
  dwarf2_append_unwinders(gdbarch);
  frame_unwind_append_unwinder(gdbarch, &brew_frame_unwind);

  // Single stepping.
  set_gdbarch_get_next_pcs(gdbarch, brew_software_single_step);
  set_gdbarch_get_longjmp_target(gdbarch, brew_get_longjmp_target);

  // Support simple overlay manager.
  set_gdbarch_overlay_update(gdbarch, simple_overlay_update);

  // Support reverse debugging.
  set_gdbarch_process_record(gdbarch, brew_process_record);

  return gdbarch;
}

// Register this machine's init routine.
INIT_GDB_FILE (brew_tdep)
{
  gdbarch_register(bfd_arch_brew, brew_gdbarch_init);
}
