/* brew core-specific support for 32-bit ELF.
   Copyright (C) 2009-2021 Free Software Foundation, Inc.

   Copied from elf32-fr30.c which is..
   Copyright (C) 1998-2021 Free Software Foundation, Inc.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/brew.h"

/* Forward declarations.  */

static reloc_howto_type brew_elf_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (
    R_BREW_NONE,                /* type */
    0,                          /* rightshift */
    3,                          /* size (0 = byte, 1 = short, 2 = long) */
    0,                          /* bitsize */
    false,                      /* pc_relative */
    0,                          /* bitpos */
    complain_overflow_dont,     /* complain_on_overflow */
    bfd_elf_generic_reloc,      /* special_function */
    "R_BREW_NONE",              /* name */
    false,                      /* partial_inplace */
    0,                          /* src_mask */
    0,                          /* dst_mask */
    false                       /* pcrel_offset */
  ),

  /* A 32 bit absolute relocation.  */
  HOWTO (
    R_BREW_32,                  /* type */
    0,                          /* rightshift */
    2,                          /* size (0 = byte, 1 = short, 2 = long) */
    32,                         /* bitsize */
    false,                      /* pc_relative */
    0,                          /* bitpos */
    complain_overflow_unsigned, /* complain_on_overflow */
    bfd_elf_generic_reloc,      /* special_function */
    "R_BREW_32",                /* name */
    false,                      /* partial_inplace */
    0x00000000,                 /* src_mask */
    0xffffffff,                 /* dst_mask */
    false                       /* pcrel_offset */
  ),

  /* A 32 bit negative absolute relocation.  */
  HOWTO (
    R_BREW_NEG32,               /* type */
    0,                          /* rightshift */
    -2,                         /* size (0 = byte, 1 = short, 2 = long) negative: negate*/
    32,                         /* bitsize */
    false,                      /* pc_relative */
    0,                          /* bitpos */
    complain_overflow_unsigned, /* complain_on_overflow */
    bfd_elf_generic_reloc,      /* special_function */
    "R_BREW_NEG32",             /* name */
    false,                      /* partial_inplace */
    0x00000000,                 /* src_mask */
    0xffffffff,                 /* dst_mask */
    false                       /* pcrel_offset */
  ),

  /* A 16 bit absolute relocation.  */
  HOWTO (
    R_BREW_16,                  /* type */
    0,                          /* rightshift */
    1,                          /* size (0 = byte, 1 = short, 2 = long) */
    16,                         /* bitsize */
    false,                      /* pc_relative */
    0,                          /* bitpos */
    complain_overflow_unsigned, /* complain_on_overflow */
    bfd_elf_generic_reloc,      /* special_function */
    "R_BREW_16",                /* name */
    false,                      /* partial_inplace */
    0x0000,                     /* src_mask */
    0xffff,                     /* dst_mask */
    false                       /* pcrel_offset */
  ),

  /* A 32 bit negative absolute relocation.  */
  HOWTO (
    R_BREW_NEG16,               /* type */
    0,                          /* rightshift */
    -1,                         /* size (0 = byte, 1 = short, 2 = long) */
    16,                         /* bitsize */
    false,                      /* pc_relative */
    0,                          /* bitpos */
    complain_overflow_unsigned, /* complain_on_overflow */
    bfd_elf_generic_reloc,      /* special_function */
    "R_BREW_NEG16",             /* name */
    false,                      /* partial_inplace */
    0x0000,                     /* src_mask */
    0xffff,                     /* dst_mask */
    false                       /* pcrel_offset */
  ),


  /* 16 bit PC relative offset.  */
  HOWTO (
    R_BREW_16_SPCREL,           /* type */
    1,                          /* rightshift */
    1,                          /* size (0 = byte, 1 = short, 2 = long) */
    16,                         /* bitsize */
    true,                       /* pc_relative */
    0,                          /* bitpos */
    complain_overflow_signed,   /* complain_on_overflow */
    NULL,                       /* special_function */
    "R_BREW_16_SPCREL",         /* name */
    false,                      /* partial_inplace */
    0x0,                        /* src_mask */
    0xffff,                     /* dst_mask */
    // TODO: not sure what the right value here is.
    true                        /* pcrel_offset */
  ),


  /* A 7 bit absolute relocation.  */
  HOWTO (
    R_BREW_7,                   /* type */
    1,                          /* rightshift */
    2,                          /* size (0 = byte, 1 = short, 2 = long) */
    7,                          /* bitsize */
    false,                      /* pc_relative */
    1,                          /* bitpos */
    complain_overflow_unsigned, /* complain_on_overflow */
    bfd_elf_generic_reloc,      /* special_function */
    "R_BREW_7",                 /* name */
    false,                      /* partial_inplace */
    0xff01,                     /* src_mask */
    0x00fe,                     /* dst_mask */
    false                       /* pcrel_offset */
  ),

  /* A 7 bit negative absolute relocation.  */
  HOWTO (
    R_BREW_NEG7,                /* type */
    2,                          /* rightshift */
    -2,                         /* size (0 = byte, 1 = short, 2 = long) */
    7,                          /* bitsize */
    false,                      /* pc_relative */
    1,                          /* bitpos */
    complain_overflow_unsigned, /* complain_on_overflow */
    bfd_elf_generic_reloc,      /* special_function */
    "R_BREW_NEG7",              /* name */
    false,                      /* partial_inplace */
    0xff01,                     /* src_mask */
    0x00fe,                     /* dst_mask */
    false                       /* pcrel_offset */
  ),

};

/* Map BFD reloc types to BREW ELF reloc types.  */

struct brew_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned int brew_reloc_val;
};

static const struct brew_reloc_map brew_reloc_map[] =
{
  { BFD_RELOC_NONE,           R_BREW_NONE },
  { BFD_RELOC_32,             R_BREW_32 },
  { BFD_RELOC_BREW_NEG32,     R_BREW_NEG32 },
  { BFD_RELOC_16,             R_BREW_16 },
  { BFD_RELOC_BREW_NEG16,     R_BREW_NEG16 },
  { BFD_RELOC_BREW_PCREL16,   R_BREW_16_SPCREL },
  { BFD_RELOC_BREW_7,         R_BREW_7 },
  { BFD_RELOC_BREW_NEG7,      R_BREW_NEG7 }
};

static reloc_howto_type *
brew_reloc_type_lookup(
  bfd *abfd ATTRIBUTE_UNUSED,
  bfd_reloc_code_real_type code
) {
  unsigned int i;

  for (i = BREW_ARRAY_SIZE(brew_reloc_map); i--;)
    if (brew_reloc_map[i].bfd_reloc_val == code)
      return &brew_elf_howto_table[brew_reloc_map[i].brew_reloc_val];

  return NULL;
}

static reloc_howto_type *
brew_reloc_name_lookup(
  bfd *abfd ATTRIBUTE_UNUSED,
  const char *r_name
) {
  unsigned int i;

  for (i = 0; i < BREW_ARRAY_SIZE(brew_elf_howto_table); i++)
    if (
      brew_elf_howto_table[i].name != NULL &&
      strcasecmp(brew_elf_howto_table[i].name, r_name) == 0
    )
      return &brew_elf_howto_table[i];

  return NULL;
}

/* Set the howto pointer for an bre ELF reloc.  */
static bool
brew_info_to_howto_rela (
  bfd *abfd,
  arelent *cache_ptr,
  Elf_Internal_Rela *dst
) {
  unsigned int r_type;

  r_type = ELF32_R_TYPE(dst->r_info);
  if (r_type >= (unsigned int) R_BREW_max)
    {
      /* xgettext:c-format */
      _bfd_error_handler (
        _("%pB: unsupported relocation type %#x"),
        abfd,
        r_type
      );
      bfd_set_error(bfd_error_bad_value);
      return false;
    }
  cache_ptr->howto = &brew_elf_howto_table[r_type];
  return true;
}

/* Perform a single relocation. For now we use the standard BFD routines */
static bfd_reloc_status_type
brew_final_link_relocate (
  reloc_howto_type *howto,
  bfd *input_bfd,
  asection *input_section,
  bfd_byte *contents,
  Elf_Internal_Rela *rel,
  bfd_vma relocation
) {
  bfd_reloc_status_type r = bfd_reloc_ok;

  switch (howto->type)
    {
    case R_BREW_16_SPCREL:
      fprintf(stderr, "FIXME: %s:%d do we need the +2 here?\n", __FILE__, __LINE__);
      r = _bfd_final_link_relocate(
        howto,
        input_bfd,
        input_section,
        contents,
        rel->r_offset,
        //relocation + 2,
        relocation,
        rel->r_addend
      );
      break;
    default:
      r = _bfd_final_link_relocate(
        howto,
        input_bfd,
        input_section,
        contents,
        rel->r_offset,
        relocation,
        rel->r_addend
      );
      break;
    }

  return r;
}

/* Relocate an BREW ELF section.

   The RELOCATE_SECTION function is called by the new ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjusting the section contents as
   necessary, and (if using Rela relocs and generating a relocatable
   output file) adjusting the reloc addend as necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocatable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.  */

static int
brew_elf_relocate_section (
  bfd *output_bfd,
  struct bfd_link_info *info,
  bfd *input_bfd,
  asection *input_section,
  bfd_byte *contents,
  Elf_Internal_Rela *relocs,
  Elf_Internal_Sym *local_syms,
  asection **local_sections
) {
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;
      const char *name;
      int r_type;

      r_type   = ELF32_R_TYPE (rel->r_info);
      r_symndx = ELF32_R_SYM (rel->r_info);
      howto    = brew_elf_howto_table + r_type;
      h        = NULL;
      sym      = NULL;
      sec      = NULL;

      if (r_symndx < symtab_hdr->sh_info)
        {
          sym = local_syms + r_symndx;
          sec = local_sections [r_symndx];
          relocation = _bfd_elf_rela_local_sym(output_bfd, sym, &sec, rel);

          name = bfd_elf_string_from_elf_section(
            input_bfd,
            symtab_hdr->sh_link,
            sym->st_name
          );
          name = name == NULL ? bfd_section_name (sec) : name;
        }
      else
        {
          bool unresolved_reloc, warned, ignored;

          RELOC_FOR_GLOBAL_SYMBOL(
            info,
            input_bfd,
            input_section,
            rel,
            r_symndx,
            symtab_hdr,
            sym_hashes,
            h, sec,
            relocation,
            unresolved_reloc,
            warned,
            ignored
          );

          name = h->root.root.string;
        }

      if (sec != NULL && discarded_section (sec))
        RELOC_AGAINST_DISCARDED_SECTION(
          info,
          input_bfd,
          input_section,
          rel,
          1,
          relend,
          howto,
          0,
          contents
        );

      if (bfd_link_relocatable (info))
        continue;

      r = brew_final_link_relocate(
        howto,
        input_bfd,
        input_section,
        contents,
        rel,
        relocation
      );

      if (r != bfd_reloc_ok)
        {
          const char * msg = NULL;

          switch (r)
            {
            case bfd_reloc_overflow:
              (*info->callbacks->reloc_overflow)(
                info,
                (h ? &h->root : NULL),
                name,
                howto->name,
                (bfd_vma) 0,
                input_bfd,
                input_section,
                rel->r_offset
              );
              break;

            case bfd_reloc_undefined:
              (*info->callbacks->undefined_symbol)(
                info,
                name,
                input_bfd,
                input_section,
                rel->r_offset,
                true
              );
              break;

            case bfd_reloc_outofrange:
              msg = _("internal error: out of range error");
              break;

            case bfd_reloc_notsupported:
              msg = _("internal error: unsupported relocation error");
              break;

            case bfd_reloc_dangerous:
              msg = _("internal error: dangerous relocation");
              break;

            default:
              msg = _("internal error: unknown error");
              break;
            }

          if (msg)
            (*info->callbacks->warning)(
              info,
              msg,
              name,
              input_bfd,
              input_section,
              rel->r_offset
            );
        }
    }

  return true;
}

/* Return the section that should be marked against GC for a given relocation. */
static asection *
brew_elf_gc_mark_hook(
  asection *sec,
  struct bfd_link_info *info,
  Elf_Internal_Rela *rel,
  struct elf_link_hash_entry *h,
  Elf_Internal_Sym *sym
) {
  return _bfd_elf_gc_mark_hook (sec, info, rel, h, sym);
}

/* Look through the relocs for a section during the first phase.
   Since we don't do .gots or .plts, we just need to consider the
   virtual table relocs for gc. */
static bool
brew_elf_check_relocs(
  bfd *abfd,
  struct bfd_link_info *info,
  asection *sec,
  const Elf_Internal_Rela *relocs
) {
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  if (bfd_link_relocatable (info))
    return true;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
        {
          h = sym_hashes[r_symndx - symtab_hdr->sh_info];
          while (
            h->root.type == bfd_link_hash_indirect ||
            h->root.type == bfd_link_hash_warning
          )
            h = (struct elf_link_hash_entry *) h->root.u.i.link;
        }
    }

  return true;
}

#define ELF_ARCH                         bfd_arch_brew
#define ELF_MACHINE_CODE                 EM_BREW
#define ELF_MAXPAGESIZE                  0x1000

#define TARGET_LITTLE_SYM                brew_elf32_vec
#define TARGET_LITTLE_NAME               "elf32-brew"

#define elf_info_to_howto_rel            NULL
#define elf_info_to_howto                brew_info_to_howto_rela
#define elf_backend_relocate_section     brew_elf_relocate_section
#define elf_backend_gc_mark_hook         brew_elf_gc_mark_hook
#define elf_backend_check_relocs         brew_elf_check_relocs

#define elf_backend_can_gc_sections      1
#define elf_backend_rela_normal          1

#define bfd_elf32_bfd_reloc_type_lookup  brew_reloc_type_lookup
#define bfd_elf32_bfd_reloc_name_lookup  brew_reloc_name_lookup

#include "elf32-target.h"
