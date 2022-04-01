/* tc-brew.c -- Internal definitions for Brew
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

#ifndef _TC_BREW_INT_H
#define _TC_BREW_INT_H

#define DEBUG(...) { if (brew_trace) {fprintf (stderr, __VA_ARGS__); fprintf(stderr, "\n");} }
#define DEBUG_BEGIN { if (brew_trace) {
#define DEBUG_END }}

extern bool brew_trace;

#endif
