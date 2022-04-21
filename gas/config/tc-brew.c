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

static int bit_idx_to_field_a[] = {
  0,   1,   2,   3,   4,   5,   6,   7,
  8,   9,  -1,  -1,  -1,  -1,  10,  11,
 12,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
 -1,  -1,  -1,  -1,  -1,  -1,  13,  14
};

const pseudo_typeS md_pseudo_table[] =
{
  {0, 0, 0}
};

/* TOKEN PARSERS */
/*****************/

const char FLT_CHARS[] = "rRsSfFdDxXpP";
const char EXP_CHARS[] = "eE";

static bool
parse_int(const brew_lexer_tokenS *first_token, const brew_lexer_tokenS *last_token, int32_t *int_result)
{
  bool ret_val = false;
  bool negative = false;
  size_t base = 10;
  char *digit = (char *)(first_token->start); // We are temporarily modifying the string to be zero-terminated
  char *end_num;
  char *tok_end = NULL;
  char terminator = 0;
  long int converted_num;

  if (last_token->type != T_NULL)
    {
      size_t len;
      len = last_token->start + last_token->len - first_token->start;
      gas_assert(len > 0);
      if (len == 1)
        return false;
      tok_end = (char *)(first_token->start + len - 1);
      terminator = *tok_end;
      *tok_end = 0;
    }
  while (*digit == '-' || *digit == '+' || ISSPACE(*digit))
    {
      if (*digit == '-')
        negative = !negative;
      ++digit;
    }
  if (digit[0] == 0)
    goto DONE;

  // Since we test for specific characters in order
  // we don't have to worry about running off at the
  // end of the string.
  if (digit[0] == '0')
    {
      if (digit[1] == 'x')
        {
          base = 16;
          digit += 2;
        }
      else
        {
          base = 8;
          ++digit;
        }
    }

  converted_num = strtol(digit, &end_num, base);
  if (end_num == NULL || *end_num != 0)
    goto DONE;
  if (negative)
    converted_num = -converted_num;
  *int_result = converted_num;
  ret_val = true;
DONE:
  if (last_token->type != T_NULL)
    {
      *tok_end = terminator;
    }
  return ret_val;
}

// Parse an expression and then restore the input line pointer.
static char *
parse_exp_save_ilp(char *s, expressionS *op)
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
//   - it should support FP16 expressions (that would be format char 'h')
// Parses a 32-bit expression and puts it in the last four bytes of the fragment buffer
// Returns true if an expression is found, false if not.
static bool
parse_expression(
  const brew_lexer_tokenS *first_token,
  const brew_lexer_tokenS *last_token,
  char *fragment_data,
  size_t fragment_ofs,
  size_t fragment_size,
  bool support_float,
  enum bfd_reloc_code_real reloc_type // BFD_RELOC_BREW_PCREL16, BFD_RELOC_BREW_NEG16, BFD_RELOC_16, BFD_RELOC_BREW_NEG32, BFD_RELOC_32
) {

  bool ret_val = false;
  char *parse_end;
  char *expression_str;
  char terminator = 0;
  size_t expression_len = 0;

  // test for vector expressions in the form of { <item>, <item> ... }
  if (first_token->type == T_LCURLY && last_token->type == T_RCURLY) {
    as_bad(_("vector constants are not yet supported"));
    gas_assert(false);
  }

  // temporarily 0-terminate the expression (in case last token is T_NULL, string is already zero-terminated)
  expression_str = (char *)(first_token->start); // casting away const since we temporarily zero-terminate
  if (last_token->type != T_NULL)
    {
      expression_len = last_token->start + last_token->len - expression_str;
      gas_assert(expression_len > 0);
      if (expression_len == 1)
        return false;
      terminator = expression_str[expression_len-1];
      expression_str[expression_len-1] = 0;
    }

  // GAS also doesn't support any floating point expressions. We need that for float immediates, so let's test for them
  // NOTE: this still doesn't support floating point expressions, such as 1.3+2.6,
  //       but that's not supported in GAS anywhere, not even in .float
  if (support_float && fragment_size == 4)
    {
      LITTLENUM_TYPE float_store[8]; // We really shouldn't store more than 4 bytes, but we can only test for that after the call returns. So, oversize the buffer to make sure we won't overflow

      parse_end = atof_ieee(expression_str, 'f', float_store);
      if (parse_end != NULL && *parse_end == 0)
        {
          DEBUG("float parser for %s returns with 0x%04x 0x%04x", expression_str, float_store[0], float_store[1]);
          int float_part = fragment_size / 2; // float_store is in 16-bit entities
          char *frag_ptr = fragment_data + fragment_ofs;
          LITTLENUM_TYPE *float_ptr;
          for (float_ptr = float_store + float_part; float_part--;)
            {
              md_number_to_chars(frag_ptr, (valueT)(*--float_ptr), sizeof(LITTLENUM_TYPE));
              frag_ptr += sizeof (LITTLENUM_TYPE);
            }
          ret_val = true;
          goto DONE;
        }
    }

  expressionS expression;
  char *end_expr;
  DEBUG(">> parsing expression (%p) %s", expression_str, expression_str);
  end_expr = parse_exp_save_ilp(expression_str, &expression);
  DEBUG(">> returned %p", end_expr);
  if (*end_expr != 0)
    goto DONE;
  const char *old_fr_literal = frag_now->fr_literal; // this is just for debug
  printf("where: %lx, offset: %ld, size: %ld\n", fragment_ofs + (fragment_data - frag_now->fr_literal), fragment_ofs, fragment_size);
  fix_new_exp(
    frag_now,
    fragment_ofs + (fragment_data - frag_now->fr_literal),
    fragment_size - fragment_ofs,
    &expression,
    reloc_type == BFD_RELOC_BREW_PCREL16, // this is the only pc-relative relocation we're supporting at the moment
    reloc_type);
  DEBUG("frag_now->fr_literal changed from %p to %p (delta: %ld)", old_fr_literal, frag_now->fr_literal, frag_now->fr_literal - old_fr_literal);
  ret_val = true;
DONE:
  if (last_token->type != T_NULL)
    {
      expression_str[expression_len-1] = terminator;
    }
  return ret_val;
}


// NOTE:
//   When get a 'fragment' using frag_more,
//   we are committing that many bytes into
//   the object file.
//   When we call dwrf2_emit_insn, we only
//   emit the line number info (if any) for
//   the debug info (as the prefix dwarf2
//   suggests.

#define A_PROLOG(_insn_len) \
  uint16_t insn_code; \
  size_t insn_len = _insn_len; \
  char *frag = frag_more(insn_len); \
  frag_now->fr_type = rs_fill

#define A_RETURN() { \
  md_number_to_chars(frag, insn_code, 2); \
  dwarf2_emit_insn(insn_len); \
  return A_OK; \
}

#define A_PROLOG_EXT(_insn_len) \
  uint16_t ext_code; \
  uint16_t insn_code; \
  size_t insn_len = _insn_len; \
  char *frag = frag_more(insn_len); \
  frag_now->fr_type = rs_fill

#define A_RETURN_EXT() { \
  md_number_to_chars(frag, ext_code, 2); \
  md_number_to_chars(frag+2, insn_code, 2); \
  dwarf2_emit_insn(insn_len); \
  return A_OK; \
}


#define FIELD_A(value) ((value & 0xf) << 0)
#define FIELD_B(value) ((value & 0xf) << 4)
#define FIELD_C(value) ((value & 0xf) << 8)
#define FIELD_D(value) ((value & 0xf) << 12)

enum ACTION_RESULT {
  A_OK,
  A_ERR,
  A_NOT_FOUND = -1
};

static size_t tslen(const brew_parser_tokenS *tokens)
{
  size_t len = 0;
  while (tokens->parser_token != T_NULL)
    {
      ++tokens;
      ++len;
    }
  return len;
}

static bool tscheck(const brew_parser_tokenS *tokens, size_t expected_len)
{
  if (tslen(tokens) != expected_len)
    return false;
  while (tokens->parser_token != T_NULL)
    {
      if (tokens->parser_token > 0)
        {
          if (tokens->first_lexer_token != tokens->last_lexer_token)
            return false;
          if (tokens->parser_token != tokens->first_lexer_token->type)
            return false;
        }
      ++tokens;
    }
  return true;
}

// boiler-plate code for actions
#define A_CHECK(expected_len) \
  DEBUG_BEGIN \
  brew_dump_parsed_tokens(stderr, tokens); \
  DEBUG_END \
  gas_assert(tscheck(tokens, expected_len))


static int action_insn(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(2);
  A_CHECK(1);

  gas_assert(tokens[0].parser_token == T_INSN);
  insn_code = tokens[0].first_lexer_token->sub_type;
  A_RETURN();
}

static int action_swi(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  int32_t swi_idx;
  A_PROLOG(2);
  A_CHECK(2);

  gas_assert(tokens[1].parser_token == ~T_NULL);

  if (!parse_int(tokens[1].first_lexer_token, tokens[1].last_lexer_token, &swi_idx))
    {
      as_bad(_("Can't parse SWI index"));
      return A_ERR;
    }
  if (swi_idx < 0 || swi_idx > 7)
    {
      as_bad(_("Invalid SWI index"));
      return A_ERR;
    }
  insn_code =
    FIELD_D(swi_idx) |
    FIELD_C(0x0) |
    FIELD_B(0x0) |
    FIELD_A(0x0);
  A_RETURN();
}

static int action_move_reg_to_pc(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(2);
  A_CHECK(3);

  gas_assert(tokens[2].parser_token == T_REG);
  gas_assert(tokens[0].parser_token == T_PC);
  insn_code =
    FIELD_D(tokens[2].first_lexer_token->sub_type) |
    FIELD_C(0x0) |
    FIELD_B(0x0) |
    FIELD_A(tokens[2].first_lexer_token->sub_type == ST_PC_PC ? 0x2 : 0x3);
  A_RETURN();
}

static bool mem_subtype_to_opcode_ld(int sub_type, int *op_code)
{
  switch (sub_type)
    {
      case ST_MEM_8:   *op_code = 0x4; break;
      case ST_MEM_16:  *op_code = 0x5; break;
      case ST_MEM_32:  *op_code = 0x6; break;
      case ST_MEM_LL:  *op_code = 0x7; break;
      case ST_SMEM_8:  *op_code = 0xc; break;
      case ST_SMEM_16: *op_code = 0xd; break;
      default:
        return false;
    }
  return true;
}

static bool mem_subtype_to_opcode_st(int sub_type, bool is_invalidate, int *op_code)
{
  if (is_invalidate)
    {
      if (sub_type != ST_MEM_INV)
        return false;
      *op_code = 0xe;
      return true;
    }
  switch (sub_type)
    {
      case ST_MEM_8:   *op_code = 0x8; break;
      case ST_SMEM_8:  *op_code = 0x8; break;
      case ST_SMEM_16: *op_code = 0x9; break;
      case ST_MEM_16:  *op_code = 0x9; break;
      case ST_MEM_32:  *op_code = 0xa; break;
      case ST_MEM_SR:  *op_code = 0xb; break;
      default:
        return false;
    }
  return true;
}

static int action_load_reg(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_pc_target;
  A_PROLOG(2);
  A_CHECK(6);

  is_pc_target = tokens[0].parser_token == T_PC;
  gas_assert(tokens[0].parser_token == T_REG || is_pc_target);
  gas_assert(tokens[2].parser_token == T_MEM);
  gas_assert(tokens[4].parser_token == T_REG);
  if (is_pc_target)
    {
      insn_code =
        FIELD_D(tokens[0].first_lexer_token->sub_type == ST_PC_PC ? 0x2 : 0x3) |
        FIELD_C(0xe) |
        FIELD_B(0xe) |
        FIELD_A(tokens[4].first_lexer_token->sub_type);
    }
  else
    {
      int op_code;
      if (!mem_subtype_to_opcode_ld(tokens[2].first_lexer_token->sub_type, &op_code))
        {
          as_bad(_("Invalid memory reference type for load"));
          return A_ERR;
        }
      insn_code =
        FIELD_D(tokens[0].first_lexer_token->sub_type) |
        FIELD_C(0xe) |
        FIELD_B(op_code) |
        FIELD_A(tokens[4].first_lexer_token->sub_type);
    }
  A_RETURN();
}

static int action_load_reg_ofs(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_pc_target;
  bool is_tiny;
  const brew_parser_tokenS *offs;

  is_tiny = (tokens[6].parser_token == T_TINY);

  A_PROLOG(is_tiny ? 2 : 4);
  A_CHECK(is_tiny ? 9 : 8);

  offs = is_tiny ? &tokens[7] : &tokens[6];

  is_pc_target = tokens[0].parser_token == T_PC;
  gas_assert(tokens[0].parser_token == T_REG || is_pc_target);
  gas_assert(tokens[2].parser_token == T_MEM);
  gas_assert(tokens[4].parser_token == T_REG);
  gas_assert(offs->parser_token == ~T_RBRACKET);
  if (is_tiny && tokens[5].first_lexer_token->sub_type == ST_MINUS)
    {
      as_bad(_("Tiny offsets only support '+'. Encode negation after the 'tiny' marker"));
    }
  if (!parse_expression(is_tiny ? offs->first_lexer_token : (offs-1)->first_lexer_token, offs->last_lexer_token, frag, is_tiny ? 0 : 2, insn_len, false, is_tiny ? BFD_RELOC_BREW_7 : BFD_RELOC_16))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }
  if (is_pc_target)
    {
      insn_code =
        FIELD_D(tokens[0].first_lexer_token->sub_type == ST_PC_PC ? 0x2 : 0x3) |
        FIELD_C(0xf) |
        FIELD_B(0xe) |
        FIELD_A(tokens[4].first_lexer_token->sub_type);
    }
  else if (is_tiny)
    {
      switch (tokens[4].first_lexer_token->sub_type)
        {
          case BREW_REG_FP:
          case BREW_REG_SP:
          break;
          default:
            as_bad(_("Tiny loads only support $sp and $fp as base registers"));
            return A_ERR;
        }
      insn_code =
        FIELD_D(tokens[0].first_lexer_token->sub_type) |
        FIELD_C(0xd) |
        FIELD_B(0x0) |
        FIELD_A(tokens[4].first_lexer_token->sub_type & 1);
    }
  else
    {
      int op_code;
      if (!mem_subtype_to_opcode_ld(tokens[2].first_lexer_token->sub_type, &op_code))
        {
          as_bad(_("Invalid memory reference type for load"));
          return A_ERR;
        }
      insn_code =
        FIELD_D(tokens[0].first_lexer_token->sub_type) |
        FIELD_C(0xf) |
        FIELD_B(op_code) |
        FIELD_A(tokens[4].first_lexer_token->sub_type);
    }
  A_RETURN();
}

static int action_load_ofs(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_pc_target;
  A_PROLOG(6);
  A_CHECK(6);

  is_pc_target = tokens[0].parser_token == T_PC;
  gas_assert(tokens[0].parser_token == T_REG || is_pc_target);
  gas_assert(tokens[2].parser_token == T_MEM);
  gas_assert(tokens[4].parser_token == ~T_RBRACKET);
  if (!parse_expression(tokens[4].first_lexer_token, tokens[4].last_lexer_token, frag, 2, insn_len, false, BFD_RELOC_32))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }

  if (is_pc_target)
    {
      insn_code =
        FIELD_D(tokens[0].first_lexer_token->sub_type == ST_PC_PC ? 0x2 : 0x3) |
        FIELD_C(0xf) |
        FIELD_B(0xe) |
        FIELD_A(0xf);
    }
  else
    {
      int op_code;
      if (!mem_subtype_to_opcode_ld(tokens[2].first_lexer_token->sub_type, &op_code))
        {
          as_bad(_("Invalid memory reference type for load"));
          return A_ERR;
        }
      insn_code =
        FIELD_D(tokens[0].first_lexer_token->sub_type) |
        FIELD_C(0xf) |
        FIELD_B(op_code) |
        FIELD_A(0xf);
    }

  A_RETURN();
}

static int action_store_reg(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_invalidate;
  int op_code;
  A_PROLOG(2);
  is_invalidate = tokens[4].parser_token == T_NULL;
  A_CHECK(is_invalidate ? 4 : 6);

  gas_assert(tokens[0].parser_token == T_MEM);
  gas_assert(tokens[2].parser_token == T_REG);
  gas_assert(is_invalidate || (tokens[5].parser_token == T_REG));

  if (!mem_subtype_to_opcode_st(tokens[0].first_lexer_token->sub_type, is_invalidate, &op_code))
    {
      as_bad(_("Invalid memory reference type for store"));
      return A_ERR;
    }
  insn_code =
    FIELD_D(is_invalidate ? 1 : tokens[5].first_lexer_token->sub_type) |
    FIELD_C(0xe) |
    FIELD_B(op_code) |
    FIELD_A(tokens[2].first_lexer_token->sub_type);
  A_RETURN();
}

static int action_store_reg_ofs(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_invalidate;
  bool is_tiny;
  int op_code;
  const brew_parser_tokenS *offs;
  const brew_parser_tokenS *src;

  is_tiny = (tokens[4].parser_token == T_TINY);
  is_invalidate = tokens[6].parser_token == T_NULL;

  A_PROLOG(is_tiny ? 2 : 4);
  A_CHECK(is_tiny ? 9 : is_invalidate ? 6 : 8);

  offs = is_tiny ? &tokens[5] : &tokens[4];
  src = is_tiny ? &tokens[8] : is_invalidate ? NULL : &tokens[7];

  gas_assert(tokens[0].parser_token == T_MEM);
  gas_assert(tokens[2].parser_token == T_REG);
  gas_assert(offs->parser_token == ~T_RBRACKET);
  gas_assert(is_invalidate || (src->parser_token == T_REG));
  if (is_tiny && tokens[5].first_lexer_token->sub_type == ST_MINUS)
    {
      as_bad(_("Tiny offsets only support '+'. Encode negation after the 'tiny' marker"));
    }
  if (!parse_expression(is_tiny ? offs->first_lexer_token : (offs-1)->first_lexer_token, offs->last_lexer_token, frag, is_tiny ? 0 : 2, insn_len, false, is_tiny ? BFD_RELOC_BREW_7 : BFD_RELOC_16))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }
  if (is_tiny)
    {
      switch (tokens[2].first_lexer_token->sub_type)
        {
          case BREW_REG_FP:
          case BREW_REG_SP:
          break;
          default:
            as_bad(_("Tiny stores only support $fp and $sp as base registers"));
            return A_ERR;
        }
      insn_code =
        FIELD_D(src->first_lexer_token->sub_type) |
        FIELD_C(0xc) |
        FIELD_B(0x0) |
        FIELD_A(tokens[2].first_lexer_token->sub_type & 1);
    }
  else
    {
      if (!mem_subtype_to_opcode_st(tokens[0].first_lexer_token->sub_type, is_invalidate, &op_code))
        {
          as_bad(_("Invalid memory reference type for store"));
          return A_ERR;
        }
      insn_code =
        FIELD_D(is_invalidate ? 1 : src->first_lexer_token->sub_type) |
        FIELD_C(0xf) |
        FIELD_B(op_code) |
        FIELD_A(tokens[2].first_lexer_token->sub_type);
    }
  A_RETURN();
}

static int action_store_ofs(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_invalidate;
  int op_code;
  A_PROLOG(6);
  is_invalidate = tokens[4].parser_token == T_NULL;
  A_CHECK(is_invalidate ? 4 : 6);

  gas_assert(tokens[0].parser_token == T_MEM);
  gas_assert(tokens[2].parser_token == ~T_RBRACKET);
  gas_assert(is_invalidate || (tokens[5].parser_token == T_REG));

  if (!parse_expression(tokens[2].first_lexer_token, tokens[2].last_lexer_token, frag, 2, insn_len, false, BFD_RELOC_32))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }

  if (!mem_subtype_to_opcode_st(tokens[0].first_lexer_token->sub_type, is_invalidate, &op_code))
    {
      as_bad(_("Invalid memory reference type for store"));
      return A_ERR;
    }
  insn_code =
    FIELD_D(is_invalidate ? 1 : tokens[5].first_lexer_token->sub_type) |
    FIELD_C(0xf) |
    FIELD_B(op_code) |
    FIELD_A(0xf);

  A_RETURN();
}

static int action_move_imm_to_pc(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(6);
  A_CHECK(3);

  gas_assert(tokens[0].parser_token == T_PC);
  gas_assert(tokens[2].parser_token == ~T_NULL);
  if (!parse_expression(tokens[2].first_lexer_token, tokens[2].last_lexer_token, frag, 2, insn_len, false, BFD_RELOC_32))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }
  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type == ST_PC_PC ? 0x2 : 0x3) |
    FIELD_C(0x0) |
    FIELD_B(0xe) |
    FIELD_A(0xf);
  A_RETURN();
}

static int action_move_short_imm_to_pc(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(4);
  A_CHECK(4);

  gas_assert(tokens[0].parser_token == T_PC);
  gas_assert(tokens[3].parser_token == ~T_NULL);
  if (!parse_expression(tokens[3].first_lexer_token, tokens[3].last_lexer_token, frag, 2, insn_len, false, BFD_RELOC_16))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }
  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type == ST_PC_PC ? 0x2 : 0x3) |
    FIELD_C(0x0) |
    FIELD_B(0xf) |
    FIELD_A(0xe);
  A_RETURN();
}

static int action_set_type_reg(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(2);
  A_CHECK(4);

  gas_assert(tokens[1].parser_token == T_REG);
  gas_assert(tokens[3].parser_token == T_REG);
  insn_code =
    FIELD_D(tokens[1].first_lexer_token->sub_type) |
    FIELD_C(0x0) |
    FIELD_B(0xc) |
    FIELD_A(tokens[3].first_lexer_token->sub_type);
  A_RETURN();
}

static int action_set_type_imm(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(2);
  A_CHECK(5);

  gas_assert(tokens[1].parser_token == T_REG);
  gas_assert(tokens[4].parser_token == T_TYPENAME);
  insn_code =
    FIELD_D(tokens[1].first_lexer_token->sub_type) |
    FIELD_C(0x0) |
    FIELD_B(0xe) |
    FIELD_A(tokens[4].first_lexer_token->sub_type);
  A_RETURN();
}

static int action_link(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  int32_t link_ofs;
  A_PROLOG(2);
  A_CHECK(5);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[2].parser_token == T_PC);
  gas_assert(tokens[4].parser_token == ~T_NULL);

  if (tokens[2].first_lexer_token->sub_type != ST_PC_PC)
    {
      as_bad(_("only $pc is allowed as link source"));
    }
  if (!parse_int(tokens[3].first_lexer_token, tokens[4].last_lexer_token, &link_ofs))
    {
      as_bad(_("Can't parse link offset"));
      return A_ERR;
    }
  if ((link_ofs & 1) != 0)
    {
      as_bad(_("Link offset must be even"));
      return A_ERR;
    }
  link_ofs /= 2;
  if (link_ofs > 14 || link_ofs < 0)
    {
      as_bad(_("Link offset is out of range"));
      return A_ERR;
    }
  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(0x0) |
    FIELD_B(0x2) |
    FIELD_A(link_ofs);
  A_RETURN();
}

static int action_move_pc_to_reg(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(2);
  A_CHECK(3);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[2].parser_token == T_PC);

  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(0x0) |
    FIELD_B(0x0) |
    FIELD_A(tokens[2].first_lexer_token->sub_type == ST_PC_PC ? 0x4 : 0x5);
  A_RETURN();
}

static int action_binary_reg_reg(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(2);
  A_CHECK(5);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[2].parser_token == T_REG);
  gas_assert(tokens[4].parser_token == T_REG);

  size_t op_code = tokens[3].first_lexer_token->sub_type;
  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(op_code) |
    FIELD_B(tokens[4].first_lexer_token->sub_type) |
    FIELD_A(tokens[2].first_lexer_token->sub_type);
  A_RETURN();
}

static int action_binary_reg_imm(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool support_float = true;
  const brew_parser_tokenS *reg_dst;
  const brew_parser_tokenS *reg_op;
  const brew_parser_tokenS *op_tok;
  const brew_parser_tokenS *imm_op;
  bool is_short;
  enum bfd_reloc_code_real reloc_type;
  size_t op_code;

  is_short = tokens[2].parser_token == T_SHORT;

  A_PROLOG(is_short ? 4 : 6);
  A_CHECK(is_short ? 6 : 5);

  reg_dst = &tokens[0];
  if (is_short)
    {
      reg_op = &tokens[3];
      op_tok = &tokens[4];
      imm_op = &tokens[5];
    }
  else
    {
      reg_op = &tokens[2];
      op_tok = &tokens[3];
      imm_op = &tokens[4];
    }
  gas_assert(reg_dst->parser_token == T_REG);
  gas_assert(reg_op->parser_token == T_REG);
  gas_assert(imm_op->parser_token == ~T_NULL);

  reloc_type = is_short ? BFD_RELOC_16 : BFD_RELOC_32;
  op_code = op_tok->first_lexer_token->sub_type;

  // We don't have this op in this format. We have only VALUE-$rX. So we'll implement it as $rx+(-VALUE)
  if (imm_op->first_lexer_token->sub_type == ST_MINUS)
    {
      reloc_type = is_short ? BFD_RELOC_BREW_NEG16 : BFD_RELOC_BREW_NEG32;
      op_code = 0x4; // override operation to '+' instead of '-'
    }

  if (!parse_expression(imm_op->first_lexer_token, imm_op->last_lexer_token, frag, 2, insn_len, support_float, reloc_type))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }

  if (is_short)
    {
      insn_code =
        FIELD_D(reg_dst->first_lexer_token->sub_type) |
        FIELD_C(op_code) |
        FIELD_B(0xf) |
        FIELD_A(reg_op->first_lexer_token->sub_type);
    }
  else
    {
      insn_code =
        FIELD_D(reg_dst->first_lexer_token->sub_type) |
        FIELD_C(op_code) |
        FIELD_B(reg_op->first_lexer_token->sub_type) |
        FIELD_A(0xf);
    }
  A_RETURN();
}

static int action_move_imm_to_reg(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  const brew_parser_tokenS *reg_dst;
  const brew_parser_tokenS *imm_op;
  bool is_short;

  is_short = tokens[2].parser_token == T_SHORT;

  A_PROLOG(is_short ? 4 : 6);
  A_CHECK(is_short ? 4 : 3);

  reg_dst = &tokens[0];
  if (is_short)
    {
      imm_op = &tokens[3];
    }
  else
    {
      imm_op = &tokens[2];
    }

  gas_assert(reg_dst->parser_token == T_REG);
  gas_assert(imm_op->parser_token == ~T_NULL);

  if (!parse_expression(imm_op->first_lexer_token, imm_op->last_lexer_token, frag, 2, insn_len, false, is_short ? BFD_RELOC_16 : BFD_RELOC_32))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }

  if (is_short)
    {
      insn_code =
        FIELD_D(reg_dst->first_lexer_token->sub_type) |
        FIELD_C(0x0) |
        FIELD_B(0xf) |
        FIELD_A(0x0);
    }
  else
    {
      insn_code =
        FIELD_D(reg_dst->first_lexer_token->sub_type) |
        FIELD_C(0x0) |
        FIELD_B(0x0) |
        FIELD_A(0xf);
    }
  A_RETURN();
}

static int action_binary_imm_reg(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool support_float = true;
  const brew_parser_tokenS *reg_dst;
  const brew_parser_tokenS *reg_op;
  const brew_lexer_tokenS *op_tok;
  const brew_parser_tokenS *imm_op;
  bool is_short;
  enum bfd_reloc_code_real reloc_type;
  size_t op_code;

  is_short = tokens[2].parser_token == T_SHORT;

  A_PROLOG(is_short ? 4 : 6);
  A_CHECK(is_short ? 5 : 4);

  reg_dst = &tokens[0];
  if (is_short)
    {
      reg_op = &tokens[4];
      op_tok = tokens[3].last_lexer_token-1;
      imm_op = &tokens[3];
    }
  else
    {
      reg_op = &tokens[3];
      op_tok = tokens[2].last_lexer_token-1;
      imm_op = &tokens[2];
    }
  gas_assert(reg_dst->parser_token == T_REG);
  gas_assert(reg_op->parser_token == T_REG);
  gas_assert(imm_op->parser_token == ~T_REG);

  DEBUG("op_tok: %d, %s", op_tok->type, brew_tok_name(op_tok->type));

  reloc_type = is_short ? BFD_RELOC_16 : BFD_RELOC_32;
  op_code = op_tok->sub_type;

  if (!parse_expression(imm_op->first_lexer_token, imm_op->last_lexer_token-1, frag, 2, insn_len, support_float, reloc_type))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }

  if (is_short)
    {
      insn_code =
        FIELD_D(reg_dst->first_lexer_token->sub_type) |
        FIELD_C(op_code) |
        FIELD_B(0xf) |
        FIELD_A(reg_op->first_lexer_token->sub_type);
    }
  else
    {
      insn_code =
        FIELD_D(reg_dst->first_lexer_token->sub_type) |
        FIELD_C(op_code) |
        FIELD_B(reg_op->first_lexer_token->sub_type) |
        FIELD_A(0xf);
    }
  A_RETURN();
}

static int action_reciprocal(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(2);
  A_CHECK(5);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[4].parser_token == T_REG);

  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(0x0) |
    FIELD_B(0x9) |
    FIELD_A(tokens[4].first_lexer_token->sub_type);
  A_RETURN();
}

static int action_tiny_add_sub(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  int32_t imm;
  A_PROLOG(2);
  A_CHECK(6);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[3].parser_token == T_REG);
  gas_assert(tokens[5].parser_token == ~T_NULL);


  if (tokens[2].first_lexer_token->sub_type != ST_PC_PC)
    {
      as_bad(_("only $pc is allowed as link source"));
    }
  if (!parse_int(tokens[4].first_lexer_token, tokens[5].last_lexer_token, &imm))
    {
      as_bad(_("Can't parse immediate"));
      return A_ERR;
    }
  if (imm > 7 || imm < -7)
    {
      as_bad(_("Immediate is out of range"));
      return A_ERR;
    }
  // Convert to 1's complement
  if (imm < 0)
    imm -= 1;
  gas_assert((imm & 0xf) != 0xf);
  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(0xb) |
    FIELD_B(tokens[3].first_lexer_token->sub_type) |
    FIELD_A(imm);
  A_RETURN();
}


static void cmp_subtype_to_op_code(int sub_type, bool is_zero, bool is_signed, bool is_any, int *op_code, bool *swap_args)
{
  *swap_args = false;
  switch (sub_type)
    {
      case ST_CMP_EQ:
        *op_code = is_zero ? 0 : 1;
        break;
      case ST_CMP_NE:
        *op_code = is_zero ? 1 : 2;
        break;
      case ST_CMP_LT:
        if (is_zero)
          {
            *op_code = 2;
          }
        else
          {
            *op_code = is_signed ? 3 : 5;
          }
        break;
      case ST_CMP_GE:
        if (is_zero)
          {
            *op_code = 3;
          }
        else
          {
            *op_code = is_signed ? 4 : 6;
          }
        break;
      case ST_CMP_GT:
        if (is_zero)
          {
            *op_code = 4;
          }
        else
          {
            *op_code = is_signed ? 3 : 5;
            *swap_args = true;
          }
        break;
      case ST_CMP_LE:
        if (is_zero)
          {
            *op_code = 5;
          }
        else
          {
            *op_code = is_signed ? 4 : 6;
            *swap_args = true;
          }
        break;
      default:
        gas_assert(false);
    }
  if (!is_any)
    *op_code |= 8;
}

static int action_lane_cmp(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_signed;
  bool is_zero;
  bool swap_args;
  int op_code = 0;

  A_PROLOG_EXT(4);
  is_signed = tokens[2].parser_token == T_SIGNED;
  A_CHECK(is_signed ? 6 : 5);

  const brew_parser_tokenS *first_arg = &tokens[is_signed ? 3 : 2];
  const brew_parser_tokenS *cmp = &tokens[is_signed ? 4 : 3];
  const brew_parser_tokenS *second_arg = &tokens[is_signed ? 5 : 4];
  gas_assert(first_arg->parser_token == T_REG);
  gas_assert(second_arg->parser_token == T_REG || !is_signed);

  is_zero = second_arg->parser_token == T_ZERO;

  cmp_subtype_to_op_code(cmp->first_lexer_token->sub_type, is_zero, is_signed, true, &op_code, &swap_args);

  ext_code =
    FIELD_D(0xf) |
    FIELD_C(0xf) |
    FIELD_B(0xf) |
    FIELD_A(0x0);
  if (is_zero)
    {
      gas_assert(!swap_args);
      insn_code =
        FIELD_D(tokens[0].first_lexer_token->sub_type) |
        FIELD_C(0) |
        FIELD_B(op_code) |
        FIELD_A(first_arg->first_lexer_token->sub_type);
    }
  else
    {
      insn_code =
        FIELD_D(tokens[0].first_lexer_token->sub_type) |
        FIELD_C(op_code) |
        FIELD_B(swap_args ? first_arg->first_lexer_token->sub_type : second_arg->first_lexer_token->sub_type) |
        FIELD_A(swap_args ? second_arg->first_lexer_token->sub_type : first_arg->first_lexer_token->sub_type);
    }
  A_RETURN_EXT();
}

static int action_cbranch(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_signed;
  bool is_zero;
  bool is_any = true;
  bool is_qualified;
  bool swap_args;
  int op_code = 0;
  int prefix_size;

  A_PROLOG(4);
  is_qualified = tokens[1].parser_token == T_IF_QUAL;
  is_signed = tokens[is_qualified ? 2 : 1].parser_token == T_SIGNED;
  prefix_size = is_qualified ? (is_signed ? 3 : 2) : (is_signed ? 2 : 1);
  DEBUG("prefix_size: %d", prefix_size);
  A_CHECK(prefix_size + 6);

  const brew_parser_tokenS *first_arg = &tokens[prefix_size + 0];
  const brew_parser_tokenS *cmp = &tokens[prefix_size + 1];
  const brew_parser_tokenS *second_arg = &tokens[prefix_size + 2];
  const brew_parser_tokenS *target = &tokens[prefix_size + 5];

  if (is_qualified)
    is_any = (tokens[1].first_lexer_token->sub_type == ST_IF_ANY);

  gas_assert(first_arg->parser_token == T_REG);
  gas_assert(second_arg->parser_token == T_REG || !is_signed);
  gas_assert(cmp->parser_token == T_CMP);
  gas_assert(target->parser_token == ~T_NULL);

  is_zero = second_arg->parser_token == T_ZERO;

  cmp_subtype_to_op_code(cmp->first_lexer_token->sub_type, is_zero, is_signed, is_any, &op_code, &swap_args);

  if (!parse_expression(target->first_lexer_token, target->last_lexer_token, frag, 2, insn_len, false, BFD_RELOC_BREW_PCREL16))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }

  if (is_zero)
    {
      gas_assert(!swap_args);
      insn_code =
        FIELD_D(0xf) |
        FIELD_C(0) |
        FIELD_B(op_code) |
        FIELD_A(first_arg->first_lexer_token->sub_type);
    }
  else
    {
      insn_code =
        FIELD_D(0xf) |
        FIELD_C(op_code) |
        FIELD_B(swap_args ? first_arg->first_lexer_token->sub_type : second_arg->first_lexer_token->sub_type) |
        FIELD_A(swap_args ? second_arg->first_lexer_token->sub_type : first_arg->first_lexer_token->sub_type);
    }
  A_RETURN();
}

static int action_cbranch_bittest(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_zero;
  int bit_idx;

  A_PROLOG(4);
  A_CHECK(10);

  is_zero = (tokens[6].parser_token == T_ZERO);

  gas_assert(tokens[1].parser_token == T_REG);
  gas_assert(tokens[3].parser_token == ~T_RBRACKET);
  gas_assert(tokens[9].parser_token == ~T_NULL);

  if (!parse_expression(tokens[9].first_lexer_token, tokens[9].last_lexer_token, frag, 2, insn_len, false, BFD_RELOC_BREW_PCREL16))
    {
      as_bad(_("Can't parse expression"));
      return A_ERR;
    }

  if (!parse_int(tokens[3].first_lexer_token, tokens[3].last_lexer_token, &bit_idx))
    {
      as_bad(_("Bit-index must be an integer"));
      return A_ERR;
    }
  if (bit_idx < 0 || bit_idx > 31)
    {
      as_bad(_("Bit-index out of range"));
      return A_ERR;
    }
  bit_idx = bit_idx_to_field_a[bit_idx];
  if (bit_idx < 0)
    {
      as_bad(_("Invalid bit-index"));
      return A_ERR;
    }
  gas_assert(bit_idx > 0 && bit_idx < 0xf);

  if (is_zero)
    {
      insn_code =
        FIELD_D(0xf) |
        FIELD_C(bit_idx) |
        FIELD_B(tokens[1].first_lexer_token->sub_type) |
        FIELD_A(0xf);
    }
  else
    {
      insn_code =
        FIELD_D(0xf) |
        FIELD_C(bit_idx) |
        FIELD_B(0xf) |
        FIELD_A(tokens[1].first_lexer_token->sub_type);
    }
  A_RETURN();
}

static int action_move_reg_to_reg(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(2);
  A_CHECK(3);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[2].parser_token == T_REG);

  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(0x2) |
    FIELD_B(tokens[2].first_lexer_token->sub_type) |
    FIELD_A(tokens[2].first_lexer_token->sub_type);
  A_RETURN();
}

static int action_inverse_and(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG(2);
  A_CHECK(6);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[3].parser_token == T_REG);
  gas_assert(tokens[5].parser_token == T_REG);

  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(0xa) |
    FIELD_B(tokens[5].first_lexer_token->sub_type) |
    FIELD_A(tokens[3].first_lexer_token->sub_type);
  A_RETURN();
}

static int action_prefix_op(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  int op_code;

  A_PROLOG(2);
  A_CHECK(4);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[3].parser_token == T_REG);

  op_code = tokens[2].first_lexer_token->sub_type;
  if (tokens[2].parser_token == T_PLUS_MINUS)
    {
      if (tokens[2].first_lexer_token->sub_type != ST_MINUS)
        {
          as_bad(_("Invalid prefix operation: use register move instead"));
          return A_ERR;
        }
      op_code = 3;
    }
  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(0x0) |
    FIELD_B(op_code) |
    FIELD_A(tokens[3].first_lexer_token->sub_type);
  A_RETURN();
}

static int action_interpolate(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  A_PROLOG_EXT(4);
  A_CHECK(6);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[3].parser_token == T_REG);
  gas_assert(tokens[5].parser_token == T_REG);

  ext_code =
    FIELD_D(0xf) |
    FIELD_C(0xf) |
    FIELD_B(0xf) |
    FIELD_A(0x1);
  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(0x0) |
    FIELD_B(tokens[5].first_lexer_token->sub_type) |
    FIELD_A(tokens[3].first_lexer_token->sub_type);
  A_RETURN_EXT();
}

static int action_full_mult(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  int shift_amount;
  A_PROLOG_EXT(4);
  A_CHECK(8);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[3].parser_token == T_REG);
  gas_assert(tokens[5].parser_token == T_REG);
  gas_assert(tokens[6].parser_token == T_RSHIFT);
  gas_assert(tokens[7].parser_token == ~T_NULL);

  if (!parse_int(tokens[7].first_lexer_token, tokens[7].last_lexer_token, &shift_amount))
    {
      as_bad(_("Can't parse shift expression"));
      return A_ERR;
    }
  if (shift_amount < 0 || shift_amount > 31)
    {
      as_bad(_("Shift amount out of range"));
      return A_ERR;
    }
  ext_code =
    FIELD_D(0xf) |
    FIELD_C(0xf) |
    shift_amount;
  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(tokens[6].first_lexer_token->sub_type == 0x7 ? 1 : 0) |
    FIELD_B(tokens[5].first_lexer_token->sub_type) |
    FIELD_A(tokens[3].first_lexer_token->sub_type);
  A_RETURN_EXT();
}

//T_REG, T_ASSIGN, T_SWIZZLE, T_REG, T_COMMA, ~T_NULL
static int action_swizzle(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  uint swizzle_pattern = 0;
  A_PROLOG(4);
  A_CHECK(6);

  gas_assert(tokens[0].parser_token == T_REG);
  gas_assert(tokens[3].parser_token == T_REG);
  gas_assert(tokens[5].parser_token == ~T_NULL);

  // Parse the swizzle lane configuration
  if (tokens[5].first_lexer_token->len != 4)
    {
      as_bad(_("Swizzle expression must be 4 digits"));
      return A_ERR;
    }
  const char *lane_digit = tokens[5].first_lexer_token->start;
  for (uint i=0;i<tokens[5].first_lexer_token->len;++i) {
    if (*lane_digit< '0' || *lane_digit > '3')
      {
        as_bad(_("Invalid swizzle expression at digit %d"), i);
        return A_ERR;
      }
    swizzle_pattern = (swizzle_pattern << 2) | (*lane_digit - '0');
    ++lane_digit;
  }
  gas_assert(swizzle_pattern <= 255);
  md_number_to_chars(frag+2, swizzle_pattern, 2);
  insn_code =
    FIELD_D(tokens[0].first_lexer_token->sub_type) |
    FIELD_C(0xa) |
    FIELD_B(0xf) |
    FIELD_A(tokens[3].first_lexer_token->sub_type);
  A_RETURN();
}

static int action_load_multi_type(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_masked;
  bool is_upper;
  uint16_t mask;
  int offs;

  is_masked = (tokens[11].parser_token != T_NULL);
  mask = 0;
  A_PROLOG(is_masked ? 4 : 2);
  A_CHECK(is_masked ? 13 : 11);

  gas_assert(tokens[1].parser_token == T_REG);
  gas_assert(tokens[3].parser_token == T_REG);
  gas_assert(tokens[7].parser_token == T_REG);
  gas_assert(tokens[9].parser_token == ~T_RBRACKET);
  gas_assert(!is_masked || tokens[12].parser_token == ~T_NULL);

  is_upper = (tokens[1].first_lexer_token->sub_type != 0);
  if (is_upper)
    {
      if (tokens[1].first_lexer_token->sub_type != 8 || tokens[3].first_lexer_token->sub_type != 14)
        {
          as_bad(_("Invalid register range"));
          return A_ERR;
        }
    }
  else
    {
      if (tokens[1].first_lexer_token->sub_type != 0 || tokens[3].first_lexer_token->sub_type != 7)
        {
          as_bad(_("Invalid register range"));
          return A_ERR;
        }
    }
  // Parse mask
  if (is_masked)
    {
      mask = 0;
      size_t mask_len = is_upper ? 7 : 8;
      const char *mask_digit;
      if (tokens[12].first_lexer_token != tokens[12].last_lexer_token-1)
        {
          as_bad(_("Mask must be a single string of 0-s and 1-s"));
          return A_ERR;
        }
      if (tokens[12].first_lexer_token->len != mask_len)
        {
          as_bad(_("Mask must be %ld digits"), mask_len);
          return A_ERR;
        }
      mask_digit = tokens[12].first_lexer_token->start;
      for (uint i=0;i<tokens[12].first_lexer_token->len;++i) {
        if (*mask_digit < '0' || *mask_digit > '1')
          {
            as_bad(_("Invalid mask at digit %d"), i);
            return A_ERR;
          }
        mask = (mask >> 1) | ((*mask_digit - '0') << mask_len);
        ++mask_digit;
      }
      md_number_to_chars(frag+2, mask, 2);
    }

  if (!parse_int(tokens[8].first_lexer_token, tokens[9].last_lexer_token, &offs))
    {
      as_bad(_("Can't parse offset"));
      return A_ERR;
    }
  if ((offs & 3) != 0)
    {
      as_bad(_("Offset must be multiple of 4"));
      return A_ERR;
    }
  offs >>= 2;
  if (offs > 7 || offs < -7)
    {
      as_bad(_("Offset out of range"));
      return A_ERR;
    }
  // Create 1-s complement
  if (offs < 0)
    offs -= 1;
  insn_code =
    FIELD_D(tokens[7].first_lexer_token->sub_type) |
    FIELD_C(is_masked ? 0xf : 0xe) |
    FIELD_B(is_upper ? 0x1 : 0x0) |
    FIELD_A(offs);
  A_RETURN();
}

static int action_store_multi_type(void *context ATTRIBUTE_UNUSED, const brew_parser_tokenS *tokens)
{
  bool is_upper;
  int offs;

  A_PROLOG(2);
  A_CHECK(11);

  gas_assert(tokens[2].parser_token == T_REG);
  gas_assert(tokens[4].parser_token == ~T_RBRACKET);
  gas_assert(tokens[8].parser_token == T_REG);
  gas_assert(tokens[10].parser_token == T_REG);

  is_upper = (tokens[8].first_lexer_token->sub_type != 0);
  if (is_upper)
    {
      if (tokens[8].first_lexer_token->sub_type != 8 || tokens[10].first_lexer_token->sub_type != 14)
        {
          as_bad(_("Invalid register range"));
          return A_ERR;
        }
    }
  else
    {
      if (tokens[8].first_lexer_token->sub_type != 0 || tokens[10].first_lexer_token->sub_type != 7)
        {
          as_bad(_("Invalid register range"));
          return A_ERR;
        }
    }

  if (!parse_int(tokens[3].first_lexer_token, tokens[4].last_lexer_token, &offs))
    {
      as_bad(_("Can't parse offset"));
      return A_ERR;
    }
  if ((offs & 3) != 0)
    {
      as_bad(_("Offset must be multiple of 4"));
      return A_ERR;
    }
  offs >>= 2;
  if (offs > 7 || offs < -7)
    {
      as_bad(_("Offset out of range"));
      return A_ERR;
    }
  // Create 1-s complement
  if (offs < 0)
    offs -= 1;
  insn_code =
    FIELD_D(tokens[2].first_lexer_token->sub_type) |
    FIELD_C(0xe) |
    FIELD_B(is_upper ? 0x3 : 0x2) |
    FIELD_A(offs);
  A_RETURN();
}



static const brew_parser_tok_type_t raw_insn[] = {
  PATTERN(T_INSN),                                                                                                                      ACTION(action_insn),
  PATTERN(T_SWI, ~T_NULL),                                                                                                              ACTION(action_swi),
  PATTERN(T_PC, T_ASSIGN, T_REG),                                                                                                       ACTION(action_move_reg_to_pc),
  PATTERN(T_PC, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_RBRACKET),                                                                        ACTION(action_load_reg),
  PATTERN(T_PC, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_PLUS_MINUS, ~T_RBRACKET, T_RBRACKET),                                             ACTION(action_load_reg_ofs),
  PATTERN(T_PC, T_ASSIGN, T_MEM, T_LBRACKET, ~T_RBRACKET, T_RBRACKET),                                                                  ACTION(action_load_ofs),
  PATTERN(T_PC, T_ASSIGN, ~T_NULL),                                                                                                     ACTION(action_move_imm_to_pc),
  PATTERN(T_PC, T_ASSIGN, T_SHORT, ~T_NULL),                                                                                            ACTION(action_move_short_imm_to_pc),

  PATTERN(T_TYPE, T_REG, T_ASSIGN, T_REG),                                                                                              ACTION(action_set_type_reg),
  PATTERN(T_TYPE, T_REG, T_ASSIGN, T_TYPE, T_TYPENAME),                                                                                 ACTION(action_set_type_imm),

  PATTERN(T_REG, T_ASSIGN, T_PC, T_PLUS_MINUS, ~T_NULL),                                                                                ACTION(action_link),
  PATTERN(T_REG, T_ASSIGN, T_PC),                                                                                                       ACTION(action_move_pc_to_reg),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_HAT, T_REG),                                                                                        ACTION(action_binary_reg_reg),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_BAR, T_REG),                                                                                        ACTION(action_binary_reg_reg),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_LSHIFT, T_REG),                                                                                     ACTION(action_binary_reg_reg),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_RSHIFT, T_REG),                                                                                     ACTION(action_binary_reg_reg),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_PLUS_MINUS, T_REG),                                                                                 ACTION(action_binary_reg_reg),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_STAR, T_REG),                                                                                       ACTION(action_binary_reg_reg),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_AND, T_REG),                                                                                        ACTION(action_binary_reg_reg),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_HAT, ~T_NULL),                                                                                      ACTION(action_binary_reg_imm),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_BAR, ~T_NULL),                                                                                      ACTION(action_binary_reg_imm),
  //PATTERN(T_REG, T_ASSIGN, T_REG, T_LSHIFT, ~T_NULL),                                                                                   ACTION(INVALID_PATTERN),
  //PATTERN(T_REG, T_ASSIGN, T_REG, T_RSHIFT, ~T_NULL),                                                                                   ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_PLUS_MINUS, ~T_NULL),                                                                               ACTION(action_binary_reg_imm),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_STAR, ~T_NULL),                                                                                     ACTION(action_binary_reg_imm),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_AND, ~T_NULL),                                                                                      ACTION(action_binary_reg_imm),
  PATTERN(T_REG, T_ASSIGN, T_TINY, T_REG, T_PLUS_MINUS, ~T_NULL),                                                                       ACTION(action_tiny_add_sub),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_CMP, T_ZERO),                                                                                       ACTION(action_lane_cmp),
  PATTERN(T_REG, T_ASSIGN, T_REG, T_CMP, T_REG),                                                                                        ACTION(action_lane_cmp),
  PATTERN(T_REG, T_ASSIGN, T_SIGNED, T_REG, T_CMP, T_REG),                                                                              ACTION(action_lane_cmp),
  PATTERN(T_REG, T_ASSIGN, T_REG),                                                                                                      ACTION(action_move_reg_to_reg),
  PATTERN(T_REG, T_ASSIGN, T_TILDE, T_REG, T_AND, T_REG),                                                                               ACTION(action_inverse_and),
  PATTERN(T_REG, T_ASSIGN, T_PREFIX_OP, T_REG),                                                                                         ACTION(action_prefix_op),
  PATTERN(T_REG, T_ASSIGN, T_PLUS_MINUS, T_REG),                                                                                        ACTION(action_prefix_op),
  PATTERN(T_REG, T_ASSIGN, T_TILDE, T_REG),                                                                                             ACTION(action_prefix_op),
  PATTERN(T_REG, T_ASSIGN, T_INTERPOLATE, T_REG, T_COMMA, T_REG),                                                                       ACTION(action_interpolate),
  PATTERN(T_REG, T_ASSIGN, T_FULL, T_REG, T_STAR, T_REG, T_RSHIFT, ~T_NULL),                                                            ACTION(action_full_mult),
  PATTERN(T_REG, T_ASSIGN, T_SWIZZLE, T_REG, T_COMMA, ~T_NULL),                                                                         ACTION(action_swizzle),

  PATTERN(T_REG, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_RBRACKET),                                                                       ACTION(action_load_reg),
  PATTERN(T_REG, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_PLUS_MINUS, T_TINY, ~T_RBRACKET, T_RBRACKET),                                    ACTION(action_load_reg_ofs),
  PATTERN(T_REG, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_PLUS_MINUS, ~T_RBRACKET, T_RBRACKET),                                            ACTION(action_load_reg_ofs),
  PATTERN(T_REG, T_ASSIGN, T_MEM, T_LBRACKET, ~T_RBRACKET, T_RBRACKET),                                                                 ACTION(action_load_ofs),

  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_HAT, ~T_NULL),                                                                             ACTION(action_binary_reg_imm),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_BAR, ~T_NULL),                                                                             ACTION(action_binary_reg_imm),
  //PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_LSHIFT, ~T_NULL),                                                                          ACTION(INVALID_PATTERN),
  //PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_RSHIFT, ~T_NULL),                                                                          ACTION(INVALID_PATTERN),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_PLUS_MINUS, ~T_NULL),                                                                      ACTION(action_binary_reg_imm),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_STAR, ~T_NULL),                                                                            ACTION(action_binary_reg_imm),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, T_REG, T_AND, ~T_NULL),                                                                             ACTION(action_binary_reg_imm),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, ~T_REG, T_REG),                                                                                     ACTION(action_binary_imm_reg),
  PATTERN(T_REG, T_ASSIGN, T_SHORT, ~T_NULL),                                                                                           ACTION(action_move_imm_to_reg),

  PATTERN(T_REG, T_ASSIGN, ~T_REG, T_REG),                                                                                              ACTION(action_binary_imm_reg),
  PATTERN(T_REG, T_ASSIGN, T_ONE, T_DIV, T_REG),                                                                                        ACTION(action_reciprocal),
  PATTERN(T_REG, T_ASSIGN, ~T_NULL),                                                                                                    ACTION(action_move_imm_to_reg),

  PATTERN(T_MEM, T_LBRACKET, T_REG, T_RBRACKET),                                                                                        ACTION(action_store_reg),
  PATTERN(T_MEM, T_LBRACKET, T_REG, T_PLUS_MINUS, ~T_RBRACKET, T_RBRACKET),                                                             ACTION(action_store_reg_ofs),
  PATTERN(T_MEM, T_LBRACKET, ~T_RBRACKET, T_RBRACKET),                                                                                  ACTION(action_store_ofs),

  PATTERN(T_MEM, T_LBRACKET, T_REG, T_RBRACKET, T_ASSIGN, T_REG),                                                                       ACTION(action_store_reg),
  PATTERN(T_MEM, T_LBRACKET, T_REG, T_PLUS_MINUS, ~T_RBRACKET, T_RBRACKET, T_ASSIGN, T_REG),                                            ACTION(action_store_reg_ofs),
  PATTERN(T_MEM, T_LBRACKET, T_REG, T_PLUS_MINUS, T_TINY, ~T_RBRACKET, T_RBRACKET, T_ASSIGN, T_REG),                                    ACTION(action_store_reg_ofs),
  PATTERN(T_MEM, T_LBRACKET, ~T_RBRACKET, T_RBRACKET, T_ASSIGN, T_REG),                                                                 ACTION(action_store_ofs),

  PATTERN(T_IF, T_REG, T_CMP, T_ZERO, T_PC, T_ASSIGN, ~T_NULL),                                                                         ACTION(action_cbranch),
  PATTERN(T_IF, T_REG, T_CMP, T_REG, T_PC, T_ASSIGN, ~T_NULL),                                                                          ACTION(action_cbranch),
  PATTERN(T_IF, T_SIGNED, T_REG, T_CMP, T_ZERO, T_PC, T_ASSIGN, ~T_NULL),                                                               ACTION(action_cbranch),
  PATTERN(T_IF, T_SIGNED, T_REG, T_CMP, T_REG, T_PC, T_ASSIGN, ~T_NULL),                                                                ACTION(action_cbranch),
  PATTERN(T_IF, T_IF_QUAL, T_REG, T_CMP, T_ZERO, T_PC, T_ASSIGN, ~T_NULL),                                                              ACTION(action_cbranch),
  PATTERN(T_IF, T_IF_QUAL, T_REG, T_CMP, T_REG, T_PC, T_ASSIGN, ~T_NULL),                                                               ACTION(action_cbranch),
  PATTERN(T_IF, T_IF_QUAL, T_SIGNED, T_REG, T_CMP, T_ZERO, T_PC, T_ASSIGN, ~T_NULL),                                                    ACTION(action_cbranch),
  PATTERN(T_IF, T_IF_QUAL, T_SIGNED, T_REG, T_CMP, T_REG, T_PC, T_ASSIGN, ~T_NULL),                                                     ACTION(action_cbranch),

  PATTERN(T_IF, T_REG, T_LBRACKET, ~T_RBRACKET, T_RBRACKET, T_CMP, T_ONE, T_PC, T_ASSIGN, ~T_NULL),                                     ACTION(action_cbranch_bittest),
  PATTERN(T_IF, T_REG, T_LBRACKET, ~T_RBRACKET, T_RBRACKET, T_CMP, T_ZERO, T_PC, T_ASSIGN, ~T_NULL),                                    ACTION(action_cbranch_bittest),

  PATTERN(T_TYPE, T_REG, T_ELLIPSIS, T_REG, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_PLUS_MINUS, ~T_RBRACKET, T_RBRACKET),                 ACTION(action_load_multi_type),
  PATTERN(T_TYPE, T_REG, T_ELLIPSIS, T_REG, T_ASSIGN, T_MEM, T_LBRACKET, T_REG, T_PLUS_MINUS, ~T_RBRACKET, T_RBRACKET, T_AND, ~T_NULL), ACTION(action_load_multi_type),
  PATTERN(T_MEM, T_LBRACKET, T_REG, T_PLUS_MINUS, ~T_RBRACKET, T_RBRACKET, T_ASSIGN, T_TYPE, T_REG, T_ELLIPSIS, T_REG),                 ACTION(action_store_multi_type),
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
  switch (brew_parse(parser, token_strm, NULL))
    {
      case A_OK:
      break;
      case A_ERR:
        // Error is already reported to the user
        ignore_rest_of_line();
      break;
      case A_NOT_FOUND:
        as_bad(_("Unrecognized instruction"));
        ignore_rest_of_line();
      break;
      default:
        gas_assert(false);
      break;
    }
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
  uint16_t munged_val;
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
    case BFD_RELOC_BREW_7:
      if ((val & 3) != 0)
        {
          as_bad_where (fixP->fx_file, fixP->fx_line, _("tiny offset must be DWORD aligned"));
        }
      max = 508;
      min = -512;
      buf[0] = (buf[0] & 1) | (((val >> 2) << 1) & 0xfe);
      buf += 2;
      break;
    case BFD_RELOC_BREW_PCREL16:
      max = INT16_MAX;
      min = INT16_MIN;
      // This is a relative 17-bit offset, the LSB being 0 and not stored
      if ((val & 1) != 0)
        as_bad_where (fixP->fx_file, fixP->fx_line, _("$pc offset must be even"));
      munged_val = brew_munge_address(val);
      buf[1] = munged_val >> 8;
      buf[0] = munged_val >> 0;
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

  fprintf(stderr, "FIXME: %s:%d do we need the -2 here?\n", __FILE__, __LINE__);
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
