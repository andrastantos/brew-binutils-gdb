/* tc-brew.c -- Assemble code for brew
   Copyright (C) 2009-2021 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 51 Franklin Street - Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* Contributed by Andras Tantos */

#include "as.h"
#include "safe-ctype.h"
#include <stdint.h>
#include "dis-asm.h" // for fprintf_ftype
#include "opcode/brew.h"
#include "opcode/brew-abi.h"
#include "elf/brew.h"
#include "tc-brew-int.h"
#include "tc-brew-lex.h"
#include "tc-brew-parse.h"

// State that's controlled through command-line switches
static bool float_support = true; // -F and -NF
bool brew_trace = false;          // -T and -NT

static brew_parser_t parser = NULL;

#define ABORT(msg) { as_bad msg; as_abort(__FILE__, __LINE__, __PRETTY_FUNCTION__); }

const char comment_chars[]        = "#";
const char line_separator_chars[] = ";";
const char line_comment_chars[]   = "#";


/*
typedef struct
{
  const char *inst_name;
  uint16_t inst_code;
} inst_tableS;

static inst_tableS inst_table[] =
{
  { "fill",        0x0000 },
  { "swi0",        0x0000 },
  { "break",       0x0001 },
  { "swi1",        0x0001 },
  { "syscall",     0x0002 },
  { "swi2",        0x0002 },
  { "swi3",        0x0003 },
  { "swi4",        0x0004 },
  { "swi5",        0x0005 },
  { "sii",         0x0006 },
  { "swi6",        0x0006 },
  { "hwi",         0x0007 },
  { "swi7",        0x0007 },
  { "stm",         0x0008 },
  { "woi",         0x0009 },
  { "fence",       0x0010 },
  { "rwfencerw",   0x0010 },
  { "wfencerw",    0x0011 },
  { "rfencerw",    0x0012 },
  { "rwfencew",    0x0014 },
  { "wfencew",     0x0015 },
  { "rfencew",     0x0016 },
  { "rwfencer",    0x0018 },
  { "wfencer",     0x001a },
  { "rfencer",     0x001b },
  { "nop",         0x2222 }, // pseudo instruction: $r1 = $r1 | $r1, which is of course, a no-op
  { NULL,          0x0000 }
};

typedef struct
{
  const char *inst_name;
  uint16_t two_op_inst_code;
  bool swap_ops;
  uint16_t zero_inst_code;
  bool complete;

  int flags;
} branch_tableS;


static branch_tableS branch_table[] =
{
//  inst_name  two_op_inst_code   swap_ops     zero_isnt_code   complete   flags
  { "<",       0x500f,            false,       0x200f,          true,      0                      },
  { ">",       0x500f,            true,        0x010f,          false,     0                      },
  { "<=",      0x600f,            true,        0x000f,          false,     0                      },
  { ">=",      0x600f,            false,       0x100f,          true,      0                      },
  { "==",      0x100f,            false,       0x000f,          false,     0                      },
  { "!=",      0x200f,            false,       0x010f,          false,     0                      },

  { "<",       0x300f,            false,       0x020f,          false,     BREW_REG_FLAG_SIGNED   },
  { ">",       0x300f,            true,        0x040f,          false,     BREW_REG_FLAG_SIGNED   },
  { "<=",      0x400f,            true,        0x050f,          false,     BREW_REG_FLAG_SIGNED   },
  { ">=",      0x400f,            false,       0x030f,          false,     BREW_REG_FLAG_SIGNED   },
  { "==",      0x100f,            false,       0x000f,          false,     BREW_REG_FLAG_SIGNED   },
  { "!=",      0x200f,            false,       0x010f,          false,     BREW_REG_FLAG_SIGNED   },

  { "<",       0xd00f,            false,       0x0b0f,          false,     BREW_REG_FLAG_FLOAT    },
  { ">",       0xd00f,            true,        0x0d0f,          false,     BREW_REG_FLAG_FLOAT    },
  { "<=",      0xe00f,            true,        0x0e0f,          false,     BREW_REG_FLAG_FLOAT    },
  { ">=",      0xe00f,            false,       0x0c0f,          false,     BREW_REG_FLAG_FLOAT    },
  { "==",      0x100f,            false,       0x000f,          false,     BREW_REG_FLAG_FLOAT    },
  { "!=",      0x200f,            false,       0x010f,          false,     BREW_REG_FLAG_FLOAT    },

  { NULL,      0x0000,            false,       0x0000,          false,     0                      }
};

#define COMMUTATIVE              (1 << 7)
#define COMMUTATIVE_BUT_NEGATE   (1 << 8)
#define HAS_UPPER                (1 << 9)

typedef struct
{
  const char *inst_name;
  uint16_t inst_code;
  uint16_t negate_inst_code;
  int op_flags;
  int type_flags_arg2;
  int type_flags_arg1;
  int type_flags_d;
} alu_tableS;

static alu_tableS alu_table[] =
{
//  inst_name  inst_code   negate_inst_code   op_flags                      type_flags_arg2        type_flags_arg1        type_flags_d
  { "^",       0x1000,     0x1000,            COMMUTATIVE,                  0,                     0,                     0 },
  { "^",       0x1000,     0x1000,            COMMUTATIVE,                  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "|",       0x2000,     0x2000,            COMMUTATIVE,                  0,                     0,                     0 },
  { "|",       0x2000,     0x2000,            COMMUTATIVE,                  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "&",       0x3000,     0x3000,            COMMUTATIVE,                  0,                     0,                     0 },
  { "&",       0x3000,     0x3000,            COMMUTATIVE,                  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "+",       0x4000,     0x4000,            COMMUTATIVE,                  0,                     0,                     0 },
  { "+",       0x4000,     0x4000,            COMMUTATIVE,                  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "-",       0x5000,     0x4000,            COMMUTATIVE_BUT_NEGATE,       0,                     0,                     0 },
  { "-",       0x5000,     0x4000,            COMMUTATIVE_BUT_NEGATE,       BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "<<",      0x6000,     0x6000,            0,                            0,                     0,                     0 },
  { "<<",      0x6000,     0x6000,            0,                            0,                     BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { ">>",      0x7000,     0x7000,            0,                            0,                     0,                     0 },
  { ">>",      0x8000,     0x8000,            0,                            0,                     BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "*",       0x9000,     0xa000,            COMMUTATIVE,                  0,                     0,                     0 },
  { "*",       0x9000,     0xb000,            COMMUTATIVE,                  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "~&",      0xa000,     0xa000,            0,                            0,                     0,                     0 },
  { "~&",      0xa000,     0xa000,            0,                            BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { NULL,      0x0000,     0x0000,            0,                            0,                     0,                     0 },
};

typedef struct
{
  const char *inst_name;
  uint16_t inst_code;
  int op_shift;
  int type_flags_op;
  int type_flags_d;
} unary_op_tableS;

static unary_op_tableS unary_op_table[] = {
//  inst_name  inst_code   op_shift  type_flags_op          type_flags_d
  { "-",       0x0300,     4,        BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED },
  { "~",       0x0400,     4,        0,                     0 },
  { "sum",     0x0500,     4,        0,                     0 },
  { "sum",     0x0500,     4,        BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED },
  { "bsi",     0x0600,     4,        BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED },
  { "wsi",     0x0700,     4,        BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED },
  { "floor",   0x0900,     4,        BREW_REG_FLAG_FLOAT,   BREW_REG_FLAG_SIGNED },
  { "rsqrt",   0x0700,     4,        BREW_REG_FLAG_FLOAT,   BREW_REG_FLAG_FLOAT },

  { NULL,      0x0000,     0,        0,                     0 }
};

static int bit_idx_to_field_a[] = {
  0,   1,   2,   3,   4,   5,   6,   7,
  8,   9,  -1,  -1,  -1,  -1,  10,  11,
 12,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
 -1,  -1,  -1,  -1,  -1,  -1,  13,  14
};
*/
// This is really unfortunate that as doesn't provide a 'v' version of these routines
/*
static void
as_vbad (const char *format, va_list args)
{
  char buffer[2000];

  vsnprintf (buffer, sizeof (buffer), format, args);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
  as_bad(buffer);
#pragma GCC diagnostic pop
}
*/

const pseudo_typeS md_pseudo_table[] =
{
  {0, 0, 0}
};

/* TOKEN PARSERS */
/*****************/

const char FLT_CHARS[] = "rRsSfFdDxXpP";
const char EXP_CHARS[] = "eE";

/*
static bool
parse_int(const brew_lexer_tokenS *token, int *int_result)
{
  int ret_val = 0;
  bool negative = false;
  char *digit = token->start;
  size_t len = token->len;
  if (len == 0)
    return false;
  if (*digit == '-')
    {
      negative = true;
      ++digit;
      --len;
    }
  while(len > 0)
    {
      if (*digit < '0' || *digit > '9') return false;
      ret_val = ret_val * 10 + (*digit - '0');
      ++digit;
      --len;
    }
  if (negative)
    ret_val = -ret_val;
  *int_result = ret_val;
  return true;
}

// Parse an expression and then restore the input line pointer.
static char *
parse_exp_save_ilp (char *s, expressionS *op)
{
  char *save = input_line_pointer;

  input_line_pointer = s;
  expression (op);
  s = input_line_pointer;
  input_line_pointer = save;
  return s;
}

// TODO: This does more than that now:
//   - it supports (or should, rather) 16-bit offsets
//   - it should support FP16 expressions
// Parses a 32-bit expression and puts it in the last four bytes of the fragment buffer
// Returns true if an expression is found, false if not.
static bool
parse_expression(const char *token, bool support_float, bool is_short, bool is_addr, bool is_negative)
{
  LITTLENUM_TYPE float_store[4]; // We really shouldn't store more than 4 bytes, but we can only test for that after the call returns. So, oversize the buffer to make sure we won't overflow
  char float_as_char[4];
  char *ret_val;

  // GAS also doesn't support any floating point expressions. We need that for float immediates, so let's test for them
  // NOTE: this still doesn't support floating point expressions, such as 1.3+2.6,
  //       but that's not supported in GAS anywhere, not even in .float
  if (support_float)
    {
      ret_val = atof_ieee((char*)token, 'f', float_store);
      if (ret_val != NULL && ret_val > token)
        {
          DEBUG("float parser return %s with 0x%04x 0x%04x", ret_val, float_store[0], float_store[1]);
          int prec = 2;
          int float_size = 4;
          char *litP = float_as_char;
          LITTLENUM_TYPE *wordP;
          for (wordP = float_store + prec; prec --;)
            {
              md_number_to_chars (litP, (valueT) (* -- wordP), sizeof (LITTLENUM_TYPE));
              litP += sizeof (LITTLENUM_TYPE);
            }

          if (is_short || is_addr)
            {
              return false; // Float constant are always 32-bits.
            }
          // This is a floating point constant we could successfully parse
          field_e_frag = frag_more(float_size);
          memcpy(field_e_frag, float_as_char, float_size);
          return true;
        }
    }

  expressionS arg;
  char *end_expr;
  size_t field_e_size = is_short ? 2 : 4;
  int reloc_type = is_short ? is_addr ? BFD_RELOC_BREW_PCREL16 : is_negative ? BFD_RELOC_BREW_NEG16 : BFD_RELOC_16 : is_negative ? BFD_RELOC_BREW_NEG32 : BFD_RELOC_32;
  bool pc_rel = is_short ? is_addr ? true : false : false;
  end_expr = parse_exp_save_ilp ((char*)token, &arg);
  if (*end_expr != 0)
  {
    return false;
  }
  field_e_frag = frag_more(field_e_size);
  fix_new_exp(
    frag_now,
    (field_e_frag - frag_now->fr_literal),
    field_e_size,
    &arg,
    pc_rel,
    reloc_type);
  return true;
}
*/


// NOTE:
//   When get a 'fragment' using frag_more,
//   we are committing that many bytes into
//   the object file.
//   When we call dwrf2_emit_insn, we only
//   emit the line number info (if any) for
//   the debug info (as the prefix dwarf2
//   suggests.

#define RETURN(frag, inst_code, field_e_size) { \
  md_number_to_chars(frag, inst_code, 2); \
  dwarf2_emit_insn(field_e_size+2); \
  return true; \
}





static bool action_insn(void *context ATTRIBUTE_UNUSED, brew_parser_tokenS *tokens)
{
  DEBUG_BEGIN
  brew_dump_parsed_tokens(stderr, tokens);
  DEBUG_END

  char *frag = frag_more(2); // The frag pointer that will (eventually) contain the instruction code

  gas_assert(tokens[0].parser_token == T_INSN);
  gas_assert(tokens[1].parser_token == 0);
  gas_assert(tokens[0].first_lexer_token == tokens[0].last_lexer_token);
  gas_assert(tokens[0].first_lexer_token->type == T_INSN);

  RETURN(frag, tokens[0].first_lexer_token->sub_type, 0);

  return true;
}

static bool INVALID_PATTERN(void *context ATTRIBUTE_UNUSED, brew_parser_tokenS *tokens)
{
  DEBUG_BEGIN
  brew_dump_parsed_tokens(stderr, tokens);
  DEBUG_END
  return false;
}

static const brew_parser_tok_type_t raw_insn[] = {
  PATTERN(T_INSN),                                                                                      ACTION(action_insn),

  PATTERN(T_PC, T_ASSIGN, T_REG),                                                                       ACTION(INVALID_PATTERN),
  PATTERN(T_PC, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_RBRACKET),                                        ACTION(INVALID_PATTERN),
  PATTERN(T_PC, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_COMMA, ~T_RBRACKET, T_RBRACKET),                  ACTION(INVALID_PATTERN),
  PATTERN(T_PC, T_ASSIGN, T_MEM, T_LBRACKET, ~T_COMMA, T_COMMA, T_REG, T_RBRACKET),                     ACTION(INVALID_PATTERN),
  PATTERN(T_PC, T_ASSIGN, T_MEM, T_LBRACKET, ~T_RBRACKET, T_RBRACKET),                                  ACTION(INVALID_PATTERN),
  PATTERN(T_PC, T_ASSIGN, ~T_NULL),                                                                     ACTION(INVALID_PATTERN),
  PATTERN(T_PC, T_ASSIGN, T_SHORT, ~T_NULL),                                                            ACTION(INVALID_PATTERN),

  PATTERN(T_TYPE, T_REG, T_ASSIGN, T_REG),                                                              ACTION(INVALID_PATTERN),
  PATTERN(T_TYPE, T_REG, T_ASSIGN, T_TYPENAME),                                                         ACTION(INVALID_PATTERN),

  PATTERN(T_REG, T_ASSIGN, T_PC, T_PLUS, ~T_NULL),                                                      ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_PC),                                                                       ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_HAT, T_REG),                                                        ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_BAR, T_REG),                                                        ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_LSHIFT, T_REG),                                                     ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_RSHIFT, T_REG),                                                     ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_PLUS, T_REG),                                                       ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_STAR, T_REG),                                                       ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_AND, T_REG),                                                        ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_HAT, ~T_NULL),                                                      ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_BAR, ~T_NULL),                                                      ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_LSHIFT, ~T_NULL),                                                   ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_RSHIFT, ~T_NULL),                                                   ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_PLUS, ~T_NULL),                                                     ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_STAR, ~T_NULL),                                                     ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_AND, ~T_NULL),                                                      ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_MINUS, T_REG),                                                      ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_MINUS, T_ONE),                                                      ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_MINUS, ~T_NULL),                                                    ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_CMP, T_ZERO),                                                       ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_CMP, T_REG),                                                        ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG),                                                                      ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_TILDE, T_REG, T_AND, T_REG),                                               ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_PREFIX_OP, T_REG),                                                         ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_MINUS, T_REG),                                                             ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_TILDE, T_REG),                                                             ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_INTERPOLATE, T_REG, T_COMMA, T_REG),                                       ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_FULL, T_REG, T_STAR, T_REG, T_LSHIFT, ~T_NULL),                            ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SWIZZLE, T_REG, T_COMMA, ~T_NULL),                                         ACTION(INVALID_PATTERN),

  PATTERN(T_REG, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_RBRACKET),                                       ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_PLUS, T_TINY, ~T_RBRACKET, T_RBRACKET),          ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_PLUS, ~T_RBRACKET, T_RBRACKET),                  ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_MEM, T_LBRACKET, ~T_RBRACKET, T_RBRACKET),                                 ACTION(INVALID_PATTERN),

  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_HAT, ~T_NULL),                                             ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_BAR, ~T_NULL),                                             ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_LSHIFT, ~T_NULL),                                          ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_RSHIFT, ~T_NULL),                                          ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_PLUS, ~T_NULL),                                            ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_STAR, ~T_NULL),                                            ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_AND, ~T_NULL),                                             ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_MINUS, ~T_NULL),                                           ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, ~T_REG, T_REG),                                                     ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, ~T_NULL),                                                           ACTION(INVALID_PATTERN),

  PATTERN(T_REG, T_ASSIGN, ~T_REG, T_REG),                                                              ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_ONE, T_DIV, T_REG),                                                        ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, ~T_NULL),                                                                    ACTION(INVALID_PATTERN),

  PATTERN(T_MEM, T_LBRACKET, T_REG, T_RBRACKET, T_ASSIGN, T_REG),                                       ACTION(INVALID_PATTERN),
  PATTERN(T_MEM, T_LBRACKET, T_REG, T_PLUS, T_TINY, ~T_RBRACKET, T_RBRACKET, T_ASSIGN, T_REG),          ACTION(INVALID_PATTERN),
  PATTERN(T_MEM, T_LBRACKET, T_REG, T_PLUS, ~T_RBRACKET, T_RBRACKET, T_ASSIGN, T_REG),                  ACTION(INVALID_PATTERN),
  PATTERN(T_MEM, T_LBRACKET, ~T_RBRACKET, T_RBRACKET, T_ASSIGN, T_REG),                                 ACTION(INVALID_PATTERN),

  PATTERN(T_IF, T_REG, T_CMP, T_ZERO, T_PC, T_ASSIGN, ~T_NULL),                                         ACTION(INVALID_PATTERN),
  PATTERN(T_IF, T_REG, T_CMP, T_REG, T_PC, T_ASSIGN, ~T_NULL),                                          ACTION(INVALID_PATTERN),
  PATTERN(T_IF, T_REG, T_LBRACKET, ~T_RBRACKET, T_RBRACKET, T_CMP, T_ONE, T_PC, T_ASSIGN, ~T_NULL),     ACTION(INVALID_PATTERN),
  PATTERN(T_IF, T_REG, T_LBRACKET, ~T_RBRACKET, T_RBRACKET, T_CMP, T_ZERO, T_PC, T_ASSIGN, ~T_NULL),    ACTION(INVALID_PATTERN),
};

// This is the guts of the machine-dependent assembler.  STR points to
// a machine dependent instruction.
// This function is supposed to emit the frags/bytes it assembles to.
void
md_assemble (char *str)
{
  brew_lexer_tokenS *token_strm;

  DEBUG("md_assemble: %s", str);
  token_strm = brew_tokenize(str);
  DEBUG_BEGIN
    brew_lexer_tokenS *t = token_strm;
    while (t->type != T_NULL)
      {
        fprintf(stderr, "    "); brew_dump_lexer_token(stderr, t); fprintf(stderr, "\n");
        t++;
      }
  DEBUG_END
  if (!brew_parse(parser, token_strm, NULL))
    {
      as_bad(_("Unrecognized instruction"));
      ignore_rest_of_line();
      return;
    }
    // PARSING EXPRESSION INTO FIELD_E: if (parse_expression(tok_start, false, false, true, false))
}

// Turn a string in input_line_pointer into a floating point constant
// of type type, and store the appropriate bytes in *litP.  The number
// of bytes emitted into litP is stored in *sizeP .  An error message is
// returned, or NULL on OK.

const char *
md_atof(int type, char *litP, int *sizeP)
{
  switch (type)
    {
    case 'f':
    case 'F':
    case 's':
    case 'S':
      break;
    default:
      *sizeP = 0;
      return _("unsupported floating point format");
    }
  return ieee_md_atof(type, litP, sizeP, target_big_endian);
}


enum options
{
  OPTION_F = OPTION_MD_BASE,
  OPTION_NF,
  OPTION_T,
  OPTION_NT,
};
struct option md_longopts[] =
{
  { "F",           no_argument, NULL, OPTION_F},
  { "NF",          no_argument, NULL, OPTION_NF},
  { "T",           no_argument, NULL, OPTION_T},
  { "NT",          no_argument, NULL, OPTION_NT},
  { NULL,          no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof(md_longopts);

const char *md_shortopts = "";

int
md_parse_option (int c ATTRIBUTE_UNUSED, const char *arg ATTRIBUTE_UNUSED)
{
  switch (c)
    {
    case OPTION_F:
      float_support = true;
      break;
    case OPTION_NF:
      float_support = false;
      break;
    case OPTION_T:
      brew_trace = true;
      break;
    case OPTION_NT:
      brew_trace = false;
      break;
    default:
      return 0;
    }

  return 1;
}

void
md_show_usage(FILE *stream)
{
  fprintf (stream, _("\
  -F                      enable floating point instructions (default)\n\
  -NF                     disable floating point instructions\n"));
}

// Apply a fixup to the object file.
void
md_apply_fix(
  fixS *fixP,
  valueT * valP,
  segT seg ATTRIBUTE_UNUSED
) {
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  long val = *valP;
  long max, min;

  max = min = 0;
  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_32:
      buf[3] = val >> 24;
      buf[2] = val >> 16;
      buf[1] = val >> 8;
      buf[0] = val >> 0;
      buf += 4;
      break;
    case BFD_RELOC_BREW_NEG32:
      buf[3] = -val >> 24;
      buf[2] = -val >> 16;
      buf[1] = -val >> 8;
      buf[0] = -val >> 0;
      buf += 4;
      break;
    case BFD_RELOC_16:
      max = INT16_MAX;
      min = INT16_MIN;
      buf[1] = val >> 8;
      buf[0] = val >> 0;
      buf += 2;
      break;
    case BFD_RELOC_BREW_NEG16:
      max = INT16_MAX;
      min = INT16_MIN;
      buf[1] = -val >> 8;
      buf[0] = -val >> 0;
      buf += 2;
      break;
    case BFD_RELOC_BREW_PCREL16:
      max = INT16_MAX;
      min = INT16_MIN;
      // This is a relative 17-bit offset, the LSB being 0 and not stored
      if ((val & 1) != 0)
        as_bad_where (fixP->fx_file, fixP->fx_line, _("$pc offset must be even"));
      buf[1] = val >> 9;
      buf[0] = val >> 1;
      buf += 2;
      break;
    default:
      as_fatal("Relocation type %d is not supported on brew. Do you have an undefined symbol?", fixP->fx_r_type);
    }

  if (max != 0 && (val < min || val > max))
    as_bad_where (fixP->fx_file, fixP->fx_line, _("offset out of range"));

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

// Put number into target byte order.
void
md_number_to_chars(char * ptr, valueT use, int nbytes)
{
  number_to_chars_littleendian(ptr, use, nbytes);
}

// Generate a machine-dependent relocation.
arelent *
tc_gen_reloc(asection *section ATTRIBUTE_UNUSED, fixS *fixP)
{
  arelent *reloc;

  reloc = XNEW (arelent);
  reloc->sym_ptr_ptr = XNEW (asymbol *);
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  reloc->address = fixP->fx_frag->fr_address + fixP->fx_where;

  reloc->addend = fixP->fx_offset;
  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixP->fx_r_type);

  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where(fixP->fx_file, fixP->fx_line,
                   /* xgettext:c-format.  */
                   _("reloc %d not supported by object file format"),
                   (int) fixP->fx_r_type);
      xfree (reloc->sym_ptr_ptr);
      xfree (reloc);

      return NULL;
    }

  return reloc;
}

// Decide from what point a pc-relative relocation is relative to,
// relative to the pc-relative fixup.  Er, relatively speaking.
long
md_pcrel_from(fixS *fixP)
{
  gas_assert(fixP->fx_r_type == BFD_RELOC_BREW_PCREL16);

  fprintf(stderr, "FIXME: %s:%d do we need the -2 here?", __FILE__, __LINE__);
  valueT addr = fixP->fx_where + fixP->fx_frag->fr_address - 2;
  return addr;
}

void
md_operand(expressionS *op ATTRIBUTE_UNUSED)
{}

static void
md_done(void);

// This function is called once, at assembler startup time.  It can
// do all sorts of local initialization that needs to happen.
// NOTE: at least it's *supposed* to be called only once, but
//       it's unclear if that's really true. We are going to
//       protect against multiple calls just in case.
void
md_begin(void)
{
  if (parser == NULL)
    parser = brew_parser_init(raw_insn, ARRAY_SIZE(raw_insn));
  atexit(md_done);
  bfd_set_arch_mach(stdoutput, TARGET_ARCH, 0);
  record_alignment(text_section, 1);
  record_alignment(data_section, 2);
  record_alignment(bss_section, 2);
  DEBUG_BEGIN
    brew_dump_parser(stderr, parser);
  DEBUG_END
}

static void
md_done(void)
{
  brew_parser_done(parser);
}
