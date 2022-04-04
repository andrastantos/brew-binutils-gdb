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

#include "as.h"
#include "safe-ctype.h"
#include <stdint.h>
#include "tc-brew-lex.h"
#include "tc-brew-parse.h"
#include "tc-brew-int.h"

typedef struct
{
  const brew_parser_tok_type_t *pattern;
  brew_parser_action action;
} insn_patternS;

typedef struct _brew_parserS
{
  insn_patternS *insn_patterns;
  size_t insn_pattern_cnt;
} brew_parserS;

// Used by qsort below to order patterns
// in the right way for faster find
static int pattern_cmp(const void *a, const void *b)
{
  const insn_patternS *p_a = (insn_patternS*)a;
  const insn_patternS *p_b = (insn_patternS*)b;
  const brew_parser_tok_type_t *t_a = p_a->pattern;
  const brew_parser_tok_type_t *t_b = p_b->pattern;
  while (*t_a == *t_b)
    {
      ++t_a;
      ++t_b;
    }
  // Negative values represent 'all-but-this' wildcard patterns.
  // We want them to come after all positive values.
  if (*t_a < 0 && *t_b < 0)
    return *t_a - *t_b;
  if (*t_a >= 0 && *t_b >= 0)
    return *t_a - *t_b;
  if (*t_a < 0)
    return 1;
  return -1;
}

brew_parser_t brew_parser_init(const brew_parser_tok_type_t *raw_insn, size_t raw_insn_size)
{
  // Determine the size number of patterns
  size_t i;
  bool in_pattern;
  size_t pattern_idx;

  brew_parser_t parser = NULL;
  int insn_pattern_cnt = 0;

  for (i=0;i<raw_insn_size;++i)
    {
      if (raw_insn[i] == T_NULL)
        ++insn_pattern_cnt;
    }
  // Each pattern has two part: a pattern and an action, each T_NULL-terminated.
  gas_assert((insn_pattern_cnt & 1) == 0);
  insn_pattern_cnt = insn_pattern_cnt / 2;

  parser = (brew_parser_t)malloc(sizeof(brew_parserS));
  parser->insn_patterns = (insn_patternS*)malloc(sizeof(insn_patternS)*insn_pattern_cnt);
  parser->insn_pattern_cnt = insn_pattern_cnt;

  // Now do the real populating...
  in_pattern = true;
  pattern_idx = 0;
  parser->insn_patterns[pattern_idx].pattern = &raw_insn[0];
  for (i=0;i<raw_insn_size;++i)
    {
      if (in_pattern) {
        // A simple consistency-check: wild-card matches are always of type ~T_<something>, T_<something>
        if (raw_insn[i] < 0) {
          gas_assert(~raw_insn[i] == raw_insn[i+1]);
        }
      }
      if (raw_insn[i] == T_NULL)
        {
          if (!in_pattern)
            {
              // The action pointer is the previous item
              parser->insn_patterns[pattern_idx].action = (brew_parser_action)raw_insn[i-1];
              // Advance to the next pattern and init it
              ++pattern_idx;
              if (pattern_idx != parser->insn_pattern_cnt)
                parser->insn_patterns[pattern_idx].pattern = &raw_insn[i+1];
            }
          in_pattern = !in_pattern;
        }
    }

  qsort(parser->insn_patterns, parser->insn_pattern_cnt, sizeof(insn_patternS), pattern_cmp);
  return parser;
}

void brew_parser_done(const brew_parser_t parser)
{
  if (parser->insn_patterns != NULL)
    free(parser->insn_patterns);
  free(parser);
}

// Finds pattern that matches given tokens and executes associated action
// If no pattern matches, return false, otherwise returns whatever
// the action returns.
// returns -1 for no match or whatever the action function returns
int brew_parse(const brew_parser_t parser, const brew_lexer_tokenS *tokens, void *context)
{
  size_t first_match_idx = 0;
  size_t last_match_idx = parser->insn_pattern_cnt-1;
  size_t tok_idx = 0;
  // We will start scanning the token stream and compare it
  // to the stored patterns. The patterns are sorted,
  // which means that (discarding wild-card matches for a moment)
  // there is a region of patterns that match all the so-far
  // encountered tokens (the prefix). All patterns in this region
  // share the same prefix and all the patterns that share this
  // prefix must be in a contiguous region due to sorting.
  // We just have to maintain first_match_idx and
  // last_match_idx to bracket this region (inclusive).
  //
  // When we encounter a wild-card match in the patterns,
  // we can (in fact we should assert) that all the tokens
  // within the region contain a wild-card token, but they
  // might be different ones. At this point each remaining
  // candidate pattern is tried independently using greedy match.
  //
  // NOTE: we have a simplifying rule that all wild-card
  // matches are of the type: ~T_xxx, T_xxx. This means that
  // no back-tracking is needed even in the case of
  // multiple wild-card tokens in the pattern.
  //
  // While we're matching, we're creating a matching token stream.
  // This allows the action to quickly map pattern tokens to
  // input token ranges.

  int ret_val = -1;
  size_t tokens_len=0;
  while (tokens[tokens_len].type != T_NULL)
    ++tokens_len;
  brew_parser_tokenS *match_tokens = (brew_parser_tokenS *)malloc(sizeof(brew_parser_tokenS) * (tokens_len+1));

  while (first_match_idx <= last_match_idx)
    {
      // TODO: these searches could be binary ones, but I'm lazy...
      while(
        first_match_idx < last_match_idx &&
        parser->insn_patterns[first_match_idx].pattern[tok_idx] != tokens[tok_idx].type &&
        parser->insn_patterns[first_match_idx].pattern[tok_idx] > 0
      ) {
        ++first_match_idx;
      }
      while(
        first_match_idx < last_match_idx &&
        parser->insn_patterns[last_match_idx].pattern[tok_idx] != tokens[tok_idx].type &&
        parser->insn_patterns[last_match_idx].pattern[tok_idx] > 0
      ) {
        --last_match_idx;
      }
      // If we have a single candidate, and it doesn't match, we're done...
      if (first_match_idx == last_match_idx)
        {
          //DEBUG("$\n");
          if (
            parser->insn_patterns[first_match_idx].pattern[tok_idx] != tokens[tok_idx].type &&
            parser->insn_patterns[first_match_idx].pattern[tok_idx] > 0
          ) {
            goto RETURN;
          }
        }
      // If we're into a wild-card match, let's do that
      if (parser->insn_patterns[first_match_idx].pattern[tok_idx] < 0 || parser->insn_patterns[last_match_idx].pattern[tok_idx] < 0)
      {
        gas_assert(parser->insn_patterns[last_match_idx].pattern[tok_idx] < 0);
        size_t prefix_size = tok_idx;
        for(;first_match_idx<=last_match_idx;++first_match_idx)
          {
            //DEBUG(" >> matching pattern %ld\n", first_match_idx);
            size_t pattern_idx = prefix_size; // We have to split the indices: due to the wild-card, the pattern and the token stream don't stay in sync.
            tok_idx = prefix_size; // Always restart the search from the prefix we know matches
            const brew_parser_tok_type_t *pattern = parser->insn_patterns[first_match_idx].pattern;

            match_tokens[pattern_idx].parser_token = pattern[pattern_idx];
            match_tokens[pattern_idx].first_lexer_token = NULL;
            match_tokens[pattern_idx].last_lexer_token = NULL;

            //gas_assert(pattern[pattern_idx] < 0);
            while(true)
              {
                if (pattern[pattern_idx] < 0)
                  {
                    // wild-card match
                    if (
                      ~pattern[pattern_idx] != tokens[tok_idx].type &&
                      tokens[tok_idx].type != T_NULL
                    )
                      {
                        // Haven't found the end of the wild-card
                        if ( match_tokens[pattern_idx].first_lexer_token == NULL)
                          match_tokens[pattern_idx].first_lexer_token = &tokens[tok_idx];
                        ;
                        match_tokens[pattern_idx].last_lexer_token = &tokens[tok_idx];
                        //DEBUG("    W ");
                        //DEBUG_BEGIN
                        //  brew_dump_lexer_token(stderr, &tokens[tok_idx]);
                        //DEBUG_END
                        //DEBUG("   pattern_idx: %ld", pattern_idx); DEBUG("\n");
                        ++tok_idx;
                      }
                    else if (~pattern[pattern_idx] == tokens[tok_idx].type)
                      {
                        // Haven't found the end of the wild-card
                        if ( match_tokens[pattern_idx].first_lexer_token == NULL)
                          match_tokens[pattern_idx].first_lexer_token = &tokens[tok_idx];
                        ;
                        match_tokens[pattern_idx].last_lexer_token = &tokens[tok_idx];
                        // This is the end of the wild-card (well, rather, the previous token was)
                        // It is a pre-requisite of the pattern that after a wild-card, the next
                        // pattern token must be the inverse of the wild-card, we could fill-in both here.
                        // However, that would duplicate the termination check and action, so it's easier
                        // to back-track one token (i.e. not increment tok_idx) and let the normal
                        // match process to take place.
                        //DEBUG("    ----- end of W ----\n");
                        ++pattern_idx;
                        // Init the next token match in case it turns out to be a wild-card one
                        match_tokens[pattern_idx].parser_token = pattern[pattern_idx];
                        match_tokens[pattern_idx].first_lexer_token = NULL;
                        match_tokens[pattern_idx].last_lexer_token = NULL;
                      }
                    else
                      {
                        gas_assert(tokens[tok_idx].type == T_NULL);
                        //DEBUG("    ----- mismatch ----\n");
                        break;
                      }
                  }
                else
                  {
                    // normal match
                    if (pattern[pattern_idx] != tokens[tok_idx].type)
                      break;
                    //DEBUG("   WM ");
                    //DEBUG_BEGIN
                    //  brew_dump_lexer_token(stderr, &tokens[tok_idx]);
                    //DEBUG_END
                    //DEBUG("   pattern_idx: %ld", pattern_idx);
                    //DEBUG("\n");
                    match_tokens[pattern_idx].parser_token = pattern[pattern_idx];
                    match_tokens[pattern_idx].first_lexer_token = &tokens[tok_idx];
                    match_tokens[pattern_idx].last_lexer_token = &tokens[tok_idx];
                    if (tokens[tok_idx].type == T_NULL)
                      {
                        // We have a match!
                        DEBUG("  >> parse rule %ld matched\n", first_match_idx);
                        ret_val = parser->insn_patterns[first_match_idx].action(context, match_tokens);
                        goto RETURN;
                      }
                    ++tok_idx;
                    ++pattern_idx;
                    // Init the next token match in case it turns out to be a wild-card one
                    match_tokens[pattern_idx].parser_token = pattern[pattern_idx];
                    match_tokens[pattern_idx].first_lexer_token = NULL;
                    match_tokens[pattern_idx].last_lexer_token = NULL;
                  }
              }; // end of match for this pattern
          } // end of loop through all viable patterns
          // We have no match
          //DEBUG("W bail\n");
          goto RETURN;
      } // end of if wild-card
      gas_assert(parser->insn_patterns[last_match_idx].pattern[tok_idx] == tokens[tok_idx].type);
      gas_assert(parser->insn_patterns[first_match_idx].pattern[tok_idx] == tokens[tok_idx].type);
      match_tokens[tok_idx].parser_token = parser->insn_patterns[first_match_idx].pattern[tok_idx];
      match_tokens[tok_idx].first_lexer_token = &tokens[tok_idx];
      match_tokens[tok_idx].last_lexer_token = &tokens[tok_idx];
      //DEBUG("    M ");
      //DEBUG_BEGIN
      //  brew_dump_lexer_token(stderr, &tokens[tok_idx]);
      //DEBUG_END
      //DEBUG("\n");
      if (tokens[tok_idx].type == T_NULL)
        {
          gas_assert(first_match_idx == last_match_idx);
          DEBUG("  >> parse rule %ld matched\n", first_match_idx);
          ret_val = parser->insn_patterns[first_match_idx].action(context, match_tokens);
          goto RETURN;
        }
      ++tok_idx;
    }
RETURN:
    free(match_tokens);
    return ret_val;
}


/////////////////////////////////////////
//            DUMP ROUTINES            //
/////////////////////////////////////////

static void dump_pattern(FILE *strm, const insn_patternS *pattern)
{
  bool first = true;
  fprintf(strm, "{");
  const brew_parser_tok_type_t *tokens = pattern->pattern;
  while (*tokens != T_NULL)
    {
      if (!first)
        fprintf(strm, ", ");
      if (*tokens < 0)
        {
          fprintf(strm, "~%s", brew_tok_name(~*tokens));
        }
      else
        {
          fprintf(strm, "%s", brew_tok_name(*tokens));
        }
      ++tokens;
      first = false;
    }
  fprintf(strm, "}");
  fprintf(strm, " -> %p\n", pattern->action);
}

void brew_dump_parser(FILE *strm, const brew_parser_t parser)
{
  const insn_patternS *pattern = parser->insn_patterns;
  fprintf(strm, "========= PATTERNS ==========\n");
  for(size_t i=0;i<parser->insn_pattern_cnt;++i)
    {
      fprintf(strm, "     %2ld ", i);
      dump_pattern(strm, pattern);
      ++pattern;
    }
}

void brew_dump_parsed_tokens(FILE *strm, const brew_parser_tokenS *tokens)
{
  const brew_parser_tokenS *t = tokens;
  while (true)
    {
      fprintf(strm, "    %-15s\n", brew_tok_name(t->parser_token));
      const brew_lexer_tokenS *tt = t->first_lexer_token;
      do
        {
          fprintf(strm, "            "); brew_dump_lexer_token(strm, tt); fprintf(strm, "\n");
          if (tt != t->last_lexer_token)
            ++tt;
        }
      while (tt != t->last_lexer_token);
      if (t->parser_token == T_NULL)
        break;
      t++;
    }
}

