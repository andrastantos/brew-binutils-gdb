/* tc-brew.c -- Assembly parser for Brew
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

#ifndef _TC_BREW_PARSE_H
#define _TC_BREW_PARSE_H

typedef __PTRDIFF_TYPE__ brew_parser_tok_type_t;

typedef struct
{
  brew_parser_tok_type_t parser_token;
  const brew_lexer_tokenS *first_lexer_token;
  const brew_lexer_tokenS *last_lexer_token;
} brew_parser_tokenS;

typedef bool (*brew_parser_action)(void *, brew_parser_tokenS *);

typedef struct _brew_parserS* brew_parser_t;

#define ACTION(fn) ((brew_parser_tok_type_t)(fn)), T_NULL
#define PATTERN(...) __VA_ARGS__, T_NULL

extern brew_parser_t brew_parser_init(const brew_parser_tok_type_t *raw_insn, size_t raw_insn_size);
extern void brew_parser_done(const brew_parser_t parser);
// returns -1 for no match or whatever the action function returns
extern int brew_parse(const brew_parser_t parser, const brew_lexer_tokenS *tokens, void *context);

extern void brew_dump_parsed_tokens(FILE *strm, brew_parser_tokenS *tokens);
extern void brew_dump_parser(FILE *strm, const brew_parser_t parser);

#endif
