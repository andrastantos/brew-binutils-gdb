/* tc-brew-lex.c -- A simple lexer (tokenizer) for the BREW assembly.

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

   You should have received a copy of the GNU General Public License along
   with GAS; see the file COPYING.  If not, write to the Free Software
   Foundation, 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <as.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libiberty.h>

#include <safe-ctype.h>
#include "tc-brew-lex.h"
#include "dis-asm.h" // for fprintf_ftype
#include "opcode/brew.h"
#include "opcode/brew-abi.h"

static const char *tok_names[] = {
  "T_NULL",
  "T_REG",
  "T_PC",
  "T_INSN",
  "T_SWI",
  "T_FENCE",
  "T_ASSIGN",
  "T_PREFIX_OP",
  "T_TILDE",
  "T_SWIZZLE",
  "T_INTERPOLATE",
  "T_HAT",
  "T_BAR",
  "T_LSHIFT",
  "T_RSHIFT",
  "T_PLUS_MINUS",
  "T_STAR",
  "T_AND",
  "T_DIV",
  "T_SHORT",
  "T_TINY",
  "T_FULL",
  "T_MEM",
  "T_COMMA",
  "T_LBRACKET",
  "T_RBRACKET",
  "T_LCURLY",
  "T_RCURLY",
  "T_IF",
  "T_IF_QUAL",
  "T_CMP",
  "T_ZERO",
  "T_ONE",
  "T_TYPE",
  "T_TYPENAME",
  "T_SIGNED",
  "T_ELLIPSIS",
  "T_UNKNOWN"
};

typedef struct
{
  brew_lexer_tokenS *data;
  size_t alloc_size;
  size_t len;
} lexer_token_strmS;

static void lexer_token_strm_init(lexer_token_strmS *strm)
{
  size_t cnt = 16; // start with 16 tokens. Grow exponentially...

  gas_assert(strm != NULL);

  strm->data = (brew_lexer_tokenS*)malloc(sizeof(brew_lexer_tokenS)*cnt);
  for(size_t i=0;i<cnt;++i)
    {
      strm->data[i].type = T_NULL;
    }
  strm->alloc_size = cnt;
  strm->len = 0;
}

static void _lexer_token_strm_grow(lexer_token_strmS *strm)
{
  size_t new_cnt;

  gas_assert(strm != NULL);
  gas_assert(strm->alloc_size > 0);

  new_cnt = strm->alloc_size*2;
  strm->data = (brew_lexer_tokenS*)realloc(strm->data, sizeof(brew_lexer_tokenS)*new_cnt);
  gas_assert(strm != NULL);
  for(size_t i=strm->alloc_size;i<new_cnt;++i)
    {
      strm->data[i].type = T_NULL;
    }
  strm->alloc_size = new_cnt;
}

static void lexer_token_strm_append_token(lexer_token_strmS *strm, enum TOK_TYPES type, size_t sub_type, uint16_t len, const char *start)
{
  brew_lexer_tokenS *token;

  gas_assert(strm != NULL);
  if (strm->len == strm->alloc_size)
    _lexer_token_strm_grow(strm);
  token = &strm->data[strm->len];
  token->type = type;
  token->sub_type = sub_type;
  token->len = len;
  token->start = start;
  strm->len+=1;
}

typedef struct
{
  const char *str;
  enum TOK_TYPES type;
  size_t sub_type;
} token_declS;

static const token_declS special_tokens[] = {
  { "<-",  T_ASSIGN,   0 },
  { "==",  T_CMP,      ST_CMP_EQ },
  { "!=",  T_CMP,      ST_CMP_NE },
  { "<=",  T_CMP,      ST_CMP_LE },
  { ">=",  T_CMP,      ST_CMP_GE },
  { ">>>", T_RSHIFT,   0x8 },
  { ">>",  T_RSHIFT,   0x7 },
  { "<<",  T_LSHIFT,   0x6 },
  { "...", T_ELLIPSIS, 0 }
};


static const token_declS token_library[] = {
  { "$pc",          T_PC,            ST_PC_PC },
  { "$tpc",         T_PC,            ST_PC_TPC },
  { "fill",         T_INSN,          0x0000 },
  { "break",        T_INSN,          0x1000 },
  { "syscall",      T_INSN,          0x2000 },
  { "sii",          T_INSN,          0x6000 },
  { "hwi",          T_INSN,          0x7000 },
  { "stm",          T_INSN,          0x8000 },
  { "woi",          T_INSN,          0x9000 },
  { "nop",          T_INSN,          0x2222 },
  { "swi",          T_SWI,           0 },
  { "fence",        T_FENCE,         0x0001 },
  { "float",        T_PREFIX_OP,     0x7 },
  { "int",          T_PREFIX_OP,     0x8 },
  { "sum",          T_PREFIX_OP,     0xb },
  { "rsqrt",        T_PREFIX_OP,     0xa },
  { "bsi",          T_PREFIX_OP,     0x5 },
  { "wsi",          T_PREFIX_OP,     0x6 },
  { "-",            T_PLUS_MINUS,    0x5 },
  { "~",            T_TILDE,         0x4 },
  { "lane_swizzle", T_SWIZZLE,       0 },
  { "interpolate",  T_INTERPOLATE,   0 },
  { "^",            T_HAT,           0x1 },
  { "|",            T_BAR,           0x2 },
  { "+",            T_PLUS_MINUS,    0x4 },
  { "*",            T_STAR,          0x9 },
  { "&",            T_AND,           0x3 },
  { "/",            T_DIV,           0 },
  { "<",            T_CMP,           ST_CMP_LT },
  { ">",            T_CMP,           ST_CMP_GT },
  { "short",        T_SHORT,         0 },
  { "tiny",         T_TINY,          0 },
  { "full",         T_FULL,          0 },
  { "signed",       T_SIGNED,          0 },
  { "mem",          T_MEM,           ST_MEM_32 },
  { "mem32",        T_MEM,           ST_MEM_32 },
  { "mem16",        T_MEM,           ST_MEM_16 },
  { "mem8",         T_MEM,           ST_MEM_8 },
  { "smem16",       T_MEM,           ST_SMEM_16 },
  { "smem8",        T_MEM,           ST_SMEM_8 },
  { "memll",        T_MEM,           ST_MEM_LL },
  { "memsr",        T_MEM,           ST_MEM_SR },
  { "memll32",      T_MEM,           ST_MEM_LL },
  { "memsr32",      T_MEM,           ST_MEM_SR },
  { "inv",          T_MEM,           ST_MEM_INV },
  { ",",            T_COMMA,         0 },
  { "[",            T_LBRACKET,      0 },
  { "]",            T_RBRACKET,      0 },
  { "{",            T_LCURLY,        0 },
  { "}",            T_RCURLY,        0 },
  { "if",           T_IF,            0 },
  { "any",          T_IF_QUAL,       ST_IF_ANY },
  { "all",          T_IF_QUAL,       ST_IF_ALL },
  { "0",            T_ZERO,          0 },
  { "1",            T_ONE,           0 },
  { "type",         T_TYPE,          0 },
  // NOTE: these type names are repeated in brew-decode.c Keep them in sync!
  { "int32",        T_TYPENAME,      BREW_REG_TYPE_INT32 },
  { "int16x2",      T_TYPENAME,      BREW_REG_TYPE_INT16x2 },
  { "int8x4",       T_TYPENAME,      BREW_REG_TYPE_INT8x4 },
  { "uint16x2s",    T_TYPENAME,      BREW_REG_TYPE_UINT16x2S },
  { "sint16x2s",    T_TYPENAME,      BREW_REG_TYPE_SINT16x2S },
  { "uint8x4s",     T_TYPENAME,      BREW_REG_TYPE_UINT8x4S },
  { "sint8x4s",     T_TYPENAME,      BREW_REG_TYPE_SINT8x4S },
  { "fp32",         T_TYPENAME,      BREW_REG_TYPE_FP32 },
  { "fp16x2",       T_TYPENAME,      BREW_REG_TYPE_FP16x2 },
  { "mask",         T_TYPENAME,      BREW_REG_TYPE_MASK },
  // Named registers (aliases to the general purpose registers)
  { "$fp",          T_REG,           BREW_REG_FP },
  { "$sp",          T_REG,           BREW_REG_SP },
  { "$lr",          T_REG,           BREW_REG_LINK },
  { "$a0",          T_REG,           BREW_REG_ARG0 },
  { "$a1",          T_REG,           BREW_REG_ARG1 },
  { "$a2",          T_REG,           BREW_REG_ARG2 },
  { "$a3",          T_REG,           BREW_REG_ARG3 },
  { "$st",          T_REG,           BREW_REG_STATIC_CHAIN },
  { "$sen",         T_REG,           BREW_REG_SYSCALL_ERRNO },
  { "$sva",         T_REG,           BREW_REG_STRUCT_VAL_ADDR },
  { "$eh0",         T_REG,           BREW_REG_EH_RETURN_DATA0 },
  { "$eh1",         T_REG,           BREW_REG_EH_RETURN_DATA1 },
  // General purpose registers
  { "$r0",          T_REG,           0 },
  { "$r1",          T_REG,           1 },
  { "$r2",          T_REG,           2 },
  { "$r3",          T_REG,           3 },
  { "$r4",          T_REG,           4 },
  { "$r5",          T_REG,           5 },
  { "$r6",          T_REG,           6 },
  { "$r7",          T_REG,           7 },
  { "$r8",          T_REG,           8 },
  { "$r9",          T_REG,           9 },
  { "$r10",         T_REG,           10 },
  { "$r11",         T_REG,           11 },
  { "$r12",         T_REG,           12 },
  { "$r13",         T_REG,           13 },
  { "$r14",         T_REG,           14 },
};

brew_lexer_tokenS *brew_tokenize(const char *str)
{
  lexer_token_strmS tokens;

  lexer_token_strm_init(&tokens);


  int paren_cnt;

  const char *tok_start = str;
  const char *tok_end;
  size_t tok_len = 0;
  bool found;
  while(tok_start[0] != 0)
    {
      while (ISSPACE(*tok_start))
        tok_start++;

      tok_len = 0;
      tok_end = tok_start;
      // special multi-char tokens
      found = false;
      for (size_t i=0;i<ARRAY_SIZE(special_tokens);++i)
        {
          tok_len = strlen(special_tokens[i].str);
          if (strncmp(special_tokens[i].str, tok_start, tok_len) == 0)
            {
              lexer_token_strm_append_token(&tokens, special_tokens[i].type, special_tokens[i].sub_type, tok_len, tok_start);
              tok_start += tok_len;
              found = true;
              break;
            }
        }
      if (found)
        continue;

      // In general, we're reading up to a delimiter or white-space,
      // but we allow the use of parens to force tokenization
      paren_cnt = 0;
      while (true)
        {
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
          // These are tokens on their own right
          if (strchr("<>![]=,^&*+/~|-.", *tok_end) != NULL)
            {
              if (tok_end == tok_start)
                {
                  ++tok_end;
                }
              break;
            }
          // These are token terminators, but part of the token
          //if (strchr("-", *tok_end) != NULL)
          //  {
          //    ++tok_end;
          //    break;
          //  }
          tok_end++;
        };
      // At this point the next token starts at tok_start and the last
      // character of it is tok_end-1. However we don't know the type of the token!
      tok_len = tok_end - tok_start;
      if (tok_len == 0)
        break;
      found = false;
      for (size_t i=0;i<ARRAY_SIZE(token_library);++i)
        {
          //printf("comparing <%s> to <%s>\n", token_library[i].str, tok_start);
          if (strncmp(token_library[i].str, tok_start, tok_len) == 0)
            {
              lexer_token_strm_append_token(&tokens, token_library[i].type, token_library[i].sub_type, tok_len, tok_start);
              found = true;
              break;
            }
        }
      if (!found)
        {
          if (*tok_start == '(' && *tok_end == ')')
            lexer_token_strm_append_token(&tokens, T_UNKNOWN, 0, tok_len - 2, tok_start+1);
          else
            lexer_token_strm_append_token(&tokens, T_UNKNOWN, 0, tok_len, tok_start);
        }
      tok_start += tok_len;
    }
  lexer_token_strm_append_token(&tokens, T_NULL, 0, 0, NULL);
  return tokens.data;
}

const char *brew_tok_name(int tok_type)
{
  static char tmp_buf[200];
  if (tok_type < 0)
    {
      tok_type = ~tok_type;
      if ((size_t)tok_type < ARRAY_SIZE(tok_names))
        sprintf(tmp_buf, "~%s", tok_names[tok_type]);
      else
        sprintf(tmp_buf, "~%d", tok_type);
      return tmp_buf;
    }
  else
    {
      if ((size_t)tok_type < ARRAY_SIZE(tok_names))
        return tok_names[tok_type];
      sprintf(tmp_buf, "%d", tok_type);
      return tmp_buf;
    }
}

void brew_dump_lexer_token(FILE *strm, const brew_lexer_tokenS *t)
{
  fprintf(strm, "%-15s {%.*s}", brew_tok_name(t->type), (int)t->len, t->start);
}
