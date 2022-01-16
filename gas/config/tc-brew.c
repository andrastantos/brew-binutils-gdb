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

/* TODO: right now this is not changed from the moxie baseline at all. It should be!!!! */

#include "as.h"
#include "safe-ctype.h"
#include <stdint.h>
#include "opcode/brew.h"
#include "elf/brew.h"

#define DEBUG(...) { if (trace) {fprintf (stderr, __VA_ARGS__); fprintf(stderr, "\n");} }
#define ABORT(msg) { as_bad msg; as_abort(__FILE__, __LINE__, __PRETTY_FUNCTION__); }

const char comment_chars[]        = "#";
const char line_separator_chars[] = ";";
const char line_comment_chars[]   = "#";

/* Global variables */
static char *tok_start; /* Points to the beginning of the current token to be parsed */
static char *tok_end; /* Points to the zero-termination character for the current token to be parsed */
static char tok_end_holder; /* Hold the original character after the current token (that was replaced by zero-termination) */
static char *inst_code_frag; /* The frag pointer that will (eventually) contain the instruction code */
static char *field_e_frag; /* The frag pointer that optionally gets the 32-bit value from an expression for 48-bit instructions */

typedef struct
{
  const char *inst_name;
  uint16_t inst_code;
} inst_tableS;

static inst_tableS inst_table[] =
{
  { "fill",    0x0000 },
  { "break",   0x0110 },
  { "syscall", 0x0220 },
  { "stu",     0x0330 },
  { "sii",     0x0440 },
  { "fence",   0x0dd0 },
  { "wfence",  0x0ee0 },
  { "woi",     0x1000 },
  { "nop",     0x1111 }, /* pseudo instruction: $r1 = $r1 | $r1, which is of course, a no-op */
  { NULL,      0x0000 }
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
/*  inst_name  two_op_inst_code   swap_ops     zero_isnt_code   complete   flags */
  { "<",       0x500f,            false,       0x200f,          true,      0                      },
  { ">",       0x500f,            true,        0x001f,          false,     0                      },
  { "<=",      0x600f,            true,        0x000f,          false,     0                      },
  { ">=",      0x600f,            false,       0x100f,          true,      0                      },
  { "==",      0x100f,            false,       0x000f,          false,     0                      },
  { "!=",      0x200f,            false,       0x001f,          false,     0                      },

  { "<",       0x300f,            false,       0x002f,          false,     BREW_REG_FLAG_SIGNED   },
  { ">",       0x300f,            true,        0x004f,          false,     BREW_REG_FLAG_SIGNED   },
  { "<=",      0x400f,            true,        0x005f,          false,     BREW_REG_FLAG_SIGNED   },
  { ">=",      0x400f,            false,       0x003f,          false,     BREW_REG_FLAG_SIGNED   },
  { "==",      0x100f,            false,       0x000f,          false,     BREW_REG_FLAG_SIGNED   },
  { "!=",      0x200f,            false,       0x001f,          false,     BREW_REG_FLAG_SIGNED   },

  { "<",       0xd00f,            false,       0x00bf,          false,     BREW_REG_FLAG_FLOAT    },
  { ">",       0xd00f,            true,        0x00df,          false,     BREW_REG_FLAG_FLOAT    },
  { "<=",      0xe00f,            true,        0x00ef,          false,     BREW_REG_FLAG_FLOAT    },
  { ">=",      0xe00f,            false,       0x00cf,          false,     BREW_REG_FLAG_FLOAT    },
  { "==",      0x100f,            false,       0x000f,          false,     BREW_REG_FLAG_FLOAT    },
  { "!=",      0x200f,            false,       0x001f,          false,     BREW_REG_FLAG_FLOAT    },

  { NULL,      0x0000,            false,       0x0000,          false,     0                      }
};

#define NO_IMM_A             (1 << 0)
#define NO_IMM_B             (1 << 1)
#define NO_A_EQ_B_D_IS_PC    (1 << 2)
#define NO_A_IS_PC           (1 << 3)
#define NO_B_IS_PC           (1 << 4)
#define NO_D_IS_PC           (1 << 5)
#define NO_AB_IS_PC          (1 << 6)
#define COMMUTATIVE          (1 << 7)
#define HAS_UPPER            (1 << 8)

typedef struct
{
  const char *inst_name;
  uint16_t inst_code;
  int op_flags;
  int type_flags_a;
  int type_flags_b;
  int type_flags_d;
} alu_tableS;

static alu_tableS alu_table[] =
{
/* BINARY OPERATIONS */
/*  inst_name  inst_code   op_flags                                                                        type_flags_a           type_flags_b           type_flags_d*/
  { "^",       0x0000,     NO_A_EQ_B_D_IS_PC | NO_IMM_B | COMMUTATIVE,                                     0,                     0,                     0 },
  { "^",       0x0000,     NO_A_EQ_B_D_IS_PC | NO_IMM_B | COMMUTATIVE,                                     BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "|",       0x1000,     NO_IMM_B | NO_AB_IS_PC | COMMUTATIVE,                                           0,                     0,                     0 },
  { "|",       0x1000,     NO_IMM_B | NO_AB_IS_PC | COMMUTATIVE,                                           BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "&",       0x2000,     NO_IMM_B | COMMUTATIVE,                                                         0,                     0,                     0 },
  { "&",       0x2000,     NO_IMM_B | COMMUTATIVE,                                                         BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "-",       0x3000,     0,                                                                              0,                     0,                     0 },
  { "-",       0x3000,     0,                                                                              BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "+",       0x4000,     NO_IMM_B | COMMUTATIVE,                                                         0,                     0,                     0 },
  { "+",       0x4000,     NO_IMM_B | COMMUTATIVE,                                                         BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "<<",      0x5000,     0,                                                                              0,                     0,                     0 },
  { "<<",      0x5000,     0,                                                                              0,                     BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { ">>",      0x6000,     0,                                                                              0,                     0,                     0 },
  { ">>",      0x7000,     0,                                                                              0,                     BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  { "*",       0x8000,     NO_A_IS_PC | NO_B_IS_PC | NO_D_IS_PC | HAS_UPPER | NO_IMM_B | COMMUTATIVE,      0,                     0,                     0 },
  { "*",       0x9000,     NO_A_IS_PC | NO_B_IS_PC | NO_D_IS_PC | HAS_UPPER | NO_IMM_B | COMMUTATIVE,      BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED  },
  /*           0xa000 is the 'upper' version of 0x8000 */
  /*           0xb000 is the 'upper' version of 0x9000 */
  { "+",       0xc000,     NO_A_IS_PC | NO_B_IS_PC | NO_D_IS_PC | NO_IMM_B | COMMUTATIVE,                  BREW_REG_FLAG_FLOAT,   BREW_REG_FLAG_FLOAT,   BREW_REG_FLAG_FLOAT },
  { "-",       0xd000,     NO_A_IS_PC | NO_B_IS_PC | NO_D_IS_PC,                                           BREW_REG_FLAG_FLOAT,   BREW_REG_FLAG_FLOAT,   BREW_REG_FLAG_FLOAT },
  { "*",       0xe000,     NO_A_IS_PC | NO_B_IS_PC | NO_D_IS_PC | NO_IMM_B | COMMUTATIVE,                  BREW_REG_FLAG_FLOAT,   BREW_REG_FLAG_FLOAT,   BREW_REG_FLAG_FLOAT },
  { NULL,      0x0000,     0,                                                                              0,                     0,                     0 },
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
/*  inst_name  inst_code   op_shift  type_flags_op          type_flags_d*/
  { "-",       0xa000,     4,        BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED },
  { "~",       0xa000,     8,        0,                     0 },
  { "bswap",   0xb000,     4,        0,                     0 },
  { "wswap",   0xb000,     8,        0,                     0 },
  { "bsi",     0xc000,     4,        BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED },
  { "wsi",     0xc000,     8,        BREW_REG_FLAG_SIGNED,  BREW_REG_FLAG_SIGNED },
  { "floor",   0xd000,     4,        BREW_REG_FLAG_FLOAT,   BREW_REG_FLAG_SIGNED },
  { "rsqrt",   0xe000,     8,        BREW_REG_FLAG_FLOAT,   BREW_REG_FLAG_FLOAT },

  { NULL,      0x0000,     0,        0,                     0 }
};

/* This is really unfortunate that as doesn't provide a 'v' version of these routines */
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

const pseudo_typeS md_pseudo_table[] =
{
  {0, 0, 0}
};

const char FLT_CHARS[] = "rRsSfFdDxXpP";
const char EXP_CHARS[] = "eE";

bool float_support = true;
bool trace = false;

/* TOKEN STREAM ROUTINES */
/*************************/

/* Starts a new token stream by initializing tok_start, tok_end and
   tok_end_holder for get_next_token */
static void
start_token_strm(char *str)
{
  tok_end = str;
  tok_end_holder = str[0];
  tok_start = str;
}

/* Stops parsing by restoring the original character stream */
static void 
end_token_strm(void)
{
  if (tok_start == NULL)
    return;
  *tok_end = tok_end_holder;
  tok_start = NULL;
}

const char *special_tokens[] = {
  "<-",
  "==",
  "!=",
  "<=",
  ">=",
  ">>",
  "<<",
  NULL
};


/* Advances tok_start and tok_end to the next token. Sets tok_start to 
   NULL if no more tokens are found. The string is in-place modified by
   putting a zero-termination into the token delimiter. The original
   character is saved in tok_end_holder */
static void
get_optional_next_token(const char *terminals)
{
  int paren_cnt;
  //DEBUG("get_optional_next_token with tok_start: %8p, tok_end: %8p, tok_end_holder: '%c'", tok_start, tok_end, tok_end_holder);

  if (tok_start == NULL)
    return;

  *tok_end = tok_end_holder;
  tok_start = tok_end;
  const char **special_token;

  /* Drop leading whitespace.  */
  while (ISSPACE(*tok_start))
    tok_start++;

  /* Find the end of the token */
  tok_end = tok_start;
  /* If terminals are provided, read until any of them occurs */
  if (terminals != NULL)
    {
      while(strchr(terminals, *tok_end) == NULL)
        {
          ++tok_end;
          if (*tok_end == 0)
            break;
          if (is_end_of_line[*tok_end & 0xff])
            break;
        }
        tok_end_holder = *tok_end;
        *tok_end = 0;
        DEBUG("                 returning %s", tok_start);
        return;
    }

  //DEBUG("get_optional_next_token %s", tok_end);
  /* special 2-char tokens */
  for (special_token = special_tokens; *special_token != NULL; ++special_token)
    {
      if (strncmp(*special_token, tok_start, 2) == 0)
        {
          tok_end = tok_start+2;
          tok_end_holder = *tok_end;
          *tok_end = 0;
          DEBUG("                 returning %s", tok_start);
          return;
        }
    }
  /* We need to be a bit tricky around parnethesis:
     we want something, like '(label + 3 *2 -1)' to
     be parsed into a single token so that the expression
     parser can get it as one go.
  */
  paren_cnt = 0;
  do {
    if (*tok_end == 0)
      break;
    if (is_end_of_line[*tok_end & 0xff])
      break;
    if (ISSPACE(*tok_end) && paren_cnt == 0)
      break;
    if (*tok_end == '(')
      ++paren_cnt;
    if (*tok_end == ')')
      {
        --paren_cnt;
        if (paren_cnt == 0)
          {
            ++tok_end;
            break;
          }
      }
    if (paren_cnt > 0)
      {
        ++tok_end;
        continue;
      }
    /* These are tokens on their own right */
    if (strchr("<>![]=,^&*+/~|-", *tok_end) != NULL)
      {
        if (tok_end == tok_start)
          {
            ++tok_end;
          }
        break;
      }
    /* These are token terminators, but part of the token */
    if (strchr("-", *tok_end) != NULL)
      {
        ++tok_end;
        break;
      }
    tok_end++;
  } while (true);

  tok_end_holder = *tok_end;
  if (tok_end == tok_start)
    {
      tok_start = NULL;
      return;
    }
  *tok_end = 0;
  DEBUG("                 returning %s", tok_start);
}

/* Same as get_optional_next_token, but requires next token to be valid.
   If not, the supplied error error message is emitted and parsing is
   stopped.

   Returns true on success, false on failure.
*/
static bool
vget_next_token(const char *terminals, const char *err_msg, va_list args)
{
  get_optional_next_token(terminals);
  if (tok_start == NULL)
    {
      as_vbad(err_msg, args);
      end_token_strm();
      return false;
    }
  return true;
}
static bool
get_next_token(const char *terminals, const char *err_msg, ...)
{
  va_list args;
  bool ret_val;

  va_start(args, err_msg);
  ret_val = vget_next_token(terminals, err_msg, args);
  va_end(args);
  return ret_val;
}

/* Same as get_next_token, but also tests if token is the supplied one.

   Returns true on success, false on failure.
*/
static bool
is_next_token(const char *expected_tok, char *err_msg, ...)
{
  va_list args;
  bool ret_val;

  va_start(args, err_msg);
  //DEBUG("is_next_token %s enters with %s", expected_tok, tok_start);
  ret_val = vget_next_token(NULL, err_msg, args);
  //DEBUG("is_next_token %s compares to %s with ret_val %s", expected_tok, tok_start, ret_val ? "true":"false");
  if (!ret_val)
    {
    va_end(args);
      return false;
    }

  if (strcasecmp(tok_start, expected_tok) != 0)
    {
      as_vbad(err_msg, args);
      va_end(args);
      end_token_strm();
      return false;

    }
  va_end(args);
  return true;
}

/* Undoes the grabbing of the last token
   NOTE: this can only be done ONCE. It can't
   be used to walk the tokenstream backwards */
static void
undo_last_token(void)
{
  if (tok_start == NULL)
    return;
  *tok_end = tok_end_holder;
  tok_end = tok_start;
  tok_end_holder = *tok_end;
  DEBUG("                 undo");
}

/* back-tracks to the specified point */
static void
back_track_to(char *point)
{
  *tok_end = tok_end_holder;
  tok_end = point;
  tok_end_holder = *tok_end;
  DEBUG("                 back-track to '%s'", tok_end);
}

/* Makes sure the token stream is completely parsed
   Outputs warkings and errors about this and ends the token stream.
*/
static void close_token_strm(void)
{
  get_optional_next_token(NULL);
  if (tok_start != NULL)
    as_warn (_("extra stuff on line ignored '%s'"), tok_start);
  end_token_strm();
}



/* TOKEN PARSERS */
/*****************/

static int
parse_reg_index(char *str)
{
  /* Single-digit hex (or decimal) numbers */
  if (str[0] >= '0' && str[0] <= '9' && str[1] == 0) return str[0] - '0';
  if (str[0] >= 'a' && str[0] <= 'f' && str[1] == 0) return str[0] - 'a' + 10;
  if (str[0] >= 'A' && str[0] <= 'F' && str[1] == 0) return str[0] - 'A' + 10;
  /* double-digit decimal numbers */
  if (str[0] == '1' && str[1] >= '0' && str[1] <= '4' && str[2] == 0) return str[1] - '0' + 10;
  return -1;
}

/*
   registers are named (case insensitive) as:
   $r0...$re or $r0...$r14 for unsigned scalar registers
   $pc is an alias to $r0
   $tpc is also recognized, but handled differently.

   NOTE: any register can be prefixed by:
     s: for signed scalar register - sets BREW_REG_FLAG_SIGNED in return value
     f: for floating point register (aliases) - sets BREW_REG_FLAG_FLOAT in return value

   Return value:
      register index, or BREW_REG_TPC for $tpc and flags set as appropriate
      -1 in case of failure.
*/

typedef struct 
{
  const char *name;
  int regno;
  bool is_tpc;
} named_registers_s;

static named_registers_s named_registers[] = {
  {"pc",  BREW_REG_PC,  false},
  {"tpc", BREW_REG_TPC, true},
  {"sp",  BREW_REG_SP,  false},
  {"fp",  BREW_REG_FP,  false},
  {NULL,  0, false}
};

static int
parse_register_operand (char *token, bool allow_tpc)
{
  int flags = 0;
  int reg_idx;
  named_registers_s *named_register_entry;

  if (token[0] != '$')
    {
      as_bad (_("expecting register, got %s"), token);
      return -1;
    }
  token++;
  /* have to special case $sp */
  if (token[0] == 's' && !(token[1] == 'p' && token[2] == 0)) 
    {
      flags = BREW_REG_FLAG_SIGNED;
      token++;
    }
  /* have to special-case $fp */
  else if (float_support && (token[0] == 'f') && !(token[1] == 'p' && token[2] == 0))
    {
      flags = BREW_REG_FLAG_FLOAT;
      token++;
    }
  
  for (named_register_entry = named_registers; named_register_entry->name != NULL; ++named_register_entry)
    {
      if ((allow_tpc || !named_register_entry->is_tpc) && strcasecmp(token, named_register_entry->name) == 0)
        {
          reg_idx = named_register_entry->regno;
          return flags | reg_idx;
        }
    }
  if (token[0] == 'r')
    {
      reg_idx = parse_reg_index(token+1);
    }
  else
    {
      as_bad (_("illegal register name"));
      return -1;
    }
  if (reg_idx == -1)
    {
      as_bad (_("illegal register index %s"), token);
      return -1;
    }
  return flags | reg_idx;
}

/* Parse an expression and then restore the input line pointer.  */
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

/* Parses a 32-bit expression and puts it in the last four bytes of the fragment buffer

   Returns true if an expression is found, false if not.
*/
static bool
parse_expression(const char *token, bool support_float)
{
  LITTLENUM_TYPE float_store[4]; /* We really shouldn't store more than 4 bytes, but we can only test for that after the call returns. So, oversize the buffer to make sure we won't overflow */
  char float_as_char[4];
  char *ret_val;

  /* The GAS expression parser happily accepts register names (such as $r0) as expressions. We don't want that. */
  if (token[0] == '$')
    return false;

  /* GAS also doesn't support any floating point expressions. We need that for float immediates, so let's test for them */
  /* NOTE: this still doesn't support floating point expressions, such as 1.3+2.6, but that's not supported in GAS anywhere,
     not even in .float */
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

          /* This is a floating point constant we could successfully parse */
          field_e_frag = frag_more(float_size);
          memcpy(field_e_frag, float_as_char, float_size);
          return true;
        }
    }

  expressionS arg;
  char *end_expr;
  end_expr = parse_exp_save_ilp ((char*)token, &arg);
  if (*end_expr != 0)
  {
    return false;
  }
  field_e_frag = frag_more (4);
  fix_new_exp(
    frag_now,
    (field_e_frag - frag_now->fr_literal),
    4,
    &arg,
    0,
    BFD_RELOC_32);
  return true;
}





#define GET_NEXT_TOKEN(...) { bool x; x = get_next_token(NULL, __VA_ARGS__); if (!x) { DEBUG("GET_NEXT_TOKEN RETURN with token %s, position %ld in str %s\n", tok_start, tok_start-str, str); close_token_strm(); ignore_rest_of_line(); return;}}
#define GET_NEXT_TOKEN_UNTIL(...) { bool x; x = get_next_token(__VA_ARGS__); if (!x) { DEBUG("GET_NEXT_TOKEN RETURN with token %s, position %ld in str %s\n", tok_start, tok_start-str, str); close_token_strm(); ignore_rest_of_line(); return;}}
#define IS_NEXT_TOKEN(params) { if (!is_next_token params) { DEBUG("IS_NEXT_TOKEN RETURN with token %s, position %ld in str %s\n", tok_start, tok_start-str, str); close_token_strm(); ignore_rest_of_line(); return;}}
#define ERR_RETURN { DEBUG("ERR RETURN with token %s, position %ld in str %s\n", tok_start, tok_start-str, str); close_token_strm(); ignore_rest_of_line(); return; }
#define RETURN(inst_code) { \
  md_number_to_chars (inst_code_frag, inst_code, 2); \
  if (field_e_frag != NULL) \
    dwarf2_emit_insn(6); \
  else \
    dwarf2_emit_insn(2); \
  close_token_strm(); \
  return; \
}
#define ASSIGNMENT_STR "<-"

/* This is the guts of the machine-dependent assembler.  STR points to
   a machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles to.  */

void
md_assemble (char *str)
{
  int reg_d;
  int reg_b;
  int reg_a;

  start_token_strm(str);

  uint16_t inst_code = 0;

  inst_code_frag = frag_more (2); /* Let's start a new fragment for the 16-bit instruction code */
  field_e_frag = NULL; /* We initially don't have field_e. If, during parsing we encouter an expression, we'll allocate a fragment here */

  DEBUG("md_assemble: %s", str);

  GET_NEXT_TOKEN(_("Invalid instruction: not a single token is found "));
  /* Deal with simple instructions */
  {
    inst_tableS *inst_table_entry;
    for (inst_table_entry = inst_table; inst_table_entry->inst_name != NULL; ++inst_table_entry)
      {
        if (strcasecmp(tok_start, inst_table_entry->inst_name) == 0)
          RETURN(inst_table_entry->inst_code);
      }
  }

  /* Store operations */
  do
    {
      if (strcasecmp(tok_start, "mem") == 0)
        inst_code = 0xf700;
      else if (strcasecmp(tok_start, "mem32") == 0)
        inst_code = 0xf700;
      else if (strcasecmp(tok_start, "mem8") == 0)
        inst_code = 0xf500;
      else if (strcasecmp(tok_start, "mem16") == 0)
        inst_code = 0xf600;
      else
        break;
      IS_NEXT_TOKEN(("[", _("invalid store operation syntax ")));
      GET_NEXT_TOKEN_UNTIL("],", _("invalid store operation syntax "));
      /* There are four formats we recognize here: {reg}; {expr}; {reg},{expr}; {expr},{reg} */
      if (parse_expression(tok_start, false))
        {
          inst_code |= 0x0800; /* Set the appropriate bit to signal the presence of an immediate offset */
          GET_NEXT_TOKEN(_("invalid store offset syntax"));
          if (strcmp(tok_start, ",") == 0)
            {
              /* We have the format of MEM[{expr},{reg}] = {reg} */
              GET_NEXT_TOKEN(_("invalid store offset syntax"));
              reg_a = parse_register_operand(tok_start, false);
              if (reg_a == -1)
                {
                  as_bad(_("Invlid register offset in store"));
                  ERR_RETURN;
                }
            }
          else
            {
              /* We have the format of MEM[{expr}] = {reg} */
              reg_a = 0xf;
              undo_last_token();
            }
        }
      else
        {
          reg_a = parse_register_operand(tok_start, false);
          if (reg_a == -1)
            {
              as_bad(_("Invlid register offset in store"));
              ERR_RETURN;
            }
          GET_NEXT_TOKEN(_("invalid store offset syntax"));
          if (strcmp(tok_start, ",") == 0)
            {
              GET_NEXT_TOKEN_UNTIL("]",_("invalid store offset syntax"));
              if (!parse_expression(tok_start, false))
                {
                  as_bad(_("Invalid store offset syntax: expecting expression"));
                  ERR_RETURN;
                }
              /* We have the format of MEM[{reg},{expr}] = {reg} */
              inst_code |= 0x0800; /* Set the appropriate bit to signal the presence of an immediate offset */
            }
          else
            {
              /* We have the format of MEM[{reg}] = {reg} */
              undo_last_token();
            }
        }
      IS_NEXT_TOKEN(("]", _("invalid store operation syntax ")));
      IS_NEXT_TOKEN((ASSIGNMENT_STR, _("invalid store instruction syntax: expecting " ASSIGNMENT_STR ", got %s"), tok_start));
      GET_NEXT_TOKEN(_("invalid store operation syntax "));
      reg_d = parse_register_operand(tok_start, true);
      if (reg_d == -1)
        {
          as_bad(_("Invalid source register for store "));
          ERR_RETURN;
        }
      inst_code |= (reg_a & 0xf) << 4;
      /* special-case TPC stores: these only have a 48-bit variant */
      if ((reg_d & 0xf) == BREW_REG_TPC)
        {
          inst_code |= 0x0800;
          inst_code |= 0x000f;
          if (field_e_frag == NULL)
            {
              field_e_frag = frag_more(4);
              md_number_to_chars (field_e_frag, 0, 4);
            }
          RETURN(inst_code);
        }
      inst_code |= (reg_d & 0xf) << 0;
      RETURN(inst_code);
    }
  while (false);


  /* branches */
  if (strcasecmp(tok_start, "if") == 0)
    {
      int reg1;
      int reg2;

      GET_NEXT_TOKEN(_("invalid instruction syntax"));
      reg1 = parse_register_operand(tok_start, false);
      if (reg1 == -1)
        {
          as_bad(_("Invalid branch instruction: expected test register"));
          ERR_RETURN;
        }
      GET_NEXT_TOKEN(_("invalid instruction syntax"));
      if (strcmp(tok_start, "[") == 0)
        {
          int bit_idx;
          /* This is a bit-test branch */
          GET_NEXT_TOKEN(_("invalid instruction syntax"));
          bit_idx = parse_reg_index(tok_start);
          if (bit_idx == -1)
            {
              as_bad(_("Invalid bit-test branch instruction: bit-index must be between 0 and 14"));
              ERR_RETURN;
            }
          IS_NEXT_TOKEN(("]", _("invalid bit-test branch instruction: expected ']'")));
          IS_NEXT_TOKEN(("==", _("invalid bit-test branch instruction: expected '=='")));
          GET_NEXT_TOKEN(_("invalid bit-test branch instruction: expected condition"));
          if (strcmp(tok_start, "0") == 0)
            {
              inst_code = 0x00ff;
              inst_code |= bit_idx << 12;
              inst_code |= (reg1 & 0xf) << 8;
            }
          else if (strcmp(tok_start, "1") == 0)
            {
              inst_code = 0x0f0f;
              inst_code |= bit_idx << 12;
              inst_code |= (reg1 & 0xf) << 4;
            }
          else
            {
              as_bad(_("invalid bit-test branch instruction: condition must be 0 or 1. Got: %s"), tok_start);
            }
          IS_NEXT_TOKEN(("$pc", _("invalid bit-test branch instruction: expected '$pc'")));
          IS_NEXT_TOKEN(("<-", _("invalid bit-test branch instruction: expected '<-'")));
          GET_NEXT_TOKEN(_("invalid bit-test branch instruction: expected branch target"));
          if (!parse_expression(tok_start, false))
            {
              as_bad(_("invalid bit-test branch instruction: expected branch target"));
              ERR_RETURN;
            }
          RETURN(inst_code);
        }
      else
        {
          /* This is a comparison branch */
          char branch_op[3];
          if (strlen(tok_start) > 2)
            {
              as_bad(_("invalid comparison branch instruction: unrecognized condition: %s"), tok_start);
              ERR_RETURN;
            }
          strcpy(branch_op, tok_start);
          GET_NEXT_TOKEN(_("invlid comparison branch instruction: expected second comparison argument"));
          if (strcmp(tok_start, "0") == 0)
            {
              /* comparison with 0 */
              branch_tableS *branch_table_entry;

              for (branch_table_entry = branch_table; branch_table_entry->inst_name != NULL; ++branch_table_entry)
                {
                  if (strcmp(branch_op, branch_table_entry->inst_name) == 0 && branch_table_entry->flags == (reg1 & BREW_REG_FLAG_MASK))
                    break;
                }
              if (branch_table_entry->inst_name == NULL)
                {
                  as_bad(_("invalid comparison branch instruction: unrecognized condition: %s"), branch_op);
                  ERR_RETURN;
                }
              inst_code = branch_table_entry->zero_inst_code;
              if (!branch_table_entry->complete)
                {
                  inst_code |= (reg1 & 0xf) << 8;
                }
              IS_NEXT_TOKEN(("$pc", _("invalid comparison branch instruction: expected '$pc'")));
              IS_NEXT_TOKEN(("<-", _("invalid comparison branch instruction: expected '<-'")));
              GET_NEXT_TOKEN(_("invalid comparison branch instruction: expected branch target"));
              if (!parse_expression(tok_start, false))
                {
                  as_bad(_("invalid comparison branch instruction: expected branch target"));
                  ERR_RETURN;
                }
              RETURN(inst_code);
            }
          else
            {
              branch_tableS *branch_table_entry;

              reg2 = parse_register_operand(tok_start, false);
              if (reg2 == -1)
                {
                  as_bad(_("invalid branch instruction: second comparison argument must be 0 or a register"));
                  ERR_RETURN;
                }
              /* comparison between two registers */
              if ((reg1 & BREW_REG_FLAG_MASK) != (reg2 & BREW_REG_FLAG_MASK))
                {
                  as_bad(_("invalid branch instruction: comparison must be between registers of the same type"));
                  ERR_RETURN;
                }
              for (branch_table_entry = branch_table; branch_table_entry->inst_name != NULL; ++branch_table_entry)
                {
                  if (strcmp(branch_op, branch_table_entry->inst_name) == 0 && branch_table_entry->flags == (reg1 & BREW_REG_FLAG_MASK))
                    break;
                }
              if (branch_table_entry->inst_name == NULL)
                {
                  as_bad(_("invalid branch instruction: unrecognized condition: %s"), branch_op);
                  ERR_RETURN;
                }

              inst_code = branch_table_entry->two_op_inst_code;
              if (branch_table_entry->swap_ops)
                {
                  reg_a = reg1 & 0xf;
                  reg_b = reg2 & 0xf;
                }
              else
                {
                  reg_a = reg2 & 0xf;
                  reg_b = reg1 & 0xf;
                }
              inst_code |= reg_b << 8;
              inst_code |= reg_a << 4;
              IS_NEXT_TOKEN(("$pc", _("invalid bit-test branch instruction: expected '$pc'")));
              IS_NEXT_TOKEN(("<-", _("invalid bit-test branch instruction: expected '<-'")));
              GET_NEXT_TOKEN(_("invalid bit-test branch instruction: expected branch target"));
              if (!parse_expression(tok_start, false))
                {
                  as_bad(_("invalid bit-test branch instruction: expected branch target"));
                  ERR_RETURN;
                }
              RETURN(inst_code);
            }
          ABORT((_("internal error during branch parsing")));
        }
      ABORT((_("internal error during branch parsing")));
    }

  /* All other operations have the form of {reg} <- ... */
  reg_d = parse_register_operand(tok_start, true);
  if (reg_d == -1)
    {
      as_bad(_("Invalid target register for instruction"));
      ERR_RETURN;
    }
  IS_NEXT_TOKEN((ASSIGNMENT_STR, _("Invalid operation ")));
  GET_NEXT_TOKEN(_("Incomplete instructions"));


  /* Load operations */
  do
    {
      if (strcasecmp(tok_start, "mem") == 0 || strcasecmp(tok_start, "mem32") == 0)
        inst_code = 0xf400;
      else if (strcasecmp(tok_start, "mem8") == 0)
        {
          if ((reg_d & BREW_REG_FLAG_SIGNED) != 0)
            inst_code = 0xf000;
          else
            inst_code = 0xf100;
        }
      else if (strcasecmp(tok_start, "mem16") == 0)
        {
          if ((reg_d & BREW_REG_FLAG_SIGNED) != 0)
            inst_code = 0xf200;
          else
            inst_code = 0xf300;
        }
      else
        break;
      IS_NEXT_TOKEN(("[", _("invalid load operation syntax ")));
      GET_NEXT_TOKEN_UNTIL("],", _("invalid load operation syntax "));
      /* There are four formats we recognize here: {reg}; {expr}; {reg},{expr}; {expr},{reg} */
      if (parse_expression(tok_start, false))
        {
          inst_code |= 0x0800; /* Set the appropriate bit to signal the presence of an immediate offset */
          GET_NEXT_TOKEN(_("invalid load offset syntax"));
          if (strcmp(tok_start, ",") == 0)
            {
              /* We have the format of {reg} = MEM[{expr},{reg}]*/
              GET_NEXT_TOKEN(_("invalid load offset syntax"));
              reg_a = parse_register_operand(tok_start, false);
              if (reg_a == -1)
                {
                  as_bad(_("Invlid register offset in load"));
                  ERR_RETURN;
                }
            }
          else
            {
              /* We have the format of {reg} = MEM[{expr}] */
              reg_a = 0xf;
              undo_last_token();
            }
        }
      else
        {
          reg_a = parse_register_operand(tok_start, false);
          if (reg_a == -1)
            {
              as_bad(_("Invlid register offset in load"));
              ERR_RETURN;
            }
          GET_NEXT_TOKEN(_("invalid load offset syntax"));
          if (strcmp(tok_start, ",") == 0)
            {
              GET_NEXT_TOKEN_UNTIL("]", _("invalid load offset syntax"));
              if (!parse_expression(tok_start, false))
                {
                  as_bad(_("Invalid load offset syntax: expecting expression"));
                  ERR_RETURN;
                }
              /* We have the format of {reg} = MEM[{reg},{expr}] */
              inst_code |= 0x0800; /* Set the appropriate bit to signal the presence of an immediate offset */
            }
          else
            {
              /* We have the format of {reg} = MEM[{reg}] */
              undo_last_token();
            }
        }
      IS_NEXT_TOKEN(("]", _("invalid load operation syntax ")));
      inst_code |= (reg_a & 0xf) << 4;
      /* special-case TPC stores: these only have a 48-bit variant */
      if ((reg_d & 0xf) == BREW_REG_TPC)
        {
          inst_code |= 0x0800;
          inst_code |= 0x000f;
          if (field_e_frag == NULL)
            {
              field_e_frag = frag_more(4);
              md_number_to_chars (field_e_frag, 0, 4);
            }
          RETURN(inst_code);
        }
      inst_code |= (reg_d & 0xf) << 0;
      RETURN(inst_code);
    }
  while (false);

  /* From here on every operation has the format of {reg} <- ... */
  /* We've already parsed the destination register and the assignment part. Our current token is the first one after the assignment */
  /* ALU operations */
  do
    {
      bool is_upper = false;
      unary_op_tableS *unary_op_table_entry;
      if (strcasecmp(tok_start, "upper") == 0)
        {
          is_upper = true;
          GET_NEXT_TOKEN(_("unexpected end of instruction after 'upper'"));
        }
      /* check for prefix unary operators */
      for (unary_op_table_entry = unary_op_table; unary_op_table_entry->inst_name != NULL; ++unary_op_table_entry)
        {
          if (strcasecmp(tok_start, unary_op_table_entry->inst_name) == 0 && (unary_op_table_entry->type_flags_d == (reg_d & BREW_REG_FLAG_MASK)))
            {
              int reg_op;
              /* Found a unary ALU operation */
              if (is_upper)
                {
                  as_bad(_("unary operation %s can't be prefixed with 'upper'"), unary_op_table_entry->inst_name);
                  ERR_RETURN;
                }
              GET_NEXT_TOKEN(_("expected operand for unary operation %s"), tok_start);
              reg_op = parse_register_operand(tok_start, false);
              if ((reg_d & 0xf) == BREW_REG_TPC)
                {
                  as_bad(_("unary operation %s can't target TPC"), unary_op_table_entry->inst_name);
                  ERR_RETURN;
                }
              if ((reg_d & 0xf) == BREW_REG_PC)
                {
                  as_bad(_("unary operation %s can't target PC"), unary_op_table_entry->inst_name);
                  ERR_RETURN;
                }
              if (reg_op == -1)
                {
                  as_bad(_("operand for unary operation %s must be a register"), unary_op_table_entry->inst_name);
                  ERR_RETURN;
                }
              inst_code = unary_op_table_entry->inst_code;
              inst_code |= (reg_op & 0xf) << unary_op_table_entry->op_shift;
              inst_code |= (reg_d & 0xf);
              RETURN(inst_code);
            }
        }
      /* it's not a prefix operation. It could be a register or an expression or a few special cases. Check for those first */
      if (strcmp(tok_start, "1") == 0)
        {
          if (is_upper)
            {
              as_bad(_("unexpected 'upper'"));
              ERR_RETURN;
            }
          char *back_track_point = tok_start;
          get_optional_next_token(NULL);
          if (tok_start == NULL)
            {
              /* This is simply {reg} <- 1. We need to handle that here even though load immediate is usually handled elsewhere, but we're too far down the road to back out. */
              if ((reg_d & BREW_REG_FLAG_FLOAT) != 0)
                {
                  ABORT(("FIXME: we need to create a float constant for 1.0f"));
                }
              else
                {
                  parse_expression("1", false);
                }
              if ((reg_d & 0xf) == BREW_REG_TPC)
                inst_code = 0x80f0;
              else
                {
                  inst_code = 0x0ff0;
                  inst_code |= (reg_d & 0xf);
                }
              RETURN(inst_code);
            }
          if ((reg_d & BREW_REG_FLAG_FLOAT) != 0)
            {
              if ((reg_d & 0xf) == BREW_REG_TPC)
                {
                  as_bad(_("reciprocal operation can't target TPC"));
                  ERR_RETURN;
                }
              if ((reg_d & 0xf) == BREW_REG_PC)
                {
                  as_bad(_("reciprocal operation can't target PC"));
                  ERR_RETURN;
                }
              int reg_op;
              if (strcasecmp(tok_start, "/") != 0)
                {
                  as_bad(_("expected division (/) operator, got %s"), tok_start);
                  ERR_RETURN;
                }
              GET_NEXT_TOKEN(_("expected operand for reciprocal"));
              reg_op = parse_register_operand(tok_start, false);
              if (reg_op == -1)
                {
                  as_bad(_("expected register operand for reciprocal"));
                  ERR_RETURN;
                }
              if ((reg_op & BREW_REG_FLAG_FLOAT) == 0)
                {
                  as_bad(_("reciprocal operand must be a floating point register"));
                  ERR_RETURN;
                }
              if ((reg_op & 0xf) == BREW_REG_PC)
                {
                  as_bad(_("reciprocal operand can't be PC"));
                  ERR_RETURN;
                }
              inst_code = 0xe000;
              inst_code |= (reg_op & 0xf) << 4;
              inst_code |= (reg_d & 0xf) << 0;
              RETURN(inst_code);
            }
            /* back-track to further processing... */
            back_track_to(back_track_point);
            GET_NEXT_TOKEN(_("Unable to get next token after undo?!"));
        }
      /* It could be a negative immediate ($r1 <- -1), which would fail the previous check for prefix operations, so check for that. */
      if (strcmp(tok_start, "-") == 0)
        {
          char *real_start = tok_start;
          get_optional_next_token(NULL); // This will advance tok_end to the next token and replace the zero-terminator
          tok_start = real_start;
        }
      //DEBUG("trying to parse token '%s' as expression", tok_start);
      /* let's see if this is a simple immediate load */
      if (parse_expression(tok_start, (reg_d & BREW_REG_FLAG_MASK) == BREW_REG_FLAG_FLOAT))
        {
          char op[10];
          alu_tableS *alu_table_entry;

          get_optional_next_token(NULL);
          if (tok_start == NULL)
            {
              /* This is an immediate load */
              if (is_upper)
                {
                  as_bad(_("unexpected 'upper' register load"));
                  ERR_RETURN;
                }
              if ((reg_d & 0xf) == BREW_REG_TPC)
                {
                  inst_code = 0x80f0;
                }
              else
                {
                  inst_code = 0x0ff0;
                  inst_code |= (reg_d & 0xf);
                }
              RETURN(inst_code);
            }
          /* This is not an immediate load. it must be a binary operation of the {reg} <- {expr} {op} {reg} kind. */
          if (strlen(tok_start) > sizeof(op)-1)
            {
              as_bad(_("invalid binary operation: %s"), tok_start);
              ERR_RETURN;
            }
          strcpy(op, tok_start);
          GET_NEXT_TOKEN(_("expected second operand for binary operation '%s'"), op);
          reg_a = parse_register_operand(tok_start, false);
          if (reg_a == -1)
            {
              as_bad(_("second operatand for operation %s must be a register"), op);
              ERR_RETURN;
            }
          for (alu_table_entry = alu_table; alu_table_entry->inst_name != NULL; ++alu_table_entry)
            {
              if (strcasecmp(op, alu_table_entry->inst_name) == 0)
                {
                  bool swap_args = false;
                  /* Can we use this entry for the {exp} {op} {reg} thing we have here? */
                  if ((alu_table_entry->op_flags & NO_IMM_B) != 0)
                    {
                      if ((alu_table_entry->op_flags & COMMUTATIVE) == 0)
                        continue;
                      if ((alu_table_entry->op_flags & NO_IMM_A) != 0)
                        continue;
                      swap_args = true;
                      reg_b = reg_a;
                    }
                  if (swap_args)
                    {
                      if ((reg_b & 0xf) == BREW_REG_PC && (alu_table_entry->op_flags & NO_B_IS_PC) != 0)
                        continue;
                      if ((reg_b & BREW_REG_FLAG_MASK) != alu_table_entry->type_flags_b)
                        continue;
                    }
                  else
                    {
                      if ((reg_a & 0xf) == BREW_REG_PC && (alu_table_entry->op_flags & NO_A_IS_PC) != 0)
                        continue;
                      if ((reg_a & BREW_REG_FLAG_MASK) != alu_table_entry->type_flags_a)
                        continue;
                    }
                  if ((reg_d & 0xf) == BREW_REG_PC && (alu_table_entry->op_flags & NO_D_IS_PC) != 0)
                    continue;
                  if ((reg_d & BREW_REG_FLAG_MASK) != alu_table_entry->type_flags_d)
                    continue;
                  if ((alu_table_entry->op_flags & HAS_UPPER) == 0 && is_upper)
                    {
                      as_bad(_("operation %s doesn't support 'upper'"), op);
                      ERR_RETURN;
                    }
                  /* operation is suitable */
                  inst_code = alu_table_entry->inst_code;
                  if (is_upper)
                    {
                      inst_code += 0x2000;
                    }
                  if (swap_args)
                    {
                      inst_code |= 0x00f0; /* mark OP A as immediate */
                      inst_code |= (reg_b & 0xf) << 8;
                      inst_code |= (reg_d & 0xf) << 0;
                      RETURN(inst_code);
                    }
                  else
                    {
                      inst_code |= 0x0f00; /* mark OP B as immediate */
                      inst_code |= (reg_a & 0xf) << 4;
                      inst_code |= (reg_d & 0xf) << 0;
                      RETURN(inst_code);
                    }
                }
            }
            as_bad(_("unrecognized binary operation %s"), op);
            ERR_RETURN;
        }
      reg_b = parse_register_operand(tok_start, true);
      if (reg_b == -1)
        {
          as_bad(_("expected source register, got %s"), tok_start);
          ERR_RETURN;
        }
      do {
          char op[10];
          alu_tableS *alu_table_entry;

          get_optional_next_token(NULL);
          if (tok_start == NULL)
            {
              /* This is a register move */
              if (is_upper)
                {
                  as_bad(_("unexpected 'upper' for register move"));
                  ERR_RETURN;
                }
              if ((reg_d & 0xf) == BREW_REG_TPC || (reg_b & 0xf) == BREW_REG_TPC)
                {
                  if ((reg_d & 0xf) == BREW_REG_PC || (reg_b & 0xf) == BREW_REG_PC)
                    {
                      as_bad(_("direct move between $PC and $TPC is not supported"));
                      ERR_RETURN;
                    }
                  /* This is a TPC move */
                  reg_d = ((reg_d & 0xf) == BREW_REG_TPC) ? 0 : (reg_d & 0xf);
                  reg_b = ((reg_b & 0xf) == BREW_REG_TPC) ? 0 : (reg_b & 0xf);
                  inst_code = 0x8000;
                  inst_code |= reg_b << 4;
                  inst_code |= reg_d << 0;
                  RETURN(inst_code);
                }
              if ((reg_d & BREW_REG_FLAG_MASK) != 0 && (reg_b & BREW_REG_FLAG_SIGNED) != 0)
                {
                  /* This is float conversion */
                  inst_code = 0xd000;
                  inst_code |= (reg_b & 0xf) << 8;
                  inst_code |= (reg_d & 0xf) << 0;
                  RETURN(inst_code);
                }
              if ((reg_d & BREW_REG_FLAG_MASK) != (reg_b & BREW_REG_FLAG_MASK))
                {
                  as_bad(_("register moves only support identical source and destination types"));
                  ERR_RETURN;
                }
              /* This is a simple register move -> use the AND operation with both sources being the same */
              inst_code = 0x2000;
              inst_code |= (reg_b & 0xf) << 8;
              inst_code |= (reg_b & 0xf) << 4;
              inst_code |= (reg_d & 0xf) << 0;
              RETURN(inst_code);
            }
          /* This is not a register move. it must be a binary operation of the {reg} <- {reg} {op} [{expr}|{reg}] kind. */
          if (strlen(tok_start) > sizeof(op)-1)
            {
              as_bad(_("invalid binary operation: %s"), tok_start);
              ERR_RETURN;
            }
          strcpy(op, tok_start);
          //DEBUG("getting second operand for op %s", op);
          GET_NEXT_TOKEN_UNTIL("\0", _("expected second operand for binary operation '%s'"), op);
          /* Special-case increment and decrement operations */
          do
            {
              if (strcmp(tok_start, "1") == 0)
                {
                  if ((reg_b & 0xf) == BREW_REG_PC)
                    break;
                  if ((reg_b & BREW_REG_FLAG_FLOAT) != 0)
                    break;
                  if ((reg_b & BREW_REG_FLAG_MASK) != (reg_d & BREW_REG_FLAG_MASK))
                    break;
                  if (strcmp(op, "+") == 0)
                    {
                      inst_code = 0x9000;
                      inst_code |= (reg_b & 0xf) << 4;
                      inst_code |= (reg_d & 0xf) << 0;
                      RETURN(inst_code);
                    }
                  else if (strcmp(op, "-") == 0)
                    {
                      inst_code = 0x9000;
                      inst_code |= (reg_b & 0xf) << 8;
                      inst_code |= (reg_d & 0xf) << 0;
                      RETURN(inst_code);
                    }
                }
            }
          while(false);
          /* Let's see if we have a {reg} {op} {exp} type binary operation */
          if (parse_expression(tok_start, (reg_d & BREW_REG_FLAG_MASK) == BREW_REG_FLAG_FLOAT))
            {
              for (alu_table_entry = alu_table; alu_table_entry->inst_name != NULL; ++alu_table_entry)
                {
                  if (strcasecmp(op, alu_table_entry->inst_name) == 0)
                    {
                      bool swap_args = false;
                      /* Can we use this entry for the {reg} {op} {exp} thing we have here? */
                      if ((alu_table_entry->op_flags & NO_IMM_A) != 0)
                        {
                          if ((alu_table_entry->op_flags & COMMUTATIVE) == 0)
                            continue;
                          if ((alu_table_entry->op_flags & NO_IMM_B) != 0)
                            continue;
                          swap_args = true;
                          reg_a = reg_b;
                        }
                      if (swap_args)
                        {
                          if ((reg_a & 0xf) == BREW_REG_PC && (alu_table_entry->op_flags & NO_A_IS_PC) != 0)
                            continue;
                          if ((reg_a & BREW_REG_FLAG_MASK) != alu_table_entry->type_flags_a)
                            continue;
                        }
                      else
                        {
                          if ((reg_b & 0xf) == BREW_REG_PC && (alu_table_entry->op_flags & NO_B_IS_PC) != 0)
                            continue;
                          if ((reg_b & BREW_REG_FLAG_MASK) != alu_table_entry->type_flags_b)
                            continue;
                        }
                      if ((reg_d & 0xf) == BREW_REG_PC && (alu_table_entry->op_flags & NO_D_IS_PC) != 0)
                        continue;
                      if ((reg_d & BREW_REG_FLAG_MASK) != alu_table_entry->type_flags_d)
                        continue;
                      if ((alu_table_entry->op_flags & HAS_UPPER) == 0 && is_upper)
                        {
                          as_bad(_("operation %s doesn't support 'upper'"), op);
                          ERR_RETURN;
                        }
                      /* operation is suitable */
                      inst_code = alu_table_entry->inst_code;
                      if (is_upper)
                        {
                          inst_code += 0x2000;
                        }
                      if (swap_args)
                        {
                          inst_code |= 0x0f00; /* mark OP B as immediate */
                          inst_code |= (reg_a & 0xf) << 4;
                          inst_code |= (reg_d & 0xf) << 0;
                          RETURN(inst_code);
                        }
                      else
                        {
                          inst_code |= 0x00f0; /* mark OP A as immediate */
                          inst_code |= (reg_b & 0xf) << 8;
                          inst_code |= (reg_d & 0xf) << 0;
                          RETURN(inst_code);
                        }
                    }
                }
                as_bad(_("unrecognized binary operation %s"), op);
                ERR_RETURN;
            }
          reg_a = parse_register_operand(tok_start, false);
          if (reg_a == -1)
            {
              as_bad(_("second operatand for operation %s must be a register or an expression. Got: %s"), op, tok_start);
              ERR_RETURN;
            }
          for (alu_table_entry = alu_table; alu_table_entry->inst_name != NULL; ++alu_table_entry)
            {
              //DEBUG("Testing for compatiblity with %s", alu_table_entry->inst_name);
              if (strcasecmp(op, alu_table_entry->inst_name) == 0)
                {
                  /* Can we use this entry for the {reg} {op} {reg} thing we have here? */
                  if ((reg_a & 0xf) == (reg_b & 0xf) && (reg_d & 0xf) == BREW_REG_PC && (alu_table_entry->op_flags & NO_A_EQ_B_D_IS_PC) != 0)
                    continue;
                  if ((reg_a & 0xf) == BREW_REG_PC && (alu_table_entry->op_flags & NO_A_IS_PC) != 0)
                    continue;
                  if ((reg_b & 0xf) == BREW_REG_PC && (alu_table_entry->op_flags & NO_B_IS_PC) != 0)
                    continue;
                  if ((reg_d & 0xf) == BREW_REG_PC && (alu_table_entry->op_flags & NO_D_IS_PC) != 0)
                    continue;
                  if ((reg_a & BREW_REG_FLAG_MASK) != alu_table_entry->type_flags_a)
                    continue;
                  if ((reg_b & BREW_REG_FLAG_MASK) != alu_table_entry->type_flags_b)
                    continue;
                  if ((reg_d & BREW_REG_FLAG_MASK) != alu_table_entry->type_flags_d)
                    continue;
                  if ((alu_table_entry->op_flags & HAS_UPPER) == 0 && is_upper)
                    {
                      as_bad(_("operation %s doesn't support 'upper'"), op);
                      ERR_RETURN;
                    }
                  /* operation is suitable */
                  inst_code = alu_table_entry->inst_code;
                  if (is_upper)
                    {
                      inst_code += 0x2000;
                    }
                  inst_code |= (reg_a & 0xf) << 4;
                  inst_code |= (reg_b & 0xf) << 8;
                  inst_code |= (reg_d & 0xf) << 0;
                  RETURN(inst_code);
                }
            }
            as_bad(_("unrecognized binary operation %s"), op);
            ERR_RETURN;
        }
      while (false);
    }
  while (false);
  
  as_bad(_("Unrecognized instruction"));
  ERR_RETURN;

}

/* Turn a string in input_line_pointer into a floating point constant
   of type type, and store the appropriate bytes in *litP.  The number
   of bytes emitted into litP is stored in *sizeP .  An error message is
   returned, or NULL on OK.  */

const char *
md_atof (int type, char *litP, int *sizeP)
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
  return ieee_md_atof (type, litP, sizeP, target_big_endian);
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

size_t md_longopts_size = sizeof (md_longopts);

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
      trace = true;
      break;
    case OPTION_NT:
      trace = false;
      break;
    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (FILE *stream ATTRIBUTE_UNUSED)
{
  fprintf (stream, _("\
  -F                      enable floating point instructions (default)\n\
  -NF                     disable floating point instructions\n"));
}

/* Apply a fixup to the object file.  */

void
md_apply_fix (fixS *fixP ATTRIBUTE_UNUSED,
              valueT * valP ATTRIBUTE_UNUSED, segT seg ATTRIBUTE_UNUSED)
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  long val = *valP;
  /*long newval;*/
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
    /* The only reloc we can actually have is a 32-bit one
    case BFD_RELOC_16:
      buf[1] = val >> 8;
      buf[0] = val >> 0;
      buf += 2;
      break;

    case BFD_RELOC_8:
      *buf++ = val;
      break;
    */
    default:
      as_fatal("Relocation type %d is not supported on brew. Do you have an undefined symbol?", fixP->fx_r_type);
    }

  if (max != 0 && (val < min || val > max))
    as_bad_where (fixP->fx_file, fixP->fx_line, _("offset out of range"));

  if (fixP->fx_addsy == NULL && fixP->fx_pcrel == 0)
    fixP->fx_done = 1;
}

/* Put number into target byte order.  */

void
md_number_to_chars (char * ptr, valueT use, int nbytes)
{
  number_to_chars_littleendian (ptr, use, nbytes);
}


/* Generate a machine-dependent relocation.  */
arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixP)
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

/* Decide from what point a pc-relative relocation is relative to,
   relative to the pc-relative fixup.  Er, relatively speaking.  */
long
md_pcrel_from (fixS *fixP)
{
  valueT addr = fixP->fx_where + fixP->fx_frag->fr_address;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_32:
      return addr + 4;
    /* We don't have this type of relocation */
    /*
    case BFD_RELOC_MOXIE_10_PCREL:
      // Offset is from the end of the instruction.
      return addr + 2;
    */
    default:
      abort ();
      return addr;
    }
}

void
md_operand (expressionS *op __attribute__((unused)))
{
  /* Empty for now. */
}

/* This function is called once, at assembler startup time.  It can
   do all sorts of local initialization that needs to happen. */
void
md_begin (void)
{
  bfd_set_arch_mach (stdoutput, TARGET_ARCH, 0);
  record_alignment (text_section, 1);
  record_alignment (data_section, 2);
  record_alignment (bss_section, 2);
}
