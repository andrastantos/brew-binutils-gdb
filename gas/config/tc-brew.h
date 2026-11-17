/* tc-brew.h -- Header file for tc-brew.c.

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

#define TC_BREW 1
/* You should define this macro to be non-zero if the target is big
   endian, and zero if the target is little endian.  */
#ifndef TARGET_BYTES_BIG_ENDIAN
#define TARGET_BYTES_BIG_ENDIAN 0
#endif
#define WORKING_DOT_WORD

/* This macro is the BFD architecture to pass to `bfd_set_arch_mach'.  */
#define TARGET_FORMAT "elf32-brew"

#define TARGET_ARCH bfd_arch_brew

#define md_undefined_symbol(NAME)           0

/* These macros must be defined, but it will be a fatal assembler
   error if we ever hit them.  */
#define md_estimate_size_before_relax(A, B) (as_fatal (_("estimate size\n")), 0)
#define md_convert_frag(B, S, F)            as_fatal (_("convert_frag\n"))

#define md_section_align(SEGMENT, SIZE)     (SIZE)

// Symbol differences (A - B):
// - Same section: resolved by the assembler (the generic
//   TC_FORCE_RELOCATION_SUB_SAME). The linker doesn't relax, so
//   in-section deltas can't change at link time. Brew used to inherit
//   RISC-V's override, which keeps code-section differences as fixups.
//   Brew relocations can't encode a subtrahend, so tc_gen_reloc emitted
//   just A, and e.g. DW_AT_high_pc and .debug_aranges lengths
//   (.Letext0-.Ltext0) became end addresses.
// - B in the fixup's own section: not turned into a pc-relative
//   fixup (the only pc-relative relocation is the branch offset).
// - Anything else can't be expressed, and the generic
//   TC_VALIDATE_FIX_SUB reports it as an error.
#define TC_FORCE_RELOCATION_SUB_LOCAL(FIX, SEG) 1
#define TC_FORCE_RELOCATION_LOCAL(FIX) 1
#define DIFF_EXPR_OK 1
