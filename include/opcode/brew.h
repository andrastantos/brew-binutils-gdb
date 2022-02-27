/* Definitions for decoding and simulating brew instructions.
   Copyright (C) 2009-2021 Free Software Foundation, Inc.
   Contributed by Andras Tantos.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */
#ifndef __OPCODE_BREW_H__
#define __OPCODE_BREW_H__

typedef enum {
  BREW_EXCEPTION_FILL,
  BREW_EXCEPTION_BREAK,
  BREW_EXCEPTION_SYSCALL,
  BREW_EXCEPTION_SWI3,
  BREW_EXCEPTION_SWI4,
  BREW_EXCEPTION_SWI5,
  BREW_EXCEPTION_SII,
  BREW_EXCEPTION_HWI,
  BREW_EXCEPTION_UNALIGNED,
  BREW_EXCEPTION_ACCESS_VIOLATION,
  BREW_EXCEPTION_F_DIV_BY_ZERO,
  BREW_EXCEPTION_F_NEG_RSQRT,
  // We have some unused exception vector indices here.
  BREW_EXCEPTION_NONE = 0xf
} brew_exception_type;

typedef enum
{
  BREW_INSN_CLS_UNKNOWN,
  BREW_INSN_CLS_EXCEPTION,
  BREW_INSN_CLS_ATOMIC,
  BREW_INSN_CLS_MOV,
  BREW_INSN_CLS_IMM,
  BREW_INSN_CLS_BIT,
  BREW_INSN_CLS_LOGIC,
  BREW_INSN_CLS_ARITH,
  BREW_INSN_CLS_SHIFT,
  BREW_INSN_CLS_MUL,
  BREW_INSN_CLS_FP,
  BREW_INSN_CLS_LD,
  BREW_INSN_CLS_ST,
  BREW_INSN_CLS_BRANCH,
  BREW_INSN_CLS_CBRANCH,
  BREW_INSN_CLS_CBRANCH0,
  BREW_INSN_CLS_CBRANCHFP,
  BREW_INSN_CLS_CBRANCH0FP,
  BREW_INSN_CLS_CBRANCHBIT,
  BREW_INSN_CLS_LINK,
  BREW_INSN_CLS_POWER,
  BREW_INSN_CLS_NOP,
  BREW_INSN_CLS_MAX
} brew_insn_classes;

typedef brew_exception_type (*brew_read_mem_ftype)(void *context, uint32_t vma, int length, uint32_t *value);
typedef brew_exception_type (*brew_write_mem_ftype)(void *context, uint32_t vma, int length, uint32_t value);
typedef void (*brew_handle_stu_ftype)(void *context, uint32_t pc, bool is_task_mode);
typedef int32_t (*brew_floor_ftype)(float);
typedef float (*brew_rsqrt_ftype)(float);

typedef struct {
  uint32_t reg[15];
  uint32_t spc;
  uint32_t tpc;
  uint32_t nspc;
  uint32_t ntpc;
  uint32_t dirty_map;
  brew_insn_classes insn_class;
  brew_exception_type insn_exception;
  bool is_task_mode;
  bool nis_task_mode;

  brew_read_mem_ftype read_mem;
  brew_write_mem_ftype write_mem;
  brew_handle_stu_ftype handle_stu;
  fprintf_ftype tracer;
  void *tracer_strm;

  // helpers for floating point operations: this avoids linking all binutils utilities against a math library
  brew_floor_ftype floor;
  brew_rsqrt_ftype rsqrt;
} brew_sim_state;

extern int brew_insn_len(uint16_t insn_code);
extern int32_t brew_unmunge_address(uint16_t field_e);
extern uint16_t brew_munge_address(int32_t offset);
extern void brew_print_insn(fprintf_ftype fpr, void *strm_or_buffer, uint16_t insn_code, uint32_t field_e);
extern void brew_sim_insn(void *context, brew_sim_state *sim_state, uint16_t insn_code, uint32_t field_e);
extern const char *brew_insn_class_name(brew_insn_classes cls);

#endif // __OPCODE_BREW_H__
