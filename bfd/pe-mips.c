/* BFD back-end for MIPS PE COFF files.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000, 2001 Free Software Foundation, Inc.
   Modified from coff-i386.c by DJ Delorie, dj@cygnus.com

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define COFF_WITH_PE
#define COFF_LONG_SECTION_NAMES
#define PCRELOFFSET true

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#include "coff/mipspe.h"

#include "coff/internal.h"

#include "coff/pe.h"

#include "libcoff.h"

static bfd_reloc_status_type coff_mips_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *coff_mips_rtype_to_howto
  PARAMS ((bfd *, asection *, struct internal_reloc *,
	   struct coff_link_hash_entry *, struct internal_syment *,

	   bfd_vma *));
#if 0
static void mips_ecoff_swap_reloc_in PARAMS ((bfd *, PTR,
					      struct internal_reloc *));
static void mips_ecoff_swap_reloc_out PARAMS ((bfd *,
					       const struct internal_reloc *,
					       PTR));
static void mips_adjust_reloc_in PARAMS ((bfd *,
					  const struct internal_reloc *,
					  arelent *));
static void mips_adjust_reloc_out PARAMS ((bfd *, const arelent *,
					   struct internal_reloc *));
#endif

static boolean in_reloc_p PARAMS ((bfd *, reloc_howto_type *));
static reloc_howto_type * coff_mips_reloc_type_lookup PARAMS ((bfd *, bfd_reloc_code_real_type));
static void mips_swap_reloc_in PARAMS ((bfd *, PTR, PTR));
static unsigned int mips_swap_reloc_out PARAMS ((bfd *, PTR, PTR));
static boolean coff_pe_mips_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *, bfd_byte *,
	   struct internal_reloc *, struct internal_syment *, asection **));

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (2)
/* The page size is a guess based on ELF.  */

#define COFF_PAGE_SIZE 0x1000

/* For some reason when using mips COFF the value stored in the .text
   section for a reference to a common symbol is the value itself plus
   any desired offset.  Ian Taylor, Cygnus Support.  */

/* If we are producing relocateable output, we need to do some
   adjustments to the object file that are not done by the
   bfd_perform_relocation function.  This function is called by every
   reloc type to make any required adjustments.  */

static bfd_reloc_status_type
coff_mips_reloc (abfd, reloc_entry, symbol, data, input_section, output_bfd,
		 error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section ATTRIBUTE_UNUSED;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  symvalue diff;

  if (output_bfd == (bfd *) NULL)
    return bfd_reloc_continue;

  if (bfd_is_com_section (symbol->section))
    {
#ifndef COFF_WITH_PE
      /* We are relocating a common symbol.  The current value in the
	 object file is ORIG + OFFSET, where ORIG is the value of the
	 common symbol as seen by the object file when it was compiled
	 (this may be zero if the symbol was undefined) and OFFSET is
	 the offset into the common symbol (normally zero, but may be
	 non-zero when referring to a field in a common structure).
	 ORIG is the negative of reloc_entry->addend, which is set by
	 the CALC_ADDEND macro below.  We want to replace the value in
	 the object file with NEW + OFFSET, where NEW is the value of
	 the common symbol which we are going to put in the final
	 object file.  NEW is symbol->value.  */
      diff = symbol->value + reloc_entry->addend;
#else
      /* In PE mode, we do not offset the common symbol.  */
      diff = reloc_entry->addend;
#endif
    }
  else
    {
      /* For some reason bfd_perform_relocation always effectively
	 ignores the addend for a COFF target when producing
	 relocateable output.  This seems to be always wrong for 386
	 COFF, so we handle the addend here instead.  */
      diff = reloc_entry->addend;
    }

#ifdef COFF_WITH_PE
#if 0
  /* dj - handle it like any other reloc? */
  /* FIXME: How should this case be handled?  */
  if (reloc_entry->howto->type == MIPS_R_RVA && diff != 0)
    abort ();
#endif
#endif

#define DOIT(x) \
  x = ((x & ~howto->dst_mask) | (((x & howto->src_mask) + (diff >> howto->rightshift)) & howto->dst_mask))

    if (diff != 0)
      {
	reloc_howto_type *howto = reloc_entry->howto;
	unsigned char *addr = (unsigned char *) data + reloc_entry->address;

	switch (howto->size)
	  {
	  case 0:
	    {
	      char x = bfd_get_8 (abfd, addr);
	      DOIT (x);
	      bfd_put_8 (abfd, x, addr);
	    }
	    break;

	  case 1:
	    {
	      short x = bfd_get_16 (abfd, addr);
	      DOIT (x);
	      bfd_put_16 (abfd, x, addr);
	    }
	    break;

	  case 2:
	    {
	      long x = bfd_get_32 (abfd, addr);
	      DOIT (x);
	      bfd_put_32 (abfd, x, addr);
	    }
	    break;

	  default:
	    abort ();
	  }
      }

  /* Now let bfd_perform_relocation finish everything up.  */
  return bfd_reloc_continue;
}

#ifdef COFF_WITH_PE
/* Return true if this relocation should
   appear in the output .reloc section.  */

static boolean
in_reloc_p (abfd, howto)
     bfd * abfd ATTRIBUTE_UNUSED;
     reloc_howto_type *howto;
{
  return ! howto->pc_relative && howto->type != MIPS_R_RVA;
}
#endif

#ifndef PCRELOFFSET
#define PCRELOFFSET false
#endif

static reloc_howto_type howto_table[] =
{
  /* Reloc type 0 is ignored.  The reloc reading code ensures that
     this is a reference to the .abs section, which will cause
     bfd_perform_relocation to do nothing.  */
  HOWTO (MIPS_R_ABSOLUTE,	/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 8,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 0,			/* special_function */
	 "IGNORE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 16 bit reference to a symbol, normally from a data section.  */
  HOWTO (MIPS_R_REFHALF,	/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_mips_reloc,	/* special_function */
	 "REFHALF",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 32 bit reference to a symbol, normally from a data section.  */
  HOWTO (MIPS_R_REFWORD,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_mips_reloc,	/* special_function */
	 "REFWORD",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 26 bit absolute jump address.  */
  HOWTO (MIPS_R_JMPADDR,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 			/* This needs complex overflow
				   detection, because the upper four
				   bits must match the PC.  */
	 coff_mips_reloc,	/* special_function */
	 "JMPADDR",		/* name */
	 true,			/* partial_inplace */
	 0x3ffffff,		/* src_mask */
	 0x3ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The high 16 bits of a symbol value.  Handled by the function
     mips_refhi_reloc.  */
  HOWTO (MIPS_R_REFHI,		/* type */
	 16,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_mips_reloc,	/* special_function */
	 "REFHI",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The low 16 bits of a symbol value.  */
  HOWTO (MIPS_R_REFLO,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 coff_mips_reloc,	/* special_function */
	 "REFLO",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A reference to an offset from the gp register.  Handled by the
     function mips_gprel_reloc.  */
  HOWTO (MIPS_R_GPREL,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_mips_reloc,	/* special_function */
	 "GPREL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A reference to a literal using an offset from the gp register.
     Handled by the function mips_gprel_reloc.  */
  HOWTO (MIPS_R_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 coff_mips_reloc,	/* special_function */
	 "LITERAL",		/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (8),
  EMPTY_HOWTO (9),
  EMPTY_HOWTO (10),
  EMPTY_HOWTO (11),
  EMPTY_HOWTO (12),
  EMPTY_HOWTO (13),
  EMPTY_HOWTO (14),
  EMPTY_HOWTO (15),
  EMPTY_HOWTO (16),
  EMPTY_HOWTO (17),
  EMPTY_HOWTO (18),
  EMPTY_HOWTO (19),
  EMPTY_HOWTO (20),
  EMPTY_HOWTO (21),
  EMPTY_HOWTO (22),
  EMPTY_HOWTO (23),
  EMPTY_HOWTO (24),
  EMPTY_HOWTO (25),
  EMPTY_HOWTO (26),
  EMPTY_HOWTO (27),
  EMPTY_HOWTO (28),
  EMPTY_HOWTO (29),
  EMPTY_HOWTO (30),
  EMPTY_HOWTO (31),
  EMPTY_HOWTO (32),
  EMPTY_HOWTO (33),
  HOWTO (MIPS_R_RVA,            /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_mips_reloc,       /* special_function */
	 "rva32",	        /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 false),                /* pcrel_offset */
  EMPTY_HOWTO (35),
  EMPTY_HOWTO (36),
  HOWTO (MIPS_R_PAIR,           /* type */
	 0,	                /* rightshift */
	 2,	                /* size (0 = byte, 1 = short, 2 = long) */
	 32,	                /* bitsize */
	 false,	                /* pc_relative */
	 0,	                /* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 coff_mips_reloc,       /* special_function */
	 "PAIR",	        /* name */
	 true,	                /* partial_inplace */
	 0xffffffff,            /* src_mask */
	 0xffffffff,            /* dst_mask */
	 false),                /* pcrel_offset */
};

/* Turn a howto into a reloc  nunmber */

#define SELECT_RELOC(x,howto) { x.r_type = howto->type; }
#define BADMAG(x) MIPSBADMAG(x)
#define MIPS 1			/* Customize coffcode.h */

#define RTYPE2HOWTO(cache_ptr, dst) \
	    (cache_ptr)->howto = howto_table + (dst)->r_type;

/* Compute the addend of a reloc.  If the reloc is to a common symbol,
   the object file contains the value of the common symbol.  By the
   time this is called, the linker may be using a different symbol
   from a different object file with a different value.  Therefore, we
   hack wildly to locate the original symbol from this file so that we
   can make the correct adjustment.  This macro sets coffsym to the
   symbol from the original file, and uses it to set the addend value
   correctly.  If this is not a common symbol, the usual addend
   calculation is done, except that an additional tweak is needed for
   PC relative relocs.
   FIXME: This macro refers to symbols and asect; these are from the
   calling function, not the macro arguments.  */

#define CALC_ADDEND(abfd, ptr, reloc, cache_ptr)		\
  {								\
    coff_symbol_type *coffsym = (coff_symbol_type *) NULL;	\
    if (ptr && bfd_asymbol_bfd (ptr) != abfd)			\
      coffsym = (obj_symbols (abfd)				\
	         + (cache_ptr->sym_ptr_ptr - symbols));		\
    else if (ptr)						\
      coffsym = coff_symbol_from (abfd, ptr);			\
    if (coffsym != (coff_symbol_type *) NULL			\
	&& coffsym->native->u.syment.n_scnum == 0)		\
      cache_ptr->addend = - coffsym->native->u.syment.n_value;	\
    else if (ptr && bfd_asymbol_bfd (ptr) == abfd		\
	     && ptr->section != (asection *) NULL)		\
      cache_ptr->addend = - (ptr->section->vma + ptr->value);	\
    else							\
      cache_ptr->addend = 0;					\
    if (ptr && howto_table[reloc.r_type].pc_relative)		\
      cache_ptr->addend += asect->vma;				\
  }

/* Convert an rtype to howto for the COFF backend linker.  */

static reloc_howto_type *
coff_mips_rtype_to_howto (abfd, sec, rel, h, sym, addendp)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     struct internal_reloc *rel;
     struct coff_link_hash_entry *h;
     struct internal_syment *sym;
     bfd_vma *addendp;
{

  reloc_howto_type *howto;

  howto = howto_table + rel->r_type;

#ifdef COFF_WITH_PE
  *addendp = 0;
#endif

  if (howto->pc_relative)
    *addendp += sec->vma;

  if (sym != NULL && sym->n_scnum == 0 && sym->n_value != 0)
    {
      /* This is a common symbol.  The section contents include the
	 size (sym->n_value) as an addend.  The relocate_section
	 function will be adding in the final value of the symbol.  We
	 need to subtract out the current size in order to get the
	 correct result.  */

      BFD_ASSERT (h != NULL);

#ifndef COFF_WITH_PE
      /* I think we *do* want to bypass this.  If we don't, I have
	 seen some data parameters get the wrong relocation address.
	 If I link two versions with and without this section bypassed
	 and then do a binary comparison, the addresses which are
	 different can be looked up in the map.  The case in which
	 this section has been bypassed has addresses which correspond
	 to values I can find in the map.  */
      *addendp -= sym->n_value;
#endif
    }

#ifndef COFF_WITH_PE
  /* If the output symbol is common (in which case this must be a
     relocateable link), we need to add in the final size of the
     common symbol.  */
  if (h != NULL && h->root.type == bfd_link_hash_common)
    *addendp += h->root.u.c.size;
#endif

#ifdef COFF_WITH_PE
  if (howto->pc_relative)
    {
      *addendp -= 4;

      /* If the symbol is defined, then the generic code is going to
         add back the symbol value in order to cancel out an
         adjustment it made to the addend.  However, we set the addend
         to 0 at the start of this function.  We need to adjust here,
         to avoid the adjustment the generic code will make.  FIXME:
         This is getting a bit hackish.  */
      if (sym != NULL && sym->n_scnum != 0)
	*addendp -= sym->n_value;
    }

  if (rel->r_type == MIPS_R_RVA)
    {
      *addendp -= pe_data(sec->output_section->owner)->pe_opthdr.ImageBase;
    }
#endif

  return howto;
}

#define coff_rtype_to_howto coff_mips_rtype_to_howto

#define coff_bfd_reloc_type_lookup coff_mips_reloc_type_lookup

/* Get the howto structure for a generic reloc type.  */

static reloc_howto_type *
coff_mips_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  int mips_type;

  switch (code)
    {
    case BFD_RELOC_16:
      mips_type = MIPS_R_REFHALF;
      break;
    case BFD_RELOC_32:
    case BFD_RELOC_CTOR:
      mips_type = MIPS_R_REFWORD;
      break;
    case BFD_RELOC_MIPS_JMP:
      mips_type = MIPS_R_JMPADDR;
      break;
    case BFD_RELOC_HI16_S:
      mips_type = MIPS_R_REFHI;
      break;
    case BFD_RELOC_LO16:
      mips_type = MIPS_R_REFLO;
      break;
    case BFD_RELOC_MIPS_GPREL:
      mips_type = MIPS_R_GPREL;
      break;
    case BFD_RELOC_MIPS_LITERAL:
      mips_type = MIPS_R_LITERAL;
      break;
/* FIXME?
    case BFD_RELOC_16_PCREL_S2:
      mips_type = MIPS_R_PCREL16;
      break;
    case BFD_RELOC_PCREL_HI16_S:
      mips_type = MIPS_R_RELHI;
      break;
    case BFD_RELOC_PCREL_LO16:
      mips_type = MIPS_R_RELLO;
      break;
    case BFD_RELOC_GPREL32:
      mips_type = MIPS_R_SWITCH;
      break;
*/
    case BFD_RELOC_RVA:
      mips_type = MIPS_R_RVA;
      break;
    default:
      return (reloc_howto_type *) NULL;
    }

  return &howto_table[mips_type];
}

static void
mips_swap_reloc_in (abfd, src, dst)
     bfd *abfd;
     PTR src;
     PTR dst;
{
  static struct internal_reloc pair_prev;
  RELOC *reloc_src = (RELOC *) src;
  struct internal_reloc *reloc_dst = (struct internal_reloc *) dst;

  reloc_dst->r_vaddr = bfd_h_get_32(abfd, (bfd_byte *)reloc_src->r_vaddr);
  reloc_dst->r_symndx =
    bfd_h_get_signed_32(abfd, (bfd_byte *) reloc_src->r_symndx);
  reloc_dst->r_type = bfd_h_get_16(abfd, (bfd_byte *) reloc_src->r_type);
  reloc_dst->r_size = 0;
  reloc_dst->r_extern = 0;
  reloc_dst->r_offset = 0;

  switch (reloc_dst->r_type)
  {
  case MIPS_R_REFHI:
    pair_prev = *reloc_dst;
    break;
  case MIPS_R_PAIR:
    reloc_dst->r_offset = reloc_dst->r_symndx;
    if (reloc_dst->r_offset & 0x8000)
      reloc_dst->r_offset -= 0x10000;
    /*printf ("dj: pair offset is %08x\n", reloc_dst->r_offset);*/
    reloc_dst->r_symndx = pair_prev.r_symndx;
    break;
  }
}

static unsigned int
mips_swap_reloc_out (abfd, src, dst)
     bfd       *abfd;
     PTR	src;
     PTR	dst;
{
  static int prev_offset = 1;
  static bfd_vma prev_addr = 0;
  struct internal_reloc *reloc_src = (struct internal_reloc *)src;
  struct external_reloc *reloc_dst = (struct external_reloc *)dst;

  switch (reloc_src->r_type)
    {
    case MIPS_R_REFHI:
      prev_addr = reloc_src->r_vaddr;
      prev_offset = reloc_src->r_offset;
      break;
    case MIPS_R_REFLO:
      if (reloc_src->r_vaddr == prev_addr)
	{
	  /* FIXME: only slightly hackish.  If we see a REFLO pointing to
	     the same address as a REFHI, we assume this is the matching
	     PAIR reloc and output it accordingly.  The symndx is really
	     the low 16 bits of the addend */
	  bfd_h_put_32 (abfd, reloc_src->r_vaddr,
			(bfd_byte *) reloc_dst->r_vaddr);
	  bfd_h_put_32 (abfd, reloc_src->r_symndx,
			(bfd_byte *) reloc_dst->r_symndx);

	  bfd_h_put_16(abfd, MIPS_R_PAIR, (bfd_byte *)
		       reloc_dst->r_type);
	  return RELSZ;
	}
      break;
    }

  bfd_h_put_32(abfd, reloc_src->r_vaddr, (bfd_byte *) reloc_dst->r_vaddr);
  bfd_h_put_32(abfd, reloc_src->r_symndx, (bfd_byte *) reloc_dst->r_symndx);

  bfd_h_put_16(abfd, reloc_src->r_type, (bfd_byte *)
	       reloc_dst->r_type);
  return RELSZ;
}

#define coff_swap_reloc_in mips_swap_reloc_in
#define coff_swap_reloc_out mips_swap_reloc_out
#define NO_COFF_RELOCS

static boolean
coff_pe_mips_relocate_section (output_bfd, info, input_bfd,
			       input_section, contents, relocs, syms,
			       sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     struct internal_reloc *relocs;
     struct internal_syment *syms;
     asection **sections;
{
  bfd_vma gp;
  boolean gp_undefined;
  size_t adjust;
  struct internal_reloc *rel;
  struct internal_reloc *rel_end;
  unsigned int i;
  boolean got_lo;

  if (info->relocateable)
  {
    (*_bfd_error_handler) (_("\
%s: `ld -r' not supported with PE MIPS objects\n"),
			  bfd_get_filename (input_bfd));
    bfd_set_error (bfd_error_bad_value);
    return false;
  }

  BFD_ASSERT (input_bfd->xvec->byteorder
	      == output_bfd->xvec->byteorder);

#if 0
  printf ("dj: relocate %s(%s) %08x\n",
	 input_bfd->filename, input_section->name,
	 input_section->output_section->vma + input_section->output_offset);
#endif

  gp = _bfd_get_gp_value (output_bfd);
  if (gp == 0)
    gp_undefined = true;
  else
    gp_undefined = false;

  got_lo = false;

  adjust = 0;

  rel = relocs;
  rel_end = rel + input_section->reloc_count;
  for (i = 0; rel < rel_end; rel++, i++)
    {
      long symndx;
      struct coff_link_hash_entry *h;
      struct internal_syment *sym;
      bfd_vma addend = 0;
      bfd_vma val, tmp, targ, src, low;
      reloc_howto_type *howto;
      unsigned char *mem = contents + rel->r_vaddr;

      symndx = rel->r_symndx;

      if (symndx == -1)
	{
	  h = NULL;
	  sym = NULL;
	}
      else
	{
	  h = obj_coff_sym_hashes (input_bfd)[symndx];
	  sym = syms + symndx;
	}

      /* COFF treats common symbols in one of two ways.  Either the
         size of the symbol is included in the section contents, or it
         is not.  We assume that the size is not included, and force
         the rtype_to_howto function to adjust the addend as needed.  */

      if (sym != NULL && sym->n_scnum != 0)
	addend = - sym->n_value;
      else
	addend = 0;

      howto = bfd_coff_rtype_to_howto (input_bfd, input_section, rel, h,
				       sym, &addend);
      if (howto == NULL)
	return false;

      /* If we are doing a relocateable link, then we can just ignore
         a PC relative reloc that is pcrel_offset.  It will already
         have the correct value.  If this is not a relocateable link,
         then we should ignore the symbol value.  */
      if (howto->pc_relative && howto->pcrel_offset)
	{
	  if (info->relocateable)
	    continue;
	  if (sym != NULL && sym->n_scnum != 0)
	    addend += sym->n_value;
	}

      val = 0;

      if (h == NULL)
	{
	  asection *sec;

	  if (symndx == -1)
	    {
	      sec = bfd_abs_section_ptr;
	      val = 0;
	    }
	  else
	    {
	      sec = sections[symndx];
              val = (sec->output_section->vma
		     + sec->output_offset
		     + sym->n_value);
	      if (! obj_pe (input_bfd))
		val -= sec->vma;
	    }
	}
      else
	{
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      asection *sec;

	      sec = h->root.u.def.section;
	      val = (h->root.u.def.value
		     + sec->output_section->vma
		     + sec->output_offset);
	      }

	  else if (! info->relocateable)
	    {
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, h->root.root.string, input_bfd, input_section,
		      rel->r_vaddr - input_section->vma, true)))
		return false;
	    }
	}

      src = rel->r_vaddr + input_section->output_section->vma
	+ input_section->output_offset;
#if 0
      printf ("dj: reloc %02x %-8s a=%08x/%08x(%08x) v=%08x+%08x %s\n",
	     rel->r_type, howto_table[rel->r_type].name,
	     src, rel->r_vaddr, *(unsigned long *)mem, val, rel->r_offset,
	     h?h->root.root.string:"(none)");
#endif

      /* OK, at this point the following variables are set up:
	   src = VMA of the memory we're fixing up
	   mem = pointer to memory we're fixing up
	   val = VMA of what we need to refer to
      */

#define UI(x) (*_bfd_error_handler) (_("%s: unimplemented %s\n"), \
				    bfd_get_filename (input_bfd), x); \
	      bfd_set_error (bfd_error_bad_value);

      switch (rel->r_type)
	{
	case MIPS_R_ABSOLUTE:
	  /* ignore these */
	  break;

	case MIPS_R_REFHALF:
	  UI("refhalf");
	  break;

	case MIPS_R_REFWORD:
	  tmp = bfd_get_32(input_bfd, mem);
	  /* printf ("refword: src=%08x targ=%08x+%08x\n", src, tmp, val); */
	  tmp += val;
	  bfd_put_32(input_bfd, tmp, mem);
	  break;

	case MIPS_R_JMPADDR:
	  tmp = bfd_get_32(input_bfd, mem);
	  targ = val + (tmp&0x03ffffff)*4;
	  if ((src & 0xf0000000) != (targ & 0xf0000000))
	    {
	      (*_bfd_error_handler) (_("%s: jump too far away\n"),
				    bfd_get_filename (input_bfd));
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	  tmp &= 0xfc000000;
	  tmp |= (targ/4) & 0x3ffffff;
	  bfd_put_32(input_bfd, tmp, mem);
	  break;

	case MIPS_R_REFHI:
	  tmp = bfd_get_32(input_bfd, mem);
	  switch (rel[1].r_type)
	    {
	    case MIPS_R_PAIR:
	      /* MS PE object */
	      targ = val + rel[1].r_offset + ((tmp & 0xffff) << 16);
	      break;
	    case MIPS_R_REFLO:
	      /* GNU COFF object */
	      low = bfd_get_32(input_bfd, contents + rel[1].r_vaddr);
	      low &= 0xffff;
	      if (low & 0x8000)
		low -= 0x10000;
	      targ = val + low + ((tmp & 0xffff) << 16);
	      break;
	    default:
	      (*_bfd_error_handler) (_("%s: bad pair/reflo after refhi\n"),
				    bfd_get_filename (input_bfd));
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	  tmp &= 0xffff0000;
	  tmp |= (targ >> 16) & 0xffff;
	  bfd_put_32(input_bfd, tmp, mem);
	  break;

	case MIPS_R_REFLO:
	  tmp = bfd_get_32(input_bfd, mem);
	  targ = val + (tmp & 0xffff);
	  /* printf ("refword: src=%08x targ=%08x\n", src, targ); */
	  tmp &= 0xffff0000;
	  tmp |= targ & 0xffff;
	  bfd_put_32(input_bfd, tmp, mem);
	  break;

	case MIPS_R_GPREL:
	case MIPS_R_LITERAL:
	  UI("gprel");
	  break;

	case MIPS_R_SECTION:
	  UI("section");
	  break;

	case MIPS_R_SECREL:
	  UI("secrel");
	  break;

	case MIPS_R_SECRELLO:
	  UI("secrello");
	  break;

	case MIPS_R_SECRELHI:
	  UI("secrelhi");
	  break;

	case MIPS_R_RVA:
	  tmp = bfd_get_32 (input_bfd, mem);
	  /* printf ("rva: src=%08x targ=%08x+%08x\n", src, tmp, val); */
	  tmp += val
	    - pe_data (input_section->output_section->owner)->pe_opthdr.ImageBase;
	  bfd_put_32 (input_bfd, tmp, mem);
	  break;

	case MIPS_R_PAIR:
	  /* ignore these */
	  break;
	}
    }

  return true;
}

#define coff_relocate_section coff_pe_mips_relocate_section

#ifdef TARGET_UNDERSCORE

/* If mips gcc uses underscores for symbol names, then it does not use
   a leading dot for local labels, so if TARGET_UNDERSCORE is defined
   we treat all symbols starting with L as local.  */

static boolean coff_mips_is_local_label_name PARAMS ((bfd *, const char *));

static boolean
coff_mips_is_local_label_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  if (name[0] == 'L')
    return true;

  return _bfd_coff_is_local_label_name (abfd, name);
}

#define coff_bfd_is_local_label_name coff_mips_is_local_label_name

#endif /* TARGET_UNDERSCORE */

#define COFF_NO_HACK_SCNHDR_SIZE

#include "coffcode.h"

const bfd_target
#ifdef TARGET_SYM
  TARGET_SYM =
#else
  mipslpe_vec =
#endif
{
#ifdef TARGET_NAME
  TARGET_NAME,
#else
  "pe-mips",			/* name */
#endif
  bfd_target_coff_flavour,
  BFD_ENDIAN_LITTLE,		/* data byte order is little */
  BFD_ENDIAN_LITTLE,		/* header byte order is little */

  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),

#ifndef COFF_WITH_PE
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC /* section flags */
   | SEC_CODE | SEC_DATA),
#else
  (SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD | SEC_RELOC /* section flags */
   | SEC_CODE | SEC_DATA
   | SEC_LINK_ONCE | SEC_LINK_DUPLICATES),
#endif

#ifdef TARGET_UNDERSCORE
  TARGET_UNDERSCORE,		/* leading underscore */
#else
  0,				/* leading underscore */
#endif
  '/',				/* ar_pad_char */
  15,				/* ar_max_namelen */

  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* data */
  bfd_getl64, bfd_getl_signed_64, bfd_putl64,
     bfd_getl32, bfd_getl_signed_32, bfd_putl32,
     bfd_getl16, bfd_getl_signed_16, bfd_putl16, /* hdrs */

/* Note that we allow an object file to be treated as a core file as well.  */
    {_bfd_dummy_target, coff_object_p, /* bfd_check_format */
       bfd_generic_archive_p, coff_object_p},
    {bfd_false, coff_mkobject, _bfd_generic_mkarchive, /* bfd_set_format */
       bfd_false},
    {bfd_false, coff_write_object_contents, /* bfd_write_contents */
       _bfd_write_archive_contents, bfd_false},

     BFD_JUMP_TABLE_GENERIC (coff),
     BFD_JUMP_TABLE_COPY (coff),
     BFD_JUMP_TABLE_CORE (_bfd_nocore),
     BFD_JUMP_TABLE_ARCHIVE (_bfd_archive_coff),
     BFD_JUMP_TABLE_SYMBOLS (coff),
     BFD_JUMP_TABLE_RELOCS (coff),
     BFD_JUMP_TABLE_WRITE (coff),
     BFD_JUMP_TABLE_LINK (coff),
     BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  COFF_SWAP_TABLE
};
