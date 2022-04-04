/* tc-brew-lex.h -- Header file for tc-brew-ex.c.

   A simple lexer (tokenizer) for the BREW assembly language.

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

#ifndef _TC_BREW_LEX_H
#define _TC_BREW_LEX_H

enum TOK_TYPES
{
  T_NULL = 0,
  T_REG,
  T_PC,
  T_INSN,
  T_SWI,
  T_FENCE,
  T_ASSIGN,
  T_PREFIX_OP,
  T_MINUS,
  T_TILDE,
  T_SWIZZLE,
  T_INTERPOLATE,
  T_HAT,
  T_BAR,
  T_LSHIFT,
  T_RSHIFT,
  T_PLUS,
  T_STAR,
  T_AND,
  T_DIV,
  T_SHORT,
  T_TINY,
  T_FULL,
  T_MEM,
  T_COMMA,
  T_LBRACKET,
  T_RBRACKET,
  T_LCURLY,
  T_RCURLY,
  T_IF,
  T_IF_QUAL,
  T_CMP,
  T_ZERO,
  T_ONE,
  T_TYPE,
  T_TYPENAME,
  T_SIGNED,
  T_UNKNOWN
};


enum TOK_SUBTYPE_CMP {
  ST_CMP_EQ,
  ST_CMP_NE,
  ST_CMP_LT,
  ST_CMP_LE,
  ST_CMP_GT,
  ST_CMP_GE
};

enum TOK_SUBTYPE_PC {
  ST_PC_PC,
  ST_PC_TPC
};

enum TOK_SUBTYPE_MEM {
  ST_MEM_32,
  ST_MEM_16,
  ST_MEM_8,
  ST_SMEM_16,
  ST_SMEM_8,
  ST_MEM_LL,
  ST_MEM_SR,
  ST_MEM_INV
};

enum TOK_SUBTYPE_IF {
  ST_IF_ANY,
  ST_IF_ALL
};

typedef struct
{
  enum TOK_TYPES type;
  size_t sub_type;
  size_t len;
  const char *start;
} brew_lexer_tokenS;

extern brew_lexer_tokenS *brew_tokenize(const char *str);
extern const char *brew_tok_name(int tok_type);
extern void brew_dump_lexer_token(FILE *strm, const brew_lexer_tokenS *t);

#endif
