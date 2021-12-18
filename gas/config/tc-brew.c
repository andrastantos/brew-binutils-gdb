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

#include <stdint.h>
#include "as.h"
#include "safe-ctype.h"
#include "opcode/brew.h"
#include "elf/brew.h"

extern const brew_opc_info_t brew_opc_info[128];

const char comment_chars[]        = "#";
const char line_separator_chars[] = ";";
const char line_comment_chars[]   = "#";

static int pending_reloc;
static htab_t opcode_hash_control;

const pseudo_typeS md_pseudo_table[] =
{
  {0, 0, 0}
};

const char FLT_CHARS[] = "rRsSfFdDxXpP";
const char EXP_CHARS[] = "eE";

/* TODO: this function is not used. That's because the MOXIE 10-bit relocation type is not used. */
/*static valueT md_chars_to_number (char * buf, int n);*/

/* Byte order.  */
extern int target_big_endian;

void
md_operand (expressionS *op __attribute__((unused)))
{
  /* Empty for now. */
}

/* This function is called once, at assembler startup time.  It sets
   up the hash table with all the opcodes in it, and also initializes
   some aliases for compatibility with other assemblers.  */

void
md_begin (void)
{
  int count;
  const brew_opc_info_t *opcode;
  opcode_hash_control = str_htab_create ();

  /* Insert names into hash table.  */
  for (count = 0, opcode = brew_form1_opc_info; count++ < 64; opcode++)
    str_hash_insert (opcode_hash_control, opcode->name, opcode, 0);

  for (count = 0, opcode = brew_form2_opc_info; count++ < 4; opcode++)
    str_hash_insert (opcode_hash_control, opcode->name, opcode, 0);

  for (count = 0, opcode = brew_form3_opc_info; count++ < 10; opcode++)
    str_hash_insert (opcode_hash_control, opcode->name, opcode, 0);

  bfd_set_arch_mach (stdoutput, TARGET_ARCH, 0);
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
   Registers are named as:
   $r0...$re (case insensitive) for unsigned scalar registers
   $s0...$se (case insensitive) for signed scalar register (aliases)
   $f0...$fe (case insensitive) for floating point register (aliases)
   $pc or $PC is also recognized and is an alias to $r0
   $tpc or $TPC is also recognized, but handled differently.

   Return value:
      register index, or BREW_REG_TPC for $tpc
      -1 in case of failure.
*/
static int
parse_register_operand (char *token, bool allow_tpc)
{
  int reg;
  int raw_reg;
  bool valid;

  if (token[0] != '$')
    {
      as_bad (_("expecting register"));
      return -1;
    }
  if (strcasecmp(token, "$PC") == 0)
    {
      return BREW_REG_PC;
    }
  if (allow_tpc && strcasecmp(token, "$TPC") == 0)
    {
      return BREW_REG_TPC;
    }
  /* Regular registers */
  valid = false;
  if (token[1] == 'r' || token[1] == 'R')
    {
      valid = True;
    }
  if (token[1] == 's' || token[1] == 'S')
    {
      valid = True;
      reg = BREW_REG_FLAG_SIGNED;
    }
  if (token[1] == 'f' || token[1] == 'F')
    {
      valid = True;
      reg = BREW_REG_FLAG_FLOAT;
    }
  if (!valid)
    {
      as_bad (_("illegal register number"));
      return -1;
    }
  raw_reg = parse_reg_index(token+2);
  if (raw_reg == -1)
    {
      as_bad (_("illegal register number"));
      return -1;
    }
  reg |= raw_reg;
  return reg;
}

static bool
char_in(char a, char *list)
{
  while (*list != 0)
    {
      if (a == *list)
        return true;
      ++list;
    }
  return false;
}

/* Returns the pointer to the begining (tok_start) and the end (tok_end)
   of the next token within str. Sets *tok_start to NULL if no more
   tokens are found. The string is in-place modified by putting a
   zero-termination into the token delimiter. The original character
   at the place of the end of the token is saved in tok_end_holder */
static void
get_next_token(char **tok_start, char **tok_end, char *tok_end_holder)
{
  /* Drop leading whitespace.  */
  while (**tok_start == ' ')
        *tok_start++;

  /* Find the end of the token */
  for (*toke_end = *tok_start;
       **tok_end && !is_end_of_line[**tok_end & 0xff] && !char_in(**tok_end, " []=,");
       *tok_end++);

  if (*tok_end == *tok_start)
    *tok_start = NULL;
    return;
  *tok_end_holder = **tok_end;
  **tok_end = 0;
}

#define GET_NEXT_TOKEN(err_msg) { \
  *tok_end = tok_end_holder; \
  tok_start = tok_end; \
  get_next_token(&tok_start, &tok_end, &tok_end_holder); \
  if (tok_start == NULL)\
    { \
      as_bad(_(err_msg)); \
      ignore_rest_of_line(); \
      return; \
    } \
}
#define GET_NEXT_TOKEN_OPT  { \
  *tok_end = tok_end_holder; \
  tok_start = tok_end; \
  get_next_token(&tok_start, &tok_end, &tok_end_holder);
#define IS_NEXT_TOKEN(expected_tok, err_msg) { \
  *tok_end = tok_end_holder; \
  tok_start = tok_end; \
  get_next_token(&tok_start, &tok_end, &tok_end_holder); \
  if (tok_start == NULL)\
    { \
      as_bad(_(err_msg)); \
      ignore_rest_of_line(); \
      return; \
    } \
  if (strcasecmp(tok_start, expected_tok) != 0)\
    { \
      as_bad(_(err_msg)); \
      ignore_rest_of_line(); \
      return; \
    } \
}
#define ERR_RETURN { if (tok_end != NULL) *tok_end = tok_end_holder; ignore_rest_of_line(); return; }
#define RETURN_16BIT(inst_code) { \
  if (tok_end != NULL) *tok_end = tok_end_holder; \
  md_number_to_chars (emit_strm, inst_code, 2); \
  dwarf2_emit_insn (2); \
 \
  get_next_token(str, &tok_start, &tok_end, &tok_end_holder); \
  if (tok_start != NULL) \
    as_warn (_("extra stuff on line ignored")); \
 \
  if (pending_reloc) \
    as_bad (_("Something forgot to clean up\n")); \
     return; \
}
#define RETURN_48BIT(inst_code, field_e) { \
  if (tok_end != NULL) *tok_end = tok_end_holder; \
  md_number_to_chars (emit_strm, inst_code, 2); \
  md_number_to_chars (emit_strm+2, field_e, 4); \
  dwarf2_emit_insn (6); \
 \
  get_next_token(str, &tok_start, &tok_end, &tok_end_holder); \
  if (tok_start != NULL) \
    as_warn (_("extra stuff on line ignored")); \
 \
  if (pending_reloc) \
    as_bad (_("Something forgot to clean up\n")); \
     return; \
}



/* This is the guts of the machine-dependent assembler.  STR points to
   a machine dependent instruction.  This function is supposed to emit
   the frags/bytes it assembles to.  */

void
md_assemble (char *str)
{
  char *tok_start;
  int reg_d;
  int reg_b;
  int reg_a;

  /* GET_NEXT_TOKEN and co. macros start by restoring the termination
     of the previous token and start searching from tok_end. So, we
     pretend there was something that just ended where our string starts. */
  char *tok_end = str;
  char tok_end_holder = str[0];

  char *emit_strm;
  char pend;

  uint16_t inst_code = 0;
  /*u_int32_t field_e = 0;*/

  /* Reserve 6 bytes for output */
  emit_strm = frag_more (6);

  GET_NEXT_TOKEN("Invalid instruction: not a single token is found ");
  /* Deal with simple instructions */
  if (strcasecmp(tok_start, "FILL") == 0)
    RETURN_16BIT(0x0000);
  if (strcasecmp(tok_start, "BREAK") == 0)
    RETURN_16BIT(0x0110);
  if (strcasecmp(tok_start, "SYSCALL") == 0)
    RETURN_16BIT(0x0220);
  if (strcasecmp(tok_start, "STU") == 0)
    RETURN_16BIT(0x0330);
  if (strcasecmp(tok_start, "SII") == 0)
    RETURN_16BIT(0x0440);
  if (strcasecmp(tok_start, "FENCE") == 0)
    RETURN_16BIT(0x0dd0);
  if (strcasecmp(tok_start, "WFENCE") == 0)
    RETURN_16BIT(0x0ee0);
  if (strcasecmp(tok_start, "WOI") == 0)
    RETURN_16BIT(0x1000);

  /* Store operations */
  do {
    if (strcasecmp(tok_start, "MEM") == 0)
      inst_code = 0xf700;
    else if (strcasecmp(tok_start, "MEM32") == 0)
      inst_code = 0xf700;
    else if (strcasecmp(tok_start, "MEM8") == 0)
      inst_code = 0xf500;
    else if (strcasecmp(tok_start, "MEM16") == 0)
      inst_code = 0xf600;
    else
      break;
    IS_NEXT_TOKEN("[", "invalid store operation syntax ");
     /* At this point we either have a register or an expression in the next token */
     // TODO: figure out how to deal with expressions and fixups!
    GET_NEXT_TOKEN("invalid store operation syntax ");
    reg_a = parse_register_operand(tok_start, false);
    if (reg_a == -1)
      {
        as_bad(_("Invalid base register for store "));
        ERR_RETURN;
      }
    IS_NEXT_TOKEN("]", "invalid store operation syntax ");
    IS_NEXT_TOKEN("=", "invalid store operation syntax ");
    GET_NEXT_TOKEN("invalid store operation syntax ");
    reg_d = parse_register_operand(tok_start, false);
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
        RETURN_48BIT(inst_code, 0);
      }
    inst_code |= (reg_d & 0xf) << 0;
    RETURN_16BIT(inst_code);
    // TODO: we need the 48-bit versions of the stores too!!
  } while (false);

  /* All other operations have the form of R_D = ... */
  reg_d = parse_register_operand(tok_start, true);
  IS_NEXT_TOKEN("=", "Invalid operation ");

  GET_NEXT_TOKEN("invalid operation syntax ");
  /* Loads */
  do {
    if (strcasecmp(tok_start, "MEM") == 0 || strcasecmp(tok_start, "MEM32") == 0)
      {
        inst_code = 0xf400;
      }
    else if (strcasecmp(tok_start, "MEM8") == 0)
      {
        if (reg_d & BREW_REG_FLAG_FLOAT != 0)
          ERR_RETURN("8-bit loads into float register is not supported")
        if (reg_d & BREW_REG_FLAG_SIGNED != 0)
          inst_code = 0xf000;
        else
          inst_code = 0xf100;
      }
    else if (strcasecmp(tok_start, "MEM16") == 0)
      {
        if (reg_d & BREW_REG_FLAG_FLOAT != 0)
          ERR_RETURN("16-bit loads into float register is not supported")
        if (reg_d & BREW_REG_FLAG_SIGNED != 0)
          inst_code = 0xf200;
        else
          inst_code = 0xf300;
      }
    else
      break;
    IS_NEXT_TOKEN("[", "invalid load operation syntax ");
     /* At this point we either have a register or an expression in the next token */
     // TODO: figure out how to deal with expressions and fixups!
    GET_NEXT_TOKEN("invalid load operation syntax ");
    reg_a = parse_register_operand(tok_start, false);
    if (reg_a == -1)
      {
        as_bad(_("Invalid base register for load "));
        ERR_RETURN;
      }
    IS_NEXT_TOKEN("]", "invalid load operation syntax ");
    inst_code |= (reg_a & 0xf) << 4;
    /* special-case TPC stores: these only have a 48-bit variant */
    if (reg_d == BREW_REG_TPC)
      {
        inst_code |= 0x0800;
        inst_code |= 0x000f; 
        RETURN_48BIT(inst_code, 0);
      }
    inst_code |= (reg_d & 0xf) << 0;
    RETURN_16BIT(inst_code);
    // TODO: we need the 48-bit versions of the stores too!!
  } while (false);























  /* Find the op code end.  */
  op_start = str;
  for (op_end = str;
       *op_end && !is_end_of_line[*op_end & 0xff] && *op_end != ' ';
       op_end++)
    nlen++;

  /* Temporarily 0-terminate the string after the opcode so the hash lookup works */
  pend = *op_end;
  *op_end = 0;
  /* Look up the opcode, if there is one */
  if (nlen == 0)
    as_bad (_("can't find opcode "));
  opcode = (brew_opc_info_t *) str_hash_find (opcode_hash_control, op_start);
  /* Restore original string (remove 0-termination) */
  *op_end = pend;

  if (opcode == NULL)
    {
      as_bad (_("unknown opcode %s"), op_start);
      return;
    }


  switch (opcode->itype)
    {
    case BREW_ABD:
      inst_code = opcode->opcode;
      while (ISSPACE (*op_end))
	op_end++;
      {
	int dest, src_a, src_b;
	dest = parse_register_operand (&op_end, BREW_NO_TPC);
	inst_code |= dest;
	if (*op_end != ',')
	  as_warn (_("expecting comma delimited register operands"));
	op_end++;
	src_a  = parse_register_operand (&op_end, BREW_NO_TPC);
	inst_code |= src_a << 4;
	if (*op_end != ',')
	  as_warn (_("expecting comma delimited register operands"));
	op_end++;
	src_b  = parse_register_operand (&op_end, BREW_NO_TPC);
	inst_code |= src_b << 8;
	while (ISSPACE (*op_end))
	  op_end++;
	if (*op_end != 0)
	  as_warn (_("extra stuff on line ignored"));
      }
      break;
      /****************************************************
       * LEGACY CODE STARTS HERE
       ***************************************************/
    case BREW_F2_A8V:
      inst_code = (1<<15) | (opcode->opcode << 12);
      while (ISSPACE (*op_end))
	op_end++;
      {
	expressionS arg;
	int reg;
	reg = parse_register_operand (&op_end, BREW_NO_TPC);
	inst_code += (reg << 8);
	if (*op_end != ',')
	  as_warn (_("expecting comma delimited register operands"));
	op_end++;
	op_end = parse_exp_save_ilp (op_end, &arg);
	fix_new_exp (frag_now,
		     ((p + (target_big_endian ? 1 : 0)) - frag_now->fr_literal),
		     1,
		     &arg,
		     0,
		     BFD_RELOC_8);
      }
      break;
    case BREW_F1_AB:
      inst_code = opcode->opcode << 8;
      while (ISSPACE (*op_end))
	op_end++;
      {
	int dest, src;
	dest = parse_register_operand (&op_end, BREW_NO_TPC);
	if (*op_end != ',')
	  as_warn (_("expecting comma delimited register operands"));
	op_end++;
	src  = parse_register_operand (&op_end, BREW_NO_TPC);
	inst_code += (dest << 4) + src;
	while (ISSPACE (*op_end))
	  op_end++;
	if (*op_end != 0)
	  as_warn (_("extra stuff on line ignored"));
      }
      break;
    case BREW_F1_A4:
      inst_code = opcode->opcode << 8;
      while (ISSPACE (*op_end))
	op_end++;
      {
	expressionS arg;
	char *where;
	int regnum;

 	regnum = parse_register_operand (&op_end, BREW_NO_TPC);
	while (ISSPACE (*op_end))
	  op_end++;

	inst_code += (regnum << 4);

	if (*op_end != ',')
	  {
	    as_bad (_("expecting comma delimited operands"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;

	op_end = parse_exp_save_ilp (op_end, &arg);
	where = frag_more (4);
	fix_new_exp (frag_now,
		     (where - frag_now->fr_literal),
		     4,
		     &arg,
		     0,
		     BFD_RELOC_32);
      }
      break;
    case BREW_F1_M:
    case BREW_F1_4:
      inst_code = opcode->opcode << 8;
      while (ISSPACE (*op_end))
	op_end++;
      {
	expressionS arg;
	char *where;

	op_end = parse_exp_save_ilp (op_end, &arg);
	where = frag_more (4);
	fix_new_exp (frag_now,
		     (where - frag_now->fr_literal),
		     4,
		     &arg,
		     0,
		     BFD_RELOC_32);
      }
      break;
    case BREW_F1_NARG:
      inst_code = opcode->opcode << 8;
      while (ISSPACE (*op_end))
	op_end++;
      if (*op_end != 0)
	as_warn (_("extra stuff on line ignored"));
      break;
    case BREW_F1_A:
      inst_code = opcode->opcode << 8;
      while (ISSPACE (*op_end))
	op_end++;
      {
	int reg;
	reg = parse_register_operand (&op_end, BREW_NO_TPC);
	while (ISSPACE (*op_end))
	  op_end++;
	if (*op_end != 0)
	  as_warn (_("extra stuff on line ignored"));
	inst_code += (reg << 4);
      }
      break;
    case BREW_F1_ABi:
      inst_code = opcode->opcode << 8;
      while (ISSPACE (*op_end))
	op_end++;
      {
	int a, b;
	a = parse_register_operand (&op_end, BREW_NO_TPC);
	if (*op_end != ',')
	  as_warn (_("expecting comma delimited register operands"));
	op_end++;
	if (*op_end != '(')
	  {
	    as_bad (_("expecting indirect register `($rA)'"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;
	b = parse_register_operand (&op_end, BREW_NO_TPC);
	if (*op_end != ')')
	  {
	    as_bad (_("missing closing parenthesis"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;
	inst_code += (a << 4) + b;
	while (ISSPACE (*op_end))
	  op_end++;
	if (*op_end != 0)
	  as_warn (_("extra stuff on line ignored"));
      }
      break;
    case BREW_F1_AiB:
      inst_code = opcode->opcode << 8;
      while (ISSPACE (*op_end))
	op_end++;
      {
	int a, b;
	if (*op_end != '(')
	  {
	    as_bad (_("expecting indirect register `($rA)'"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;
	a = parse_register_operand (&op_end, BREW_NO_TPC);
	if (*op_end != ')')
	  {
	    as_bad (_("missing closing parenthesis"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;
	if (*op_end != ',')
	  as_warn (_("expecting comma delimited register operands"));
	op_end++;
	b = parse_register_operand (&op_end, BREW_NO_TPC);
	inst_code += (a << 4) + b;
	while (ISSPACE (*op_end))
	  op_end++;
	if (*op_end != 0)
	  as_warn (_("extra stuff on line ignored"));
      }
      break;
    case BREW_F1_4A:
      inst_code = opcode->opcode << 8;
      while (ISSPACE (*op_end))
	op_end++;
      {
	expressionS arg;
	char *where;
	int a;

	op_end = parse_exp_save_ilp (op_end, &arg);
	where = frag_more (4);
	fix_new_exp (frag_now,
		     (where - frag_now->fr_literal),
		     4,
		     &arg,
		     0,
		     BFD_RELOC_32);

	if (*op_end != ',')
	  {
	    as_bad (_("expecting comma delimited operands"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;

 	a = parse_register_operand (&op_end, BREW_NO_TPC);
	while (ISSPACE (*op_end))
	  op_end++;
	if (*op_end != 0)
	  as_warn (_("extra stuff on line ignored"));

	inst_code += (a << 4);
      }
      break;
    case BREW_F1_ABi2:
      inst_code = opcode->opcode << 8;
      while (ISSPACE (*op_end))
	op_end++;
      {
	expressionS arg;
	char *offset;
	int a, b;

 	a = parse_register_operand (&op_end, BREW_NO_TPC);
	while (ISSPACE (*op_end))
	  op_end++;

	if (*op_end != ',')
	  {
	    as_bad (_("expecting comma delimited operands"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;

	op_end = parse_exp_save_ilp (op_end, &arg);
	offset = frag_more (2);
	fix_new_exp (frag_now,
		     (offset - frag_now->fr_literal),
		     2,
		     &arg,
		     0,
		     BFD_RELOC_16);

	if (*op_end != '(')
	  {
	    as_bad (_("expecting indirect register `($rX)'"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;
	b = parse_register_operand (&op_end, BREW_NO_TPC);
	if (*op_end != ')')
	  {
	    as_bad (_("missing closing parenthesis"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;

	while (ISSPACE (*op_end))
	  op_end++;
	if (*op_end != 0)
	  as_warn (_("extra stuff on line ignored"));

	inst_code += (a << 4) + b;
      }
      break;
    case BREW_F1_AiB2:
      inst_code = opcode->opcode << 8;
      while (ISSPACE (*op_end))
	op_end++;
      {
	expressionS arg;
	char *offset;
	int a, b;

	op_end = parse_exp_save_ilp (op_end, &arg);
	offset = frag_more (2);
	fix_new_exp (frag_now,
		     (offset - frag_now->fr_literal),
		     2,
		     &arg,
		     0,
		     BFD_RELOC_16);

	if (*op_end != '(')
	  {
	    as_bad (_("expecting indirect register `($rX)'"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;
	a = parse_register_operand (&op_end, BREW_NO_TPC);
	if (*op_end != ')')
	  {
	    as_bad (_("missing closing parenthesis"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;

	if (*op_end != ',')
	  {
	    as_bad (_("expecting comma delimited operands"));
	    ignore_rest_of_line ();
	    return;
	  }
	op_end++;

 	b = parse_register_operand (&op_end, BREW_NO_TPC);
	while (ISSPACE (*op_end))
	  op_end++;

	while (ISSPACE (*op_end))
	  op_end++;
	if (*op_end != 0)
	  as_warn (_("extra stuff on line ignored"));

	inst_code += (a << 4) + b;
      }
      break;
    case BREW_F2_NARG:
      inst_code = opcode->opcode << 12;
      while (ISSPACE (*op_end))
	op_end++;
      if (*op_end != 0)
	as_warn (_("extra stuff on line ignored"));
      break;
    case BREW_F3_PCREL:
      inst_code = (3<<14) | (opcode->opcode << 10);
      while (ISSPACE (*op_end))
	op_end++;
      {
	expressionS arg;

	op_end = parse_exp_save_ilp (op_end, &arg);
	/* TODO: this is not something we can do at the moment!!!! */
	/*fix_new_exp (frag_now,
		     (p - frag_now->fr_literal),
		     2,
		     &arg,
		     true,
		     BFD_RELOC_MOXIE_10_PCREL);*/
      }
      break;
    case BREW_BAD:
      inst_code = 0;
      while (ISSPACE (*op_end))
	op_end++;
      if (*op_end != 0)
	as_warn (_("extra stuff on line ignored"));
      break;
    default:
      abort ();
    }

  md_number_to_chars (p, inst_code, 2);
  dwarf2_emit_insn (2);

  while (ISSPACE (*op_end))
    op_end++;

  if (*op_end != 0)
    as_warn (_("extra stuff on line ignored"));

  if (pending_reloc)
    as_bad (_("Something forgot to clean up\n"));
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
  OPTION_EB = OPTION_MD_BASE,
  OPTION_EL,
};

struct option md_longopts[] =
{
  { "EB",          no_argument, NULL, OPTION_EB},
  { "EL",          no_argument, NULL, OPTION_EL},
  { NULL,          no_argument, NULL, 0}
};

size_t md_longopts_size = sizeof (md_longopts);

const char *md_shortopts = "";

int
md_parse_option (int c ATTRIBUTE_UNUSED, const char *arg ATTRIBUTE_UNUSED)
{
  switch (c)
    {
    case OPTION_EB:
      target_big_endian = 1;
      break;
    case OPTION_EL:
      target_big_endian = 0;
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
  -EB                     assemble for a big endian system (default)\n\
  -EL                     assemble for a little endian system\n"));
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
      if (target_big_endian)
	{
	  buf[0] = val >> 24;
	  buf[1] = val >> 16;
	  buf[2] = val >> 8;
	  buf[3] = val >> 0;
	}
      else
	{
	  buf[3] = val >> 24;
	  buf[2] = val >> 16;
	  buf[1] = val >> 8;
	  buf[0] = val >> 0;
	}
      buf += 4;
      break;

    case BFD_RELOC_16:
      if (target_big_endian)
	{
	  buf[0] = val >> 8;
	  buf[1] = val >> 0;
	}
      else
	{
	  buf[1] = val >> 8;
	  buf[0] = val >> 0;
	}
      buf += 2;
      break;

    case BFD_RELOC_8:
      *buf++ = val;
      break;

    /* TODO: we probably don't need this here: we don't have any 10-bit PC-relative relocation. */
    /*       if we added 32-bit PC-relative ones, maybe something like this might be useful?    */
    /*
    case BFD_RELOC_MOXIE_10_PCREL:
      if (!val)
	break;
      if (val < -1024 || val > 1022)
	as_bad_where (fixP->fx_file, fixP->fx_line,
                      _("pcrel too far BFD_RELOC_MOXIE_10"));
      // 11 bit offset even numbered, so we remove right bit.
      val >>= 1;
      newval = md_chars_to_number (buf, 2);
      newval |= val & 0x03ff;
      md_number_to_chars (buf, newval, 2);
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
  if (target_big_endian)
    number_to_chars_bigendian (ptr, use, nbytes);
  else
    number_to_chars_littleendian (ptr, use, nbytes);
}

/* Convert from target byte order to host byte order.  */
/* TODO: this function is not used. That's because the MOXIE 10-bit relocation type is not used. */
/*
static valueT
md_chars_to_number (char * buf, int n)
{
  valueT result = 0;
  unsigned char * where = (unsigned char *) buf;

  if (target_big_endian)
    {
      while (n--)
	{
	  result <<= 8;
	  result |= (*where++ & 255);
	}
    }
  else
    {
      while (n--)
	{
	  result <<= 8;
	  result |= (where[n] & 255);
	}
    }

  return result;
}
*/

/* Generate a machine-dependent relocation.  */
arelent *
tc_gen_reloc (asection *section ATTRIBUTE_UNUSED, fixS *fixP)
{
  arelent *relP;
  bfd_reloc_code_real_type code;

  switch (fixP->fx_r_type)
    {
    case BFD_RELOC_32:
      code = fixP->fx_r_type;
      break;
    /* We don't have this type of relocation */
    /*
    case BFD_RELOC_MOXIE_10_PCREL:
      code = fixP->fx_r_type;
      break;
    */
    default:
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    _("Semantics error.  This type of operand can not be relocated, it must be an assembly-time constant"));
      return 0;
    }

  relP = XNEW (arelent);
  relP->sym_ptr_ptr = XNEW (asymbol *);
  *relP->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  relP->address = fixP->fx_frag->fr_address + fixP->fx_where;

  relP->addend = fixP->fx_offset;

  /* This is the standard place for KLUDGEs to work around bugs in
     bfd_install_relocation (first such note in the documentation
     appears with binutils-2.8).

     That function bfd_install_relocation does the wrong thing with
     putting stuff into the addend of a reloc (it should stay out) for a
     weak symbol.  The really bad thing is that it adds the
     "segment-relative offset" of the symbol into the reloc.  In this
     case, the reloc should instead be relative to the symbol with no
     other offset than the assembly code shows; and since the symbol is
     weak, any local definition should be ignored until link time (or
     thereafter).
     To wit:  weaksym+42  should be weaksym+42 in the reloc,
     not weaksym+(offset_from_segment_of_local_weaksym_definition)

     To "work around" this, we subtract the segment-relative offset of
     "known" weak symbols.  This evens out the extra offset.

     That happens for a.out but not for ELF, since for ELF,
     bfd_install_relocation uses the "special function" field of the
     howto, and does not execute the code that needs to be undone.  */

  if (OUTPUT_FLAVOR == bfd_target_aout_flavour
      && fixP->fx_addsy && S_IS_WEAK (fixP->fx_addsy)
      && ! bfd_is_und_section (S_GET_SEGMENT (fixP->fx_addsy)))
    {
      relP->addend -= S_GET_VALUE (fixP->fx_addsy);
    }

  relP->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (! relP->howto)
    {
      const char *name;

      name = S_GET_NAME (fixP->fx_addsy);
      if (name == NULL)
	name = _("<unknown>");
      as_fatal (_("Cannot generate relocation type for symbol %s, code %s"),
		name, bfd_get_reloc_code_name (code));
    }

  return relP;
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
