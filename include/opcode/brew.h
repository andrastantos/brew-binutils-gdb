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

// forward-declare our struct. This is going to be only valid for simulation contexts, but this header is used everywhere.
struct _sim_cpu;

typedef enum {
  BREW_REG_TYPE_INT32        =  0,
  BREW_REG_TYPE_FP32         =  1,
  BREW_REG_TYPE_INT16x2      =  2,
  BREW_REG_TYPE_INT8x4       =  3,
  BREW_REG_TYPE_UINT16x2S    =  4,
  BREW_REG_TYPE_SINT16x2S    =  5,
  BREW_REG_TYPE_UINT8x4S     =  6,
  BREW_REG_TYPE_SINT8x4S     =  7,
  BREW_REG_TYPE_FP16x2       =  8,
  BREW_REG_TYPE_MASK         =  0xf
} brew_reg_types;

typedef enum {
  BREW_EXCEPTION_NONE =                  0x0000,
  BREW_EXCEPTION_RESET =                 0x0000, // Hardware reset
  BREW_EXCEPTION_HWI =                   0x0010, // Hardware interrupt (only in TASK mode)
  BREW_EXCEPTION_FILL =                  0x0020, // SWI 0 instruction executed (FILL)
  BREW_EXCEPTION_SWI_0 =                 0x0020, // SWI 0 instruction executed (FILL)
  BREW_EXCEPTION_BREAK =                 0x0021, // SWI 1 instruction executed (BREAK)
  BREW_EXCEPTION_SWI_1 =                 0x0021, // SWI 1 instruction executed (BREAK)
  BREW_EXCEPTION_SYSCALL =               0x0022, // SWI 2 instruction executed (SYSCALL)
  BREW_EXCEPTION_SWI_2 =                 0x0022, // SWI 2 instruction executed (SYSCALL)
  BREW_EXCEPTION_SWI_3 =                 0x0023, // SWI 3 instruction executed
  BREW_EXCEPTION_SWI_4 =                 0x0024, // SWI 4 instruction executed
  BREW_EXCEPTION_SWI_5 =                 0x0025, // SWI 5 instruction executed
  BREW_EXCEPTION_SWI_6 =                 0x0026, // SWI 6 instruction executed
  BREW_EXCEPTION_SWI_7 =                 0x0027, // SWI 7 instruction executed
  BREW_EXCEPTION_UNKNOWN_INST =          0x0030, // Undefined instruction
  BREW_EXCEPTION_TYPE =                  0x0031, // Type error in instruction operands
  BREW_EXCEPTION_UNALIGNED =             0x0032, // Unaligned memory access
  ESPRESSO_EXCEPTION_INST_AV =           0x0040, // Instruction fetch AV
  ESPRESSO_EXCEPTION_MEM_AV =            0x0041, // Memory access AV
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
  BREW_INSN_CLS_CBRANCHBIT,
  BREW_INSN_CLS_TYPE,
  BREW_INSN_CLS_VECTOR,
  BREW_INSN_CLS_LINK,
  BREW_INSN_CLS_POWER,
  BREW_INSN_CLS_NOP,
  BREW_INSN_CLS_MAX
} brew_insn_classes;


typedef struct {
  uint32_t val;
  brew_reg_types type;
} brew_typed_reg;


#if __HAVE_FLOAT16 == 0
#define _Float16 uint16_t
#endif

// forward-declare a struct, that only makes sense in sim...
struct sim_state;

typedef void (*brew_free_model_info_ftype)(struct _sim_cpu *scpu);
typedef void (*brew_read_mem_ftype)(struct _sim_cpu *scpu, uint32_t vma, int length, uint32_t *value);
typedef uint32_t (*brew_read_inst_ftype)(struct _sim_cpu *scpu, uint32_t vma, int length);
typedef void (*brew_write_mem_ftype)(struct _sim_cpu *scpu, uint32_t vma, int length, uint32_t value);
typedef uint32_t (*brew_read_csr_ftype)(struct _sim_cpu *scpu, uint16_t csr_addr);
typedef void (*brew_write_csr_ftype)(struct _sim_cpu *scpu, uint16_t csr_addr, uint32_t value);
typedef void (*brew_handle_exception_ftype)(struct _sim_cpu *scpu);
typedef void (*brew_reset_cpu_ftype)(struct _sim_cpu *scpu, bool is_user_mode_sim);
typedef void (*brew_setup_sim_ftype)(struct sim_state *sd, struct _sim_cpu *scpu);
typedef void (*brew_handle_interrupt_ftype)(struct _sim_cpu *scpu);


typedef _Float16 (*brew_rsqrt_fp16_ftype)(_Float16);
typedef float (*brew_rsqrt_ftype)(float);

struct brew_model_functions {
  // Function pointers to virtualized (model-specific) implementations
  brew_setup_sim_ftype setup_sim;
  brew_read_mem_ftype read_mem;
  brew_read_inst_ftype read_inst;
  brew_write_mem_ftype write_mem;
  brew_read_csr_ftype read_csr;
  brew_write_csr_ftype write_csr;
  brew_handle_exception_ftype handle_exception;
  brew_reset_cpu_ftype reset_cpu;
  brew_free_model_info_ftype free_model_info; // pointer to function to free model-specific data
  brew_handle_interrupt_ftype handle_interrupt;

  // helpers for floating point operations: this avoids linking all binutils utilities against a math library
  brew_rsqrt_fp16_ftype rsqrt_fp16;
  brew_rsqrt_ftype rsqrt;
};

typedef struct {
  brew_typed_reg reg[15];
  uint32_t spc;
  uint32_t tpc;
  uint32_t nspc;
  uint32_t ntpc;
  uint32_t dirty_map;
  brew_insn_classes insn_class;
  brew_exception_type ecause; // ecause for the current instruction. Gets reset for every instruction
  brew_exception_type csr_ecause; // csr holding ecause. This is cleared on read
  uint32_t csr_eaddr;
  bool is_task_mode;
  bool nis_task_mode;

  fprintf_ftype tracer;
  void *tracer_strm;

  struct brew_model_functions model_functions;
} brew_sim_state;


extern int brew_insn_len(uint16_t insn_code);
extern int32_t brew_unmunge_address(uint16_t field_e);
extern uint16_t brew_munge_address(int32_t offset);
extern void brew_print_insn(fprintf_ftype fpr, void *strm_or_buffer, uint16_t insn_code, uint32_t field_e);
extern void brew_sim_insn(struct _sim_cpu *scpu, brew_sim_state *sim_state, uint16_t insn_code, uint32_t field_e);
extern const char *brew_insn_class_name(brew_insn_classes cls);
extern const char *brew_reg_type_name(brew_reg_types reg_type);

#endif // __OPCODE_BREW_H__
