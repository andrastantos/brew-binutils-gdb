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

#define DEBUG(msg) { printf msg; printf("\n"); }

extern const brew_opc_info_t brew_opc_info[128];

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
  { NULL,      0x0000 }
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

/* Advances tok_start and tok_end to the next token. Sets tok_start to 
   NULL if no more tokens are found. The string is in-place modified by
   putting a zero-termination into the token delimiter. The original
   character is saved in tok_end_holder */
static void
get_optional_next_token(void)
{
  //DEBUG(("get_optional_next_token with tok_start: %8p, tok_end: %8p, tok_end_holder: '%c'", tok_start, tok_end, tok_end_holder));

  if (tok_start == NULL)
    return;

  *tok_end = tok_end_holder;
  tok_start = tok_end;
  /* Drop leading whitespace.  */
  while (ISSPACE(*tok_start))
    tok_start++;

  /* Find the end of the token */
  tok_end = tok_start;
  //DEBUG(("get_optional_next_token %s", tok_end));
  do {
    if (*tok_end == 0)
      break;
    if (is_end_of_line[*tok_end & 0xff])
      break;
    if (ISSPACE(*tok_end))
      break;
    /* These are tokens on their own right */
    if (strchr("[]=,", *tok_end) != NULL)
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
  DEBUG(("                 returning %s", tok_start));
}

/* Same as get_optional_next_token, but requires next token to be valid.
   If not, the supplied error error message is emitted and parsing is
   stopped.

   Returns true on success, false on failure.
*/
static bool
vget_next_token(const char *err_msg, va_list args)
{
  get_optional_next_token();
  if (tok_start == NULL)
    {
      as_vbad(err_msg, args);
      end_token_strm();
      return false;
    }
  return true;
}
static bool
get_next_token(const char *err_msg, ...)
{
  va_list args;
  bool ret_val;

  va_start(args, err_msg);
  ret_val = vget_next_token(err_msg, args);
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
  //DEBUG(("is_next_token %s enters with %s", expected_tok, tok_start));
  ret_val = vget_next_token(err_msg, args);
  //DEBUG(("is_next_token %s compares to %s with ret_val %s", expected_tok, tok_start, ret_val ? "true":"false"));
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
}

/* Makes sure the token stream is completely parsed
   Outputs warkings and errors about this and ends the token stream.
*/
static void close_token_strm(void)
{
  get_optional_next_token();
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
static int
parse_register_operand (char *token, bool allow_tpc)
{
  int flags = 0;
  int reg_idx;

  if (token[0] != '$')
    {
      as_bad (_("expecting register, got %s"), token);
      return -1;
    }
  token++;
  if (token[0] == 's')
    {
      flags = BREW_REG_FLAG_SIGNED;
      token++;
    }
  else if (float_support && (token[0] == 'f'))
    {
      flags = BREW_REG_FLAG_FLOAT;
      token++;
    }
  
  if (strcasecmp(token, "pc") == 0)
    reg_idx = BREW_REG_PC;
  else if (allow_tpc && strcasecmp(token, "tpc") == 0)
    reg_idx = BREW_REG_TPC;
  else if (token[0] == 'r')
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
parse_expression(char *token)
{
  /* The GAS expression parser happily accepts register names (such as $r0) as expressions. We don't want that. */
  if (token[0] == '$')
    return false;

  expressionS arg;
  char *end_expr;
  end_expr = parse_exp_save_ilp (token, &arg);
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





#define GET_NEXT_TOKEN(err_msg) { bool x; x = get_next_token err_msg; if (!x) { DEBUG(("GET_NEXT_TOKEN RETURN with token %s, position %ld in str %s\n", tok_start, tok_start-str, str)); close_token_strm(); ignore_rest_of_line(); return;}}
#define IS_NEXT_TOKEN(params) { if (!is_next_token params) { DEBUG(("IS_NEXT_TOKEN RETURN with token %s, position %ld in str %s\n", tok_start, tok_start-str, str)); close_token_strm(); ignore_rest_of_line(); return;}}
#define ERR_RETURN { DEBUG(("ERR RETURN with token %s, position %ld in str %s\n", tok_start, tok_start-str, str)); close_token_strm(); ignore_rest_of_line(); return; }
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
  //int reg_b;
  int reg_a;

  start_token_strm(str);

  uint16_t inst_code = 0;

  inst_code_frag = frag_more (2); /* Let's start a new fragment for the 16-bit instruction code */
  field_e_frag = NULL; /* We initially don't have field_e. If, during parsing we encouter an expression, we'll allocate a fragment here */

  DEBUG(("md_assemble: %s", str))

  GET_NEXT_TOKEN((_("Invalid instruction: not a single token is found ")));
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
  do {
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
    GET_NEXT_TOKEN((_("invalid store operation syntax ")));
    /* There are four formats we recognize here: {reg}; {expr}; {reg},{expr}; {expr},{reg} */
    if (parse_expression(tok_start))
      {
        inst_code |= 0x0800; /* Set the appropriate bit to signal the presence of an immediate offset */
        GET_NEXT_TOKEN((_("invalid store offset syntax")));
        if (strcmp(tok_start, ",") == 0)
          {
            /* We have the format of MEM[{expr},{reg}] = {reg} */
            GET_NEXT_TOKEN((_("invalid store offset syntax")));
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
        GET_NEXT_TOKEN((_("invalid store offset syntax")));
        if (strcmp(tok_start, ",") == 0)
          {
            GET_NEXT_TOKEN((_("invalid store offset syntax")));
            if (!parse_expression(tok_start))
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
    GET_NEXT_TOKEN((_("invalid store operation syntax ")));
    reg_d = parse_register_operand(tok_start, true);
    if (reg_d == -1)
      {
        as_bad(_("Invalid source register for store "));
        ERR_RETURN;
      }
    inst_code |= (reg_a & 0xf) << 4;
    /* special-case TPC stores: these only have a 48-bit variant */
    if (reg_d == BREW_REG_TPC)
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
  } while (false);

  /* All other operations have the form of {reg} = ... */
  reg_d = parse_register_operand(tok_start, true);
  if (reg_d == -1)
    {
      as_bad(_("Invalid target register for instruction"));
      ERR_RETURN;
    }
  IS_NEXT_TOKEN((ASSIGNMENT_STR, _("Invalid operation ")));
  GET_NEXT_TOKEN((_("Incomplete instructions")));


  /* Load operations */
  do {
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
    GET_NEXT_TOKEN((_("invalid load operation syntax ")));
    /* There are four formats we recognize here: {reg}; {expr}; {reg},{expr}; {expr},{reg} */
    if (parse_expression(tok_start))
      {
        inst_code |= 0x0800; /* Set the appropriate bit to signal the presence of an immediate offset */
        GET_NEXT_TOKEN((_("invalid load offset syntax")));
        if (strcmp(tok_start, ",") == 0)
          {
            /* We have the format of {reg} = MEM[{expr},{reg}]*/
            GET_NEXT_TOKEN((_("invalid load offset syntax")));
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
        GET_NEXT_TOKEN((_("invalid load offset syntax")));
        if (strcmp(tok_start, ",") == 0)
          {
            GET_NEXT_TOKEN((_("invalid load offset syntax")));
            if (!parse_expression(tok_start))
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
    if (reg_d == BREW_REG_TPC)
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
  } while (false);

  as_bad(_("Unrecognized instruction"));
  ERR_RETURN;

}

/* Turn a string in input_line_pointer into a floating point constant
   of type type, and store the appropriate bytes in *LITP.  The number
   of LITTLENUMS emitted is stored in *SIZEP .  An error message is
   returned, or NULL on OK.  */

const char *
md_atof (int type, char *litP, int *sizeP)
{
  int prec;
  LITTLENUM_TYPE words[4];
  char *t;
  int i;

  switch (type)
    {
    case 'f':
      prec = 2;
      break;

    case 'd':
      prec = 4;
      break;

    default:
      *sizeP = 0;
      return _("bad call to md_atof");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;

  *sizeP = prec * 2;

  for (i = prec - 1; i >= 0; i--)
    {
      md_number_to_chars (litP, (valueT) words[i], 2);
      litP += 2;
    }

  return NULL;
}

enum options
{
  OPTION_F = OPTION_MD_BASE,
  OPTION_NF
};

struct option md_longopts[] =
{
  { "F",           no_argument, NULL, OPTION_F},
  { "NF",          no_argument, NULL, OPTION_NF},
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
      abort ();
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
}
