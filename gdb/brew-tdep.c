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

#include "defs.h"
#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "value.h"
#include "inferior.h"
#include "symfile.h"
#include "objfiles.h"
#include "osabi.h"
#include "language.h"
#include "arch-utils.h"
#include "regcache.h"
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

// Virtual registers. Their definition is in sim/brew/gdb-if.h
static const char * const brew_virtual_reg_names[] = {
  "$tpc", "$spc", "$pc", "task mode"
};

// gdbarch 'method' implementations. The follow the name in gdbarch with the 'brew_' prefix

static const char *brew_register_name(struct gdbarch *gdbarch, int reg_nr)
{
  // TODO: what to do with $pc/$tpc/$spc????
  if (reg_nr < 0)
    return NULL;
  if (reg_nr < 15) // normal registers
    return brew_reg_names[reg_nr];
  reg_nr -= 15;
  if (reg_nr >= sizeof(brew_virtual_reg_names)/sizeof(brew_virtual_reg_names[0]))
    return NULL;
  return brew_virtual_reg_names[reg_nr];
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
  int len = TYPE_LENGTH(type);

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

static inline int32_t sign_extend_32(uint16_t value) {
  if ((value & 0x8000) != 0) return value | 0xffff0000;
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
static bool brew_analyze_prolog(
  CORE_ADDR start_addr,
  CORE_ADDR cur_pc,
  CORE_ADDR cur_fp,
  struct brew_frame_info *frame_info,
  struct frame_info *this_frame,
  struct gdbarch *gdbarch
) {
  enum bfd_endian byte_order = gdbarch_byte_order(gdbarch);
  CORE_ADDR inst_addr = start_addr;
  uint16_t inst;
  int32_t offset;
  bool sp_is_old = true;
  bool fp_is_old = true;
  bool lr_is_saved = false;

  // We are looking for something like this:
  //
  // 8000224:  ff cc               mem32[$sp - tiny 4 (0xfffffffc)] <- $fp
  // 8000226:  fd ec               mem32[$sp - tiny 8 (0xfffffff8)] <- $lr
  // 8000228:  fb 8c               mem32[$sp - tiny 12 (0xfffffff4)] <- $r8
  // 800022a:  f9 9c               mem32[$sp - tiny 16 (0xfffffff0)] <- $r9
  // 800022c:  dd c2               $fp <- $sp
  // 800022e:  fd d4 e0 ff         $sp <- short -32 (0xffffffe0) + $sp

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
      frame_info->saved_regs[reg_idx] = offset;
      if (cur_pc > start_addr && reg_idx == BREW_REG_LINK)
        lr_is_saved = true;
      inst_addr += 2;
    } else {
      break;
    }
  }

  // This must be a $fp <- $sp instruction
  if (!read_memory_uint(inst_addr, byte_order, inst))
    return false;
  if (inst != 0xc2dd)
    return false;
  inst_addr += 2;

  if (cur_pc >= inst_addr)
    fp_is_old = false;

  // This is the $sp adjustment. It could be a short or a long decrement
  if (!read_memory_uint(inst_addr, byte_order, inst))
    return false;
  if (inst == 0xd4fd) {
    // short decrement
    inst_addr += 2;
    uint16_t short_offset;
    if (!read_memory_uint(inst_addr, byte_order, short_offset))
      return false;
    // Sign-extend
    offset = sign_extend_32(short_offset);
    inst_addr += 2;
  } else if (inst == 0xd4df) {
    // long decrement
    inst_addr += 2;
    if (!read_memory_uint(inst_addr, byte_order, offset))
      return false;
    inst_addr += 4;
  } else
    return false;

  if (cur_pc >= inst_addr)
    sp_is_old = false;

  // If we've gotten this far, we managed to parse the prolog.
  if (this_frame != NULL) {
    if (sp_is_old) {
      frame_info->prev_sp = get_frame_register_unsigned(this_frame, BREW_REG_SP);
    } else {
      frame_info->prev_sp = cur_fp;
    }
    if (fp_is_old) {
      frame_info->prev_fp = cur_fp;
    } else {
      int32_t fp_ofs = frame_info->saved_regs[BREW_REG_FP];
      uint32_t fp = cur_fp;
      uint32_t old_fp;
      if (!read_memory_uint(fp+fp_ofs, byte_order, old_fp))
        return false;
      frame_info->prev_fp = old_fp;
    }
    if (lr_is_saved) {
      int32_t fp_ofs = frame_info->saved_regs[BREW_REG_LINK];
      uint32_t fp = cur_fp;
      uint32_t old_fp;
      if (!read_memory_uint(fp+fp_ofs, byte_order, old_fp))
        return false;
      frame_info->prev_fp = old_fp;
    } else {
      frame_info->return_addr = get_frame_register_unsigned(this_frame, BREW_REG_LINK);
    }
  } else {
    frame_info->prev_sp = 0;
    frame_info->prev_fp = 0;
  }

  frame_info->prolog_end = inst_addr;
  frame_info->framesize = -offset;
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
  if (post_prologue_pc != 0)
    {
      return std::max(pc, post_prologue_pc);
    }

  // Try to find line number info for the first line in the source code as a marker
  // for the end of prologue. That is, except for assembly, which has no such concept.
  struct symtab_and_line sal;
  struct symbol *sym;
  // Find the function.
  sym = lookup_symbol(func_name, NULL, VAR_DOMAIN, NULL).symbol;
  if (sym && sym->language() == language_asm)
    {
      return func_addr;
    }
  sal = find_pc_line(func_addr, 0);
  if (sal.end && sal.end < func_end)
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
    uint32_t field_e = (uint32_t)sign_extend_32((uint16_t)read_memory_unsigned_integer(cur_pc + 2, 2, byte_order));
    next_pcs.push_back(field_e);
    return next_pcs;
  }
  //0x30fe 0x**** $tpc <- short VALUE         Load sign-extended value into $tpc
  if (brew_pattern_match(inst, "30fe")) {
    uint32_t field_e = (uint32_t)sign_extend_32((uint16_t)read_memory_unsigned_integer(cur_pc + 2, 2, byte_order));
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
    uint32_t field_e = (uint32_t)sign_extend_32((uint16_t)read_memory_unsigned_integer(cur_pc + 2, 2, byte_order));
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
    uint32_t field_e = (uint32_t)sign_extend_32((uint16_t)read_memory_unsigned_integer(cur_pc + 2, 2, byte_order));
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
  int len = TYPE_LENGTH(type); // NOTE: must be signed: we're decrementing by 4 no matter how much we need, so len might turn negative.
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
  if (TYPE_LENGTH(valtype) > 16)
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

  frame_info = FRAME_OBSTACK_ZALLOC(struct brew_frame_info);

  frame_info->valid = false;

  return frame_info;
}

// Populate a brew_frame_info object for this_frame.
static struct brew_frame_info *brew_frame_info_fill(struct frame_info *this_frame, void **this_cache)
{
  struct brew_frame_info *frame_info;

  if (*this_cache != NULL)
    return (struct brew_frame_info*) *this_cache;

  frame_info = brew_alloc_frame_info();
  *this_cache = frame_info;

  ULONGEST fp = get_frame_register_unsigned(this_frame, BREW_REG_FP);

  // This marks the outermost frame.
  if (fp == 0) {
    // printf(">>>>>>>>>>>>>>>>> FOUND OUTERMOST FRAME!!!!!!\n");
    return frame_info;
  }

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
  struct frame_info *this_frame,
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
  struct frame_info *this_frame,
  void **this_prologue_cache,
  int regnum
) {
  struct brew_frame_info *frame_info = brew_frame_info_fill(this_frame, this_prologue_cache);

  gdb_assert(regnum >= 0);
  gdb_assert(frame_info->valid);

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
  // We don't have a value for this register. Most platforms at this point return the current register value
  // but that seems to be very optimistic: the current value is most likely not what the register held in the
  // calling frame. So, I think we should return `frame_unwind_got_optimized` instead.
  //return frame_unwind_got_register(this_frame, regnum, regnum);
  return frame_unwind_got_optimized(this_frame, regnum);
}

// Return the base address of this_frame.
static CORE_ADDR brew_frame_base_address (struct frame_info *this_frame, void **this_cache)
{
  struct brew_frame_info *frame_info = brew_frame_info_fill(this_frame, this_cache);
  gdb_assert(frame_info->valid);
  return frame_info->prev_sp;
}

static const struct frame_unwind brew_frame_unwind = {
  "brew prologue",
  NORMAL_FRAME,
  default_frame_unwind_stop_reason,
  brew_frame_this_id,
  brew_frame_prev_register,
  NULL,
  default_frame_sniffer
};

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
    gdb_printf (gdb_stdlog, "Process record: brew_process_record addr = 0x%s\n", paddress(target_gdbarch (), addr));

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
    int32_t offset = (int32_t)(inst & 0xfe) << 16 >> 15;
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
    uint32_t field_e = (uint32_t)sign_extend_32((uint16_t)read_memory_unsigned_integer(addr + 2, 2, byte_order));
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
    uint32_t field_e = (uint32_t)sign_extend_32((uint16_t)read_memory_unsigned_integer(addr + 2, 2, byte_order));
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
  if (record_full_arch_list_add_end())
    return -1;
  return 0;
}











//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MAIN STRUCTURE SETUP
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// Allocate and initialize the brew gdbarch object.
static struct gdbarch *brew_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  // If there is already a candidate, use it.
  arches = gdbarch_list_lookup_by_info(arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  // Allocate space for the new architecture.
  brew_gdbarch_tdep *tdep = new brew_gdbarch_tdep;
  gdbarch = gdbarch_alloc(&info, tdep);

  set_gdbarch_wchar_bit(gdbarch, 32);
  set_gdbarch_wchar_signed(gdbarch, 0);

  set_gdbarch_num_regs(gdbarch, BREW_GDB_NUM_REGS);
  set_gdbarch_sp_regnum(gdbarch, BREW_REG_SP);
  set_gdbarch_pc_regnum(gdbarch, BREW_GDB_REG_PC);
  set_gdbarch_register_name(gdbarch, brew_register_name);
  set_gdbarch_register_type(gdbarch, brew_register_type);

  set_gdbarch_return_value(gdbarch, brew_return_value);

  set_gdbarch_skip_prologue(gdbarch, brew_skip_prologue);
  set_gdbarch_inner_than(gdbarch, core_addr_lessthan);
  set_gdbarch_breakpoint_kind_from_pc(gdbarch, brew_breakpoint::kind_from_pc);
  set_gdbarch_sw_breakpoint_from_kind(gdbarch, brew_breakpoint::bp_from_kind);
  set_gdbarch_frame_align(gdbarch, brew_frame_align);

  frame_base_set_default(gdbarch, &brew_frame_base);

  // Hook in ABI-specific overrides, if they have been registered.
  gdbarch_init_osabi(info, gdbarch);

  // Hook in the default unwinders.
  frame_unwind_append_unwinder(gdbarch, &brew_frame_unwind);

  // Single stepping.
  set_gdbarch_software_single_step(gdbarch, brew_software_single_step);

  // Support simple overlay manager.
  set_gdbarch_overlay_update(gdbarch, simple_overlay_update);

  // Support reverse debugging.
  set_gdbarch_process_record(gdbarch, brew_process_record);

  return gdbarch;
}

// Register this machine's init routine.
// NOTE: It is *very* important that the function is declared on a
//       new line, with a space between it's name and the parens.
//       The `make-init-c` script depends on this, and without it,
//       the generated `init.c` will not contain a reference to this
//       function.
void _initialize_brew_tdep();
void
_initialize_brew_tdep ()
{
  register_gdbarch_init(bfd_arch_brew, brew_gdbarch_init);
}
