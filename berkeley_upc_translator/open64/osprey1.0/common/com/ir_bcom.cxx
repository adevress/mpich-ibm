/*

  Copyright (C) 2000, 2001 Silicon Graphics, Inc.  All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if 
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU General Public License along
  with this program; if not, write the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston MA 02111-1307, USA.

  Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pky,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/


#ifdef USE_PCH
#include "common_com_pch.h"
#endif /* USE_PCH */
#pragma hdrstop
#include <unistd.h>		    /* for unlink() */
#include <sys/mman.h>		    /* for mmap() */
#include <string.h>                 /* for strerror() */
#include <errno.h>		    /* for errno */
#include <elf.h>		    /* for all Elf stuff */
#include <sys/elf_whirl.h>	    /* for WHIRL sections' sh_info */

// Solaric CC workaround
// no big deal, just to fix a compiler warning
#ifndef USE_STANDARD_TYPES
#define USE_STANDARD_TYPES	    /* override unwanted defines in "defs.h" */
#endif

#include "defs.h"
#include "alignof.h"                /* for ALIGNOF() */
#include "erglob.h"
#include "errors.h"		    /* for ErrMsg() */
#include "opcode.h"
#include "mempool.h"
#include "wn.h"
#include "wn_map.h"
#include "strtab.h"		    /* for strtab */
#include "symtab.h"		    /* for symtab */
#include "irbdata.h"		    /* for inito */
#define USE_DST_INTERNALS
#include "dwarf_DST_mem.h"	    /* for dst */
#include "pu_info.h"
#include "ir_bwrite.h"
#include "ir_bcom.h"

/* For 4K page, each kernel page maps to 4Mbytes user address space */
#define MAPPED_SIZE		0x400000
#define INIT_TMP_MAPPED_SIZE    MAPPED_SIZE

#ifdef BACK_END
#define DEFAULT_NUM_OF_PREFETCHES 64
WN **prefetch_ldsts;
INT num_prefetch_ldsts;
INT max_num_prefetch_ldsts;

#define DEFAULT_NUM_ALIAS_CLASSES 128
WN **alias_classes;
INT num_alias_class_nodes;
INT max_alias_class_nodes;

#define DEFAULT_NUM_AC_INTERNALS 128
WN **ac_internals;
INT num_ac_internal_nodes;
INT max_ac_internal_nodes;
#endif


char *Whirl_Revision = WHIRL_REVISION;

/* This variable is used by IPAA to pass its local map information
 * to ir_bwrite.c, and by ir_bread.c to pass it to WOPT:
 */
void *IPAA_Local_Map = NULL;

BOOL Doing_mmapped_io = FALSE;

/* copy a block of memory into the temporary file.
 * "align" is the alignment requirement, which must be a power of 2.
 * "padding" is the byte offset from the beginning of the buffer that the
 * alignment requirement applies.
 *
 * It returns the file offset corresponding to buf + padding.
 */
extern Elf64_Word
ir_b_save_buf (const void *buf, Elf64_Word size, unsigned int align,
	       unsigned int padding, Output_File *fl)
{
    Current_Output = fl;

    off_t file_size = ir_b_align (fl->file_size, align, padding);

    if (file_size + size >= fl->mapped_size)
	ir_b_grow_map (file_size + size - fl->file_size, fl);

    Doing_mmapped_io = TRUE;
    (void *)memcpy(fl->map_addr + file_size, buf, size);
    Doing_mmapped_io = FALSE;

    fl->file_size = file_size + size;
    return file_size + padding;
} /* ir_b_save_buf */

/* copy a file into the temporary file.
 */
extern Elf64_Word
ir_b_copy_file (const void *buf, Elf64_Word size, void *tmpfl)
{
    Output_File* fl = (Output_File*)tmpfl;

    Current_Output = fl;

    if (size >= fl->mapped_size)
	ir_b_grow_map (size, fl);
    
    Doing_mmapped_io = TRUE;
    memcpy (fl->map_addr, buf, size);
    Doing_mmapped_io = FALSE;
    fl->file_size = size;
    return size;
} // ir_b_copy_file 


static void
save_buf_at_offset (const void *buf, Elf64_Word size, off_t offset,
		    Output_File *fl)
{
    Is_True (offset + size <= fl->mapped_size, ("Invalid buffer size"));

    Doing_mmapped_io = TRUE;
    memcpy (fl->map_addr + offset, buf, size);
    Doing_mmapped_io = FALSE;
} // save_buf_at_offset

// similar to ir_b_save_buf, except that no actual copying is done, but
// file space is reserved.
static Elf64_Word
ir_b_reserve_space (Elf64_Word size, unsigned int align, Output_File *fl)
{
    off_t file_size = ir_b_align (fl->file_size, align, 0);

    if (file_size + size >= fl->mapped_size)
	ir_b_grow_map (file_size + size - fl->file_size, fl);

    fl->file_size = file_size + size;
    return file_size;
} // ir_b_reserve_space


/* increase the mmap size.  It is faster if we first unmap the region and
 * then map it again with a larger size.  The overhead for maintaining
 * multiple regions (by the kernel) outweighs that of extra system calls.
 * Also, we get a contiguous address space for the file this way.
 */
char *
ir_b_grow_map (Elf64_Word min_size, Output_File *fl)
{
    Is_True (fl->map_addr != 0, ("output file not yet mapped"));

    if (munmap (fl->map_addr, fl->mapped_size) == -1)
	ErrMsg (EC_IR_Write, fl->file_name, errno);
    min_size += fl->file_size;
    while (fl->mapped_size < min_size) {
	if (fl->mapped_size < MAPPED_SIZE)
	    fl->mapped_size = MAPPED_SIZE;
	else
	    fl->mapped_size += MAPPED_SIZE;
    }

#if defined(__sgi)
    // Only SGI supports 'MAP_AUTOGROW' for mmap()
    fl->map_addr = (char *) mmap (0, fl->mapped_size, PROT_READ|PROT_WRITE,
				  MAP_SHARED|MAP_AUTOGROW, fl->output_fd, 0); 
#else
    // mmap() normally cannot automatically increase file size, so we
    // allocate some space using ftruncate().  SGI's mmap() can avoid
    // this. cf. use of ftruncate() in ir_bwrite.cxx.
    if (ftruncate(fl->output_fd, fl->mapped_size)) {
	ErrMsg (EC_IR_Write, fl->file_name, strerror(errno));
    }
    fl->map_addr = (char *) mmap (0, fl->mapped_size, PROT_READ|PROT_WRITE,
				  MAP_SHARED, fl->output_fd, 0); 
#endif    
    
    if (fl->map_addr == (char *) (-1))
	ErrMsg (EC_IR_Write, fl->file_name, strerror(errno));

    return fl->map_addr;
} /* ir_b_grow_map */

extern char *
ir_b_create_map (Output_File *fl)
{
    int fd = fl->output_fd;
    fl->mapped_size = INIT_TMP_MAPPED_SIZE;

#if defined(__sgi)
    // Only SGI supports 'MAP_AUTOGROW' for mmap()
    fl->map_addr = (char *) mmap (0, fl->mapped_size, PROT_READ|PROT_WRITE,
                                  MAP_SHARED|MAP_AUTOGROW, fl->output_fd, 0);
#elif defined(__alpha)
    //wei: mmap requires different flags on Tru64, sigh...
    fl->map_addr = (char *) mmap (0, fl->mapped_size, PROT_READ|PROT_WRITE,
                                  MAP_SHARED|MAP_FILE|MAP_VARIABLE, fl->output_fd, 0);
#else
    fl->map_addr = (char *) mmap (0, fl->mapped_size, PROT_READ|PROT_WRITE,
                                  MAP_SHARED, fl->output_fd, 0);
#endif
    
    return fl->map_addr;
} /* ir_b_create_map */


WN *staticNode;

/* Walk the tree and copy it to contiguous memory block in the temp. file */
extern Elf64_Word
ir_b_write_tree (WN *node, off_t base_offset, Output_File *fl, WN_MAP off_map)
{
    register OPCODE opcode;
    Elf64_Word node_offset;
    char *real_addr;

    if (node == staticNode) abort();

    INT32 size = WN_Size_and_StartAddress (node, (void **) &real_addr);

#define WN_ADDR(offset) ((WN *)(fl->map_addr + offset))

    node_offset = ir_b_save_buf (real_addr, size, ALIGNOF(WN),
				 (char *)(node) - real_addr, fl); 

    opcode = (OPCODE) WN_opcode (node);

#ifdef BACK_END
    if (off_map != WN_MAP_UNDEFINED &&
	(Write_BE_Maps ||
	 Write_ALIAS_CLASS_Map ||
	 Write_AC_INTERNAL_Map)) {
	/* save node_offset for use when writing maps */
	BOOL set_offset = FALSE;
	OPERATOR opr = OPCODE_operator(opcode);

	if (Write_BE_Maps) {
	    if (opr == OPR_PREFETCH || opr == OPR_PREFETCHX ||
		OPCODE_is_load (opcode) || OPCODE_is_store (opcode))
		set_offset = TRUE;

	    /* check if the WN has a prefetch pointer */
	    if (WN_MAP_Get(WN_MAP_PREFETCH, node)) {
		/* make sure the prefetch_ldsts array is big enough to hold the
		   lds and sts plus one extra slot to mark the end */
		if (num_prefetch_ldsts == 0) {
		    max_num_prefetch_ldsts = DEFAULT_NUM_OF_PREFETCHES;
		    prefetch_ldsts =
			(WN **)malloc(max_num_prefetch_ldsts * sizeof(WN*));
		    FmtAssert (prefetch_ldsts,
			       ("No more memory for allocation."));
		} else if (max_num_prefetch_ldsts == num_prefetch_ldsts + 1) {
		    max_num_prefetch_ldsts *= 2;
		    prefetch_ldsts =
			(WN **)realloc(prefetch_ldsts,
				       max_num_prefetch_ldsts * sizeof(WN*));
		    FmtAssert (prefetch_ldsts,
			       ("No more memory for allocation."));
		}
		prefetch_ldsts[num_prefetch_ldsts] = node;
		num_prefetch_ldsts += 1;
	    }
	}
	    
	if (Write_ALIAS_CLASS_Map) {
	    if (OPCODE_is_store (opcode) ||
		OPCODE_is_load (opcode) ||
		(opr == OPR_LDA /* && LDA_has_ac_map_set(node) */) ||
		opr == OPR_PARM) {
	      set_offset = TRUE;
	    }

	    if (WN_MAP32_Get (WN_MAP_ALIAS_CLASS, node) != 0) {
		if (alias_classes == NULL) {
		    max_alias_class_nodes = DEFAULT_NUM_ALIAS_CLASSES;
		    alias_classes = (WN **) malloc (max_alias_class_nodes *
						    sizeof(WN *));
		    FmtAssert (alias_classes != NULL, ("No more memory."));
		} else if (max_alias_class_nodes == num_alias_class_nodes + 1) {
		    max_alias_class_nodes *= 2;
		    alias_classes = (WN **) realloc(alias_classes,
						    max_alias_class_nodes *
						    sizeof(WN **));
		    FmtAssert(alias_classes != NULL, ("No more memory."));
		}
		alias_classes[num_alias_class_nodes++] = node;
	    }
	}
	
	if (Write_AC_INTERNAL_Map) {
	    if (opr == OPR_ILOAD  ||
		opr == OPR_MLOAD  ||
		opr == OPR_PARM   ||
		opr == OPR_ISTORE ||
		opr == OPR_MSTORE) {
	      set_offset = TRUE;
	    }

	    if (WN_MAP_Get (WN_MAP_AC_INTERNAL, node) != NULL) {
		if (ac_internals == NULL) {
		    max_ac_internal_nodes = DEFAULT_NUM_AC_INTERNALS;
		    ac_internals = (WN **) malloc (max_ac_internal_nodes *
						   sizeof(WN *));
		    FmtAssert (ac_internals != NULL, ("No more memory."));
		} else if (max_ac_internal_nodes == num_ac_internal_nodes + 1) {
		    max_ac_internal_nodes *= 2;
		    ac_internals = (WN **) realloc(ac_internals,
						   max_ac_internal_nodes *
						   sizeof(WN **));
		    FmtAssert(ac_internals != NULL, ("No more memory."));
		}
		ac_internals[num_ac_internal_nodes++] = node;
	    }
	}

	if (set_offset)
	    WN_MAP32_Set(off_map, node, node_offset - base_offset);

    }
#endif /* BACK_END */


    if (opcode == OPC_BLOCK) {
	register Elf64_Word prev, this_node;

	if (WN_first(node) == 0) {
	    WN_first(WN_ADDR(node_offset)) = (WN *) -1;
	    WN_last(WN_ADDR(node_offset)) = (WN *) -1;
	} else {
	    register WN *wn = WN_first (node);
	    prev = ir_b_write_tree(wn, base_offset, fl, off_map);
	    WN_first(WN_ADDR(node_offset)) = (WN *) prev;

	    while (wn = WN_next(wn)) {
		this_node = ir_b_write_tree(wn, base_offset, fl, off_map);
		/* fill in the correct next/prev offsets (in place of -1) */
		WN_next(WN_ADDR(prev + base_offset)) = (WN *) this_node;
		WN_prev(WN_ADDR(this_node + base_offset)) = (WN *) prev;
		prev = this_node;
	    }

	    WN_last(WN_ADDR(node_offset)) = (WN *) prev;
	}
    } else if (!OPCODE_is_leaf(opcode)) {
	register int i;

	for (i = 0; i < WN_kid_count(node); i++) {
	    register Elf64_Word kid;

	    if (WN_kid(node, i) == 0) {
		WN_kid(WN_ADDR(node_offset), i) = (WN *) -1;
	    } else {
		kid = ir_b_write_tree (WN_kid(node, i), base_offset,
				       fl, off_map);
		WN_kid(WN_ADDR(node_offset), i) = (WN *) kid;
	    }
	}
    }

    if (OPCODE_has_next_prev(opcode)) {
	/* just set the default values for now */
	WN_prev(WN_ADDR(node_offset)) = (WN *) -1;
	WN_next(WN_ADDR(node_offset)) = (WN *) -1;
    }

    return node_offset - base_offset;
} /* ir_b_write_tree */


/*------------ symtab routines ---------------*/

// function object for writing out various symbol tables
template <class T>
struct WRITE_TABLE_OP
{
    Output_File *fl;

    void operator () (UINT, T *t, UINT size) const {
	(void) ir_b_save_buf (t, size * sizeof(T), ALIGNOF(T), 0, fl); 
    }

    WRITE_TABLE_OP (Output_File *_fl) : fl (_fl) {}
}; // WRITE_TABLE_OP


template <class TABLE>
Elf64_Word
write_table (TABLE& fld, off_t base_offset,
	     Output_File *fl)
{

    off_t cur_offset = ir_b_align (fl->file_size, ALIGNOF(typename TABLE::base_type),
				   0);
    fl->file_size = ir_b_align (fl->file_size, ALIGNOF(typename TABLE::base_type), 0);

#ifndef __GNUC__
    const WRITE_TABLE_OP<TABLE::base_type> write_table_op(fl);
#else
    const WRITE_TABLE_OP<typename TABLE::base_type> write_table_op(fl);
#endif

    For_all_blocks (fld, write_table_op);
    return cur_offset - base_offset;
} // write_table


static Elf64_Word
write_file_info (off_t base_offset, Output_File *fl)
{
    off_t cur_offset = ir_b_align (fl->file_size, ALIGNOF(FILE_INFO), 0);

    ir_b_save_buf (&File_info, sizeof(File_info), ALIGNOF(FILE_INFO), 0, fl);

    return cur_offset - base_offset;
} // write_file_info


// write a global symtab:  
Elf64_Word
ir_b_write_global_symtab (off_t base_offset, Output_File *fl)
{
    GLOBAL_SYMTAB_HEADER_TABLE gsymtab;

    // should use __builtin_alignof(gsymtab) instead of sizeof(mUINT64),
    // but our frontend has a bug and fails to compile it. 
    const Elf64_Word symtab_offset =
	ir_b_reserve_space (sizeof(gsymtab), sizeof(mUINT64), fl);

    Elf64_Word cur_offset;
    UINT i = 0;
    const UINT idx = GLOBAL_SYMTAB;

    cur_offset = write_file_info (symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset, sizeof(FILE_INFO),
			      sizeof(FILE_INFO), ALIGNOF(FILE_INFO),
			      SHDR_FILE);

    cur_offset = write_table (*(Scope_tab[idx].st_tab), symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset,
			      Scope_tab[idx].st_tab->Size () * sizeof(ST),
			      sizeof(ST), ALIGNOF(ST), SHDR_ST);

    // call fix_array_ty?

    cur_offset = write_table (Ty_tab, symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset, Ty_tab.Size () * sizeof(TY),
			      sizeof(TY), ALIGNOF(TY), SHDR_TY);

    cur_offset = write_table (Pu_Table, symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset, Pu_Table.Size () * sizeof(PU),
			      sizeof(PU), ALIGNOF(PU), SHDR_PU);

    cur_offset = write_table (Fld_Table, symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset, Fld_Table.Size () * sizeof(FLD),
			      sizeof(FLD), ALIGNOF(FLD), SHDR_FLD);

    cur_offset = write_table (Arb_Table, symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset, Arb_Table.Size () * sizeof(ARB),
			      sizeof(ARB), ALIGNOF(ARB), SHDR_ARB);

    cur_offset = write_table (Tylist_Table, symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset,
			      Tylist_Table.Size () * sizeof(TYLIST),
			      sizeof(TYLIST), ALIGNOF(TYLIST), SHDR_TYLIST); 

    cur_offset = write_table (Tcon_Table, symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset, Tcon_Table.Size () * sizeof(TCON),
			      sizeof(TCON), ALIGNOF(TCON), SHDR_TCON); 

    cur_offset = ir_b_save_buf (TCON_strtab_buffer (), TCON_strtab_size (),
				1, 0, fl) - symtab_offset;
    gsymtab.header[i++].Init (cur_offset, TCON_strtab_size (), 1, 1, SHDR_STR);

    cur_offset = write_table (*(Scope_tab[idx].inito_tab), symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset,
			      Scope_tab[idx].inito_tab->Size () * sizeof(INITO),
			      sizeof(INITO), ALIGNOF(INITO), SHDR_INITO);

    cur_offset = write_table (Initv_Table, symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset, Initv_Table.Size () * sizeof(INITV),
			      sizeof(INITV), ALIGNOF(INITV), SHDR_INITV); 

    cur_offset = write_table (Blk_Table, symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset, Blk_Table.Size () * sizeof(BLK),
			      sizeof(BLK), ALIGNOF(BLK), SHDR_BLK); 

    cur_offset = write_table (*(Scope_tab[idx].st_attr_tab), symtab_offset, fl);
    gsymtab.header[i++].Init (cur_offset,
			      Scope_tab[idx].st_attr_tab->Size () * sizeof(ST_ATTR),
			      sizeof(ST_ATTR), ALIGNOF(ST_ATTR), SHDR_ST_ATTR);

    save_buf_at_offset (&gsymtab, sizeof(gsymtab), symtab_offset, fl);

    return symtab_offset - base_offset;

} // ir_b_write_global_symtab

Elf64_Word
ir_b_write_local_symtab (const SCOPE& pu, off_t base_offset, Output_File *fl)
{

    LOCAL_SYMTAB_HEADER_TABLE symtab;

    const Elf64_Word symtab_offset =
	ir_b_reserve_space (sizeof(symtab), sizeof(mUINT64), fl);

    UINT i = 0;
    Elf64_Word cur_offset;
    cur_offset = write_table (*pu.st_tab, symtab_offset, fl);
    symtab.header[i++].Init (cur_offset, pu.st_tab->Size () * sizeof(ST),
			     sizeof(ST), ALIGNOF(ST), SHDR_ST);

    cur_offset = write_table (*pu.label_tab, symtab_offset, fl);
    symtab.header[i++].Init (cur_offset, pu.label_tab->Size () * sizeof(LABEL),
			     sizeof(LABEL), ALIGNOF(LABEL), SHDR_LABEL); 

    cur_offset = write_table (*pu.preg_tab, symtab_offset, fl);
    symtab.header[i++].Init (cur_offset, pu.preg_tab->Size () * sizeof(PREG),
			     sizeof(PREG), ALIGNOF(PREG), SHDR_PREG);

    cur_offset = write_table (*pu.inito_tab, symtab_offset, fl);
    symtab.header[i++].Init (cur_offset, pu.inito_tab->Size () * sizeof(INITO),
			     sizeof(INITO), ALIGNOF(INITO), SHDR_INITO);
    
    cur_offset = write_table (*pu.st_attr_tab, symtab_offset, fl);
    symtab.header[i++].Init (cur_offset, pu.st_attr_tab->Size () * sizeof(ST_ATTR),  
				sizeof(ST_ATTR), ALIGNOF(ST_ATTR), SHDR_ST_ATTR);

    save_buf_at_offset (&symtab, sizeof(symtab), symtab_offset, fl);

    return symtab_offset - base_offset;

} // ir_b_write_local_symtab


/* write blocks of data, then block headers, then # blocks */
extern Elf64_Word
ir_b_write_dst (DST_TYPE dst, off_t base_offset, Output_File *fl)
{
    Elf64_Word cur_offset;
    DST_BLOCK_IDX i;
    block_header *dst_blocks;
    Current_DST = dst;

    dst_blocks = ((DST_Type *)dst)->dst_blocks;
    FOREACH_DST_BLOCK(i) {
	/* may have 64-bit data fields, so align at 8 bytes */
	cur_offset = ir_b_save_buf (dst_blocks[i].offset, 
		dst_blocks[i].size, ALIGNOF(INT64), 0, fl);
	dst_blocks[i].offset = (char*)(cur_offset - base_offset);
    } 
    FOREACH_DST_BLOCK(i) {
	cur_offset = ir_b_save_buf
	    ((char*)&dst_blocks[i], sizeof(block_header),
	     ALIGNOF(block_header), 0, fl);
    } 
    cur_offset = ir_b_save_buf
	((char*)&((DST_Type *)dst)->last_block_header, sizeof(mINT32), 
	 ALIGNOF(INT32), 0, fl);
    return cur_offset - base_offset;
}



