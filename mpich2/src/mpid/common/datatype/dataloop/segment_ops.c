/* -*- Mode: C; c-basic-offset:4 ; -*- */

/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "dataloop.h"
#include "veccpy.h"

int PREPEND_PREFIX(Segment_contig_m2m)(DLOOP_Offset *blocks_p,
				       DLOOP_Type el_type,
				       DLOOP_Offset rel_off,
				       void *bufp ATTRIBUTE((unused)),
				       void *v_paramp)
{
    DLOOP_Offset el_size; /* DLOOP_Count? */
    DLOOP_Offset size;
    struct PREPEND_PREFIX(m2m_params) *paramp = v_paramp;

    DLOOP_Handle_get_size_macro(el_type, el_size);
    size = *blocks_p * el_size;

#ifdef MPID_SU_VERBOSE
    dbg_printf("\t[contig unpack: do=" MPI_AINT_FMT_DEC_SPEC ", dp=%x, bp=%x, sz=" MPI_AINT_FMT_DEC_SPEC ", blksz=" MPI_AINT_FMT_DEC_SPEC "]\n",
	       rel_off,
	       (unsigned) bufp,
	       (unsigned) paramp->u.unpack.unpack_buffer,
	       el_size,
	       *blocks_p);
#endif

    if (paramp->direction == DLOOP_M2M_TO_USERBUF) {
	memcpy((char *) MPI_AINT_CAST_TO_VOID_PTR ((MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->userbuf)) + rel_off),
	       paramp->streambuf,
	       size);
    }
    else {
	memcpy(paramp->streambuf,
	       (char *) MPI_AINT_CAST_TO_VOID_PTR ((MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->userbuf)) + rel_off),
	       size);
    }
    paramp->streambuf = (char*) MPI_AINT_CAST_TO_VOID_PTR
	                        ((MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->streambuf)) + size);
    return 0;
}

/* Segment_vector_m2m
 *
 * Note: this combines both packing and unpacking functionality.
 *
 * Note: this is only called when the starting position is at the beginning
 * of a whole block in a vector type.
 */
int PREPEND_PREFIX(Segment_vector_m2m)(DLOOP_Offset *blocks_p,
				       DLOOP_Count count ATTRIBUTE((unused)),
				       DLOOP_Count blksz,
				       DLOOP_Offset stride,
				       DLOOP_Type el_type,
				       DLOOP_Offset rel_off, /* offset into buffer */
				       void *bufp ATTRIBUTE((unused)),
				       void *v_paramp)
{
    DLOOP_Count i, blocks_left, whole_count;
    DLOOP_Offset el_size;
    struct PREPEND_PREFIX(m2m_params) *paramp = v_paramp;
    char *cbufp;

    cbufp = (char*) MPI_AINT_CAST_TO_VOID_PTR
	            ( (MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->userbuf)) + rel_off );
    DLOOP_Handle_get_size_macro(el_type, el_size);

    whole_count = (blksz > 0) ? (*blocks_p / (DLOOP_Offset) blksz) : 0;
    blocks_left = (blksz > 0) ? (*blocks_p % (DLOOP_Offset) blksz) : 0;

    if (paramp->direction == DLOOP_M2M_TO_USERBUF) {
	if (el_size == 8
	    MPIR_ALIGN8_TEST(paramp->streambuf,cbufp))
	{
	    MPIDI_COPY_TO_VEC(paramp->streambuf, cbufp, stride,
			      int64_t, blksz, whole_count);
	    MPIDI_COPY_TO_VEC(paramp->streambuf, cbufp, 0,
			      int64_t, blocks_left, 1);
	}
	else if (el_size == 4
		 MPIR_ALIGN4_TEST(paramp->streambuf,cbufp))
	{
	    MPIDI_COPY_TO_VEC((paramp->streambuf), cbufp, stride,
			      int32_t, blksz, whole_count);
	    MPIDI_COPY_TO_VEC(paramp->streambuf, cbufp, 0,
			      int32_t, blocks_left, 1);
	}
	else if (el_size == 2) {
	    MPIDI_COPY_TO_VEC(paramp->streambuf, cbufp, stride,
			      int16_t, blksz, whole_count);
	    MPIDI_COPY_TO_VEC(paramp->streambuf, cbufp, 0,
			      int16_t, blocks_left, 1);
	}
	else {
	    for (i=0; i < whole_count; i++) {
		memcpy(cbufp, paramp->streambuf, ((DLOOP_Offset) blksz) * el_size);

		paramp->streambuf = (char*) MPI_AINT_CAST_TO_VOID_PTR
		                            ( (MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->streambuf)) +
					      (((DLOOP_Offset)blksz) * el_size) );

		cbufp = (char*) MPI_AINT_CAST_TO_VOID_PTR
		                ( (MPI_PTR_DISP_CAST_TO_MPI_AINT (cbufp)) + stride);
	    }
	    if (blocks_left) {
		memcpy(cbufp, paramp->streambuf, ((DLOOP_Offset) blocks_left) * el_size);

		paramp->streambuf = (char*) MPI_AINT_CAST_TO_VOID_PTR
		                            ( (MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->streambuf)) +
					      ((DLOOP_Offset)blocks_left * el_size));
	    }
	}
    }
    else /* M2M_FROM_USERBUF */ {
	if (el_size == 8
	    MPIR_ALIGN8_TEST(cbufp,paramp->streambuf))
	{
	    MPIDI_COPY_FROM_VEC(cbufp, paramp->streambuf, stride,
				int64_t, blksz, whole_count);
	    MPIDI_COPY_FROM_VEC(cbufp, paramp->streambuf, 0,
				int64_t, blocks_left, 1);
	}
	else if (el_size == 4
		 MPIR_ALIGN4_TEST(cbufp,paramp->streambuf))
	{
	    MPIDI_COPY_FROM_VEC(cbufp, paramp->streambuf, stride,
				int32_t, blksz, whole_count);
	    MPIDI_COPY_FROM_VEC(cbufp, paramp->streambuf, 0,
				int32_t, blocks_left, 1);
	}
	else if (el_size == 2) {
	    MPIDI_COPY_FROM_VEC(cbufp, paramp->streambuf, stride,
				int16_t, blksz, whole_count);
	    MPIDI_COPY_FROM_VEC(cbufp, paramp->streambuf, 0,
				int16_t, blocks_left, 1);
	}
	else {
	    for (i=0; i < whole_count; i++) {
		memcpy(paramp->streambuf, cbufp, (DLOOP_Offset) blksz * el_size);

		paramp->streambuf = (char*) MPI_AINT_CAST_TO_VOID_PTR
		                            ( (MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->streambuf)) +
					      (((DLOOP_Offset)blksz) * el_size) );

		cbufp = (char*) MPI_AINT_CAST_TO_VOID_PTR
		                ( (MPI_PTR_DISP_CAST_TO_MPI_AINT (cbufp)) + stride);
	    }
	    if (blocks_left) {
		memcpy(paramp->streambuf, cbufp, (DLOOP_Offset) blocks_left * el_size);

		paramp->streambuf = (char*) MPI_AINT_CAST_TO_VOID_PTR
		                            ( (MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->streambuf)) +
					      ((DLOOP_Offset)blocks_left * el_size));
	    }
	}
    }

    return 0;
}

/* MPID_Segment_blkidx_m2m
 */
int PREPEND_PREFIX(Segment_blkidx_m2m)(DLOOP_Offset *blocks_p,
				       DLOOP_Count count,
				       DLOOP_Count blocklen,
				       DLOOP_Offset *offsetarray,
				       DLOOP_Type el_type,
				       DLOOP_Offset rel_off,
				       void *bufp ATTRIBUTE((unused)),
				       void *v_paramp)
{
    DLOOP_Count curblock = 0;
    DLOOP_Offset el_size;
    DLOOP_Offset blocks_left = *blocks_p;
    char *cbufp;
    struct PREPEND_PREFIX(m2m_params) *paramp = v_paramp;

    DLOOP_Handle_get_size_macro(el_type, el_size);

    while (blocks_left) {
	char *src, *dest;

	DLOOP_Assert(curblock < count);

	cbufp = (char*) MPI_AINT_CAST_TO_VOID_PTR
	                ((MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->userbuf)) +
					 rel_off + offsetarray[curblock]);

	if (blocklen > blocks_left) blocklen = blocks_left;

	if (paramp->direction == DLOOP_M2M_TO_USERBUF) {
	    src  = paramp->streambuf;
	    dest = cbufp;
	}
	else {
	    src  = cbufp;
	    dest = paramp->streambuf;
	}

	/* note: macro modifies dest buffer ptr, so we must reset */
	if (el_size == 8
	    MPIR_ALIGN8_TEST(src, dest))
	{
	    MPIDI_COPY_FROM_VEC(src, dest, 0, int64_t, blocklen, 1);
	}
	else if (el_size == 4
		 MPIR_ALIGN4_TEST(src,dest))
	{
	    MPIDI_COPY_FROM_VEC(src, dest, 0, int32_t, blocklen, 1);
	}
	else if (el_size == 2) {
	    MPIDI_COPY_FROM_VEC(src, dest, 0, int16_t, blocklen, 1);
	}
	else {
	    memcpy(dest, src, (DLOOP_Offset) blocklen * el_size);
	}

	paramp->streambuf = (char*) MPI_AINT_CAST_TO_VOID_PTR
	                            ((MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->streambuf)) +
				     ((DLOOP_Offset)blocklen * el_size));
	blocks_left -= blocklen;
	curblock++;
    }

    return 0;
}

/* MPID_Segment_index_m2m
 */
int PREPEND_PREFIX(Segment_index_m2m)(DLOOP_Offset *blocks_p,
				      DLOOP_Count count,
				      DLOOP_Count *blockarray,
				      DLOOP_Offset *offsetarray,
				      DLOOP_Type el_type,
				      DLOOP_Offset rel_off,
				      void *bufp ATTRIBUTE((unused)),
				      void *v_paramp)
{
    int curblock = 0;
    DLOOP_Offset el_size;
    DLOOP_Offset cur_block_sz, blocks_left = *blocks_p;
    char *cbufp;
    struct PREPEND_PREFIX(m2m_params) *paramp = v_paramp;

    DLOOP_Handle_get_size_macro(el_type, el_size);

    while (blocks_left) {
	char *src, *dest;

	DLOOP_Assert(curblock < count);
	cur_block_sz = blockarray[curblock];

	cbufp = (char*) MPI_AINT_CAST_TO_VOID_PTR
	                ((MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->userbuf)) +
					 rel_off + offsetarray[curblock]);

	if (cur_block_sz > blocks_left) cur_block_sz = blocks_left;

	if (paramp->direction == DLOOP_M2M_TO_USERBUF) {
	    src  = paramp->streambuf;
	    dest = cbufp;
	}
	else {
	    src  = cbufp;
	    dest = paramp->streambuf;
	}

	/* note: macro modifies dest buffer ptr, so we must reset */
	if (el_size == 8
	    MPIR_ALIGN8_TEST(src, dest))
	{
	    MPIDI_COPY_FROM_VEC(src, dest, 0, int64_t, cur_block_sz, 1);
	}
	else if (el_size == 4
		 MPIR_ALIGN4_TEST(src,dest))
	{
	    MPIDI_COPY_FROM_VEC(src, dest, 0, int32_t, cur_block_sz, 1);
	}
	else if (el_size == 2) {
	    MPIDI_COPY_FROM_VEC(src, dest, 0, int16_t, cur_block_sz, 1);
	}
	else {
	    memcpy(dest, src, cur_block_sz * el_size);
	}

	paramp->streambuf = (char*) MPI_AINT_CAST_TO_VOID_PTR
 	                            ((MPI_PTR_DISP_CAST_TO_MPI_AINT (paramp->streambuf)) +
				     (cur_block_sz * el_size) );
	blocks_left -= cur_block_sz;
	curblock++;
    }

    return 0;
}

void PREPEND_PREFIX(Segment_pack)(DLOOP_Segment *segp,
				  DLOOP_Offset   first,
				  DLOOP_Offset  *lastp,
				  void *streambuf)
{
    struct PREPEND_PREFIX(m2m_params) params;

    /* experimenting with discarding buf value in the segment, keeping in
     * per-use structure instead. would require moving the parameters around a
     * bit.
     */
    params.userbuf   = segp->ptr;
    params.streambuf = streambuf;
    params.direction = DLOOP_M2M_FROM_USERBUF;

    PREPEND_PREFIX(Segment_manipulate)(segp, first, lastp,
				       PREPEND_PREFIX(Segment_contig_m2m),
				       PREPEND_PREFIX(Segment_vector_m2m),
				       PREPEND_PREFIX(Segment_blkidx_m2m),
				       PREPEND_PREFIX(Segment_index_m2m),
				       NULL, /* size fn */
				       &params);
    return;
}

void PREPEND_PREFIX(Segment_unpack)(DLOOP_Segment *segp,
				    DLOOP_Offset   first,
				    DLOOP_Offset  *lastp,
				    void *streambuf)
{
    struct PREPEND_PREFIX(m2m_params) params;

    /* experimenting with discarding buf value in the segment, keeping in
     * per-use structure instead. would require moving the parameters around a
     * bit.
     */
    params.userbuf   = segp->ptr;
    params.streambuf = streambuf;
    params.direction = DLOOP_M2M_TO_USERBUF;

    PREPEND_PREFIX(Segment_manipulate)(segp, first, lastp,
				       PREPEND_PREFIX(Segment_contig_m2m),
				       PREPEND_PREFIX(Segment_vector_m2m),
				       PREPEND_PREFIX(Segment_blkidx_m2m),
				       PREPEND_PREFIX(Segment_index_m2m),
				       NULL, /* size fn */
				       &params);
    return;
}

struct PREPEND_PREFIX(contig_blocks_params) {
    DLOOP_Count  count;
    DLOOP_Offset last_loc;
};

/* MPID_Segment_contig_count_block
 *
 * Note: because bufp is just an offset, we can ignore it in our
 *       calculations of # of contig regions.
 */
static int DLOOP_Segment_contig_count_block(DLOOP_Offset *blocks_p,
					    DLOOP_Type el_type,
					    DLOOP_Offset rel_off,
					    DLOOP_Buffer bufp ATTRIBUTE((unused)),
					    void *v_paramp)
{
    DLOOP_Offset size, el_size;
    struct PREPEND_PREFIX(contig_blocks_params) *paramp = v_paramp;

    DLOOP_Assert(*blocks_p > 0);

    DLOOP_Handle_get_size_macro(el_type, el_size);
    size = *blocks_p * el_size;

#ifdef MPID_SP_VERBOSE
    MPIU_dbg_printf("contig count block: count = %d, buf+off = %d, lastloc = " MPI_AINT_FMT_DEC_SPEC "\n",
		    (int) paramp->count,
		    (int) ((char *) bufp + rel_off),
		    paramp->last_loc);
#endif

    if (paramp->count > 0 && rel_off == paramp->last_loc)
    {
	/* this region is adjacent to the last */
	paramp->last_loc += size;
    }
    else {
	/* new region */
	paramp->last_loc = rel_off + size;
	paramp->count++;
    }
    return 0;
}

/* DLOOP_Segment_vector_count_block
 *
 * Input Parameters:
 * blocks_p - [inout] pointer to a count of blocks (total, for all noncontiguous pieces)
 * count    - # of noncontiguous regions
 * blksz    - size of each noncontiguous region
 * stride   - distance in bytes from start of one region to start of next
 * el_type - elemental type (e.g. MPI_INT)
 * ...
 *
 * Note: this is only called when the starting position is at the beginning
 * of a whole block in a vector type.
 */
static int DLOOP_Segment_vector_count_block(DLOOP_Offset *blocks_p,
					    DLOOP_Count count,
					    DLOOP_Count blksz,
					    DLOOP_Offset stride,
					    DLOOP_Type el_type,
					    DLOOP_Offset rel_off, /* offset into buffer */
					    void *bufp ATTRIBUTE((unused)),
					    void *v_paramp)
{
    DLOOP_Count new_blk_count;
    DLOOP_Offset size, el_size;
    struct PREPEND_PREFIX(contig_blocks_params) *paramp = v_paramp;

    DLOOP_Assert(count > 0 && blksz > 0 && *blocks_p > 0);

    DLOOP_Handle_get_size_macro(el_type, el_size);
    size = el_size * blksz;
    new_blk_count = count;

    /* if size == stride, then blocks are adjacent to one another */
    if (size == stride) new_blk_count = 1;

    if (paramp->count > 0 && rel_off == paramp->last_loc)
    {
	/* first block sits at end of last block */
	new_blk_count--;
    }

    paramp->last_loc = rel_off + ((DLOOP_Offset)(count-1)) * stride + size;
    paramp->count += new_blk_count;
    return 0;
}

/* DLOOP_Segment_blkidx_count_block
 *
 * Note: this is only called when the starting position is at the
 * beginning of a whole block in a blockindexed type.
 */
static int DLOOP_Segment_blkidx_count_block(DLOOP_Offset *blocks_p,
					    DLOOP_Count count,
					    DLOOP_Count blksz,
					    DLOOP_Offset *offsetarray,
					    DLOOP_Type el_type,
					    DLOOP_Offset rel_off,
					    void *bufp ATTRIBUTE((unused)),
					    void *v_paramp)
{
    DLOOP_Count i, new_blk_count;
    DLOOP_Offset size, el_size, last_loc;
    struct PREPEND_PREFIX(contig_blocks_params) *paramp = v_paramp;

    DLOOP_Assert(count > 0 && blksz > 0 && *blocks_p > 0);

    DLOOP_Handle_get_size_macro(el_type, el_size);
    size = el_size * (DLOOP_Offset) blksz;
    new_blk_count = count;

    if (paramp->count > 0 && ((rel_off + offsetarray[0]) == paramp->last_loc))
    {
	/* first block sits at end of last block */
	new_blk_count--;
    }

    last_loc = rel_off + offsetarray[0] + size;
    for (i=1; i < count; i++) {
	if (last_loc == rel_off + offsetarray[i]) new_blk_count--;

	last_loc = rel_off + offsetarray[i] + size;
    }

    paramp->last_loc = last_loc;
    paramp->count += new_blk_count;
    return 0;
}

/* DLOOP_Segment_index_count_block
 *
 * Note: this is only called when the starting position is at the
 * beginning of a whole block in an indexed type.
 */
static int DLOOP_Segment_index_count_block(DLOOP_Offset *blocks_p,
					   DLOOP_Count count,
					   DLOOP_Count *blockarray,
					   DLOOP_Offset *offsetarray,
					   DLOOP_Type el_type,
					   DLOOP_Offset rel_off,
					   void *bufp ATTRIBUTE((unused)),
					   void *v_paramp)
{
    DLOOP_Count new_blk_count;
    DLOOP_Offset el_size, last_loc;
    struct PREPEND_PREFIX(contig_blocks_params) *paramp = v_paramp;

    DLOOP_Assert(count > 0 && *blocks_p > 0);

    DLOOP_Handle_get_size_macro(el_type, el_size);
    new_blk_count = count;

    if (paramp->count > 0 && ((rel_off + offsetarray[0]) == paramp->last_loc))
    {
	/* first block sits at end of last block */
	new_blk_count--;
    }

    /* Note: when we build an indexed type we combine adjacent regions,
     *       so we're not going to go through and check every piece
     *       separately here. if someone else were building indexed
     *       dataloops by hand, then the loop here might be necessary.
     *       DLOOP_Count i and DLOOP_Offset size would need to be
     *       declared above.
     */
#if 0
    last_loc = rel_off * offsetarray[0] + ((DLOOP_Offset) blockarray[0]) * el_size;
    for (i=1; i < count; i++) {
	if (last_loc == rel_off + offsetarray[i]) new_blk_count--;

	last_loc = rel_off + offsetarray[i] + ((DLOOP_Offset) blockarray[i]) * el_size;
    }
#else
    last_loc = rel_off + offsetarray[count-1] + ((DLOOP_Offset) blockarray[count-1]) * el_size;
#endif

    paramp->last_loc = last_loc;
    paramp->count += new_blk_count;
    return 0;
}

/* DLOOP_Segment_count_contig_blocks()
 *
 * Count number of contiguous regions in segment between first and last.
 */
void PREPEND_PREFIX(Segment_count_contig_blocks)(DLOOP_Segment *segp,
						 DLOOP_Offset first,
						 DLOOP_Offset *lastp,
						 DLOOP_Count *countp)
{
    struct PREPEND_PREFIX(contig_blocks_params) params;

    params.count    = 0;
    params.last_loc = 0;

    /* FIXME: The blkidx and index functions are not used since they
     * optimize the count by coalescing contiguous segments, while
     * functions using the count do not optimize in the same way
     * (e.g., flatten code) */
    PREPEND_PREFIX(Segment_manipulate)(segp,
				       first,
				       lastp,
				       DLOOP_Segment_contig_count_block,
				       DLOOP_Segment_vector_count_block,
				       DLOOP_Segment_blkidx_count_block,
				       DLOOP_Segment_index_count_block,
				       NULL, /* size fn */
				       (void *) &params);

    *countp = params.count;
    return;
}

/********** FUNCTIONS FOR FLATTENING INTO MPI OFFSETS AND BLKLENS  **********/

/* Segment_mpi_flatten
 *
 * Flattens into a set of blocklengths and displacements, as in an
 * MPI hindexed type. Note that we use appropriately-sized variables
 * in the associated params structure for this reason.
 *
 * NOTE: blocks will be in units of bytes when returned.
 *
 * WARNING: there's potential for overflow here as we convert from
 *          various types into an index of bytes.
 */
struct PREPEND_PREFIX(mpi_flatten_params) {
    int       index, length;
    MPI_Aint  last_end;
    int      *blklens;
    MPI_Aint *disps;
};

/* DLOOP_Segment_contig_mpi_flatten
 *
 */
static int DLOOP_Segment_contig_mpi_flatten(DLOOP_Offset *blocks_p,
					    DLOOP_Type el_type,
					    DLOOP_Offset rel_off,
					    void *bufp,
					    void *v_paramp)
{
    int last_idx, size;
    DLOOP_Offset el_size;
    char *last_end = NULL;
    struct PREPEND_PREFIX(mpi_flatten_params) *paramp = v_paramp;

    DLOOP_Handle_get_size_macro(el_type, el_size);
    size = *blocks_p * el_size;

    last_idx = paramp->index - 1;
    if (last_idx >= 0) {
	/* Since disps can be negative, we cannot use
	 * MPID_Ensure_Aint_fits_in_pointer to verify that disps +
	 * blklens fits in a pointer.  Just let it truncate, if the
	 * sizeof a pointer is less than the sizeof an MPI_Aint.
	 */
	last_end = (char*) MPI_AINT_CAST_TO_VOID_PTR
	           (paramp->disps[last_idx] + ((DLOOP_Offset) paramp->blklens[last_idx]));
    }

    /* Since bufp can be a displacement and can be negative, we cannot
     * use MPID_Ensure_Aint_fits_in_pointer to ensure the sum fits in
     * a pointer.  Just let it truncate.
     */
    if ((last_idx == paramp->length-1) &&
        (last_end != ((char *) bufp + rel_off)))
    {
	/* we have used up all our entries, and this region doesn't fit on
	 * the end of the last one.  setting blocks to 0 tells manipulation
	 * function that we are done (and that we didn't process any blocks).
	 */
	*blocks_p = 0;
	return 1;
    }
    else if (last_idx >= 0 && (last_end == ((char *) bufp + rel_off)))
    {
	/* add this size to the last vector rather than using up another one */
	paramp->blklens[last_idx] += size;
    }
    else {
	/* Since bufp can be a displacement and can be negative, we cannot use
	 * MPI_VOID_PTR_CAST_TO_MPI_AINT to cast the sum to a pointer.  Just let it
	 * sign extend.
	 */
        paramp->disps[last_idx+1]   = MPI_PTR_DISP_CAST_TO_MPI_AINT bufp + rel_off;
	paramp->blklens[last_idx+1] = size;
	paramp->index++;
    }
    return 0;
}

/* DLOOP_Segment_vector_mpi_flatten
 *
 * Input Parameters:
 * blocks_p - [inout] pointer to a count of blocks (total, for all noncontiguous pieces)
 * count    - # of noncontiguous regions
 * blksz    - size of each noncontiguous region
 * stride   - distance in bytes from start of one region to start of next
 * el_type - elemental type (e.g. MPI_INT)
 * ...
 *
 * Note: this is only called when the starting position is at the beginning
 * of a whole block in a vector type.
 *
 * TODO: MAKE THIS CODE SMARTER, USING THE SAME GENERAL APPROACH AS IN THE
 *       COUNT BLOCK CODE ABOVE.
 */
static int DLOOP_Segment_vector_mpi_flatten(DLOOP_Offset *blocks_p,
					    DLOOP_Count count,
					    DLOOP_Count blksz,
					    DLOOP_Offset stride,
					    DLOOP_Type el_type,
					    DLOOP_Offset rel_off, /* offset into buffer */
					    void *bufp, /* start of buffer */
					    void *v_paramp)
{
    int i, size, blocks_left;
    DLOOP_Offset el_size;
    struct PREPEND_PREFIX(mpi_flatten_params) *paramp = v_paramp;

    DLOOP_Handle_get_size_macro(el_type, el_size);
    blocks_left = (int)*blocks_p;

    for (i=0; i < count && blocks_left > 0; i++) {
	int last_idx;
	char *last_end = NULL;

	if (blocks_left > blksz) {
	    size = blksz * (int) el_size;
	    blocks_left -= blksz;
	}
	else {
	    /* last pass */
	    size = blocks_left * (int) el_size;
	    blocks_left = 0;
	}

	last_idx = paramp->index - 1;
	if (last_idx >= 0) {
	    /* Since disps can be negative, we cannot use
	     * MPID_Ensure_Aint_fits_in_pointer to verify that disps +
	     * blklens fits in a pointer.  Nor can we use
	     * MPI_AINT_CAST_TO_VOID_PTR to cast the sum to a pointer.
	     * Just let it truncate, if the sizeof a pointer is less
	     * than the sizeof an MPI_Aint.
	     */
	    last_end = (char *) MPI_AINT_CAST_TO_VOID_PTR
		       (paramp->disps[last_idx] +
			 (MPI_Aint)(paramp->blklens[last_idx]));
	}

	/* Since bufp can be a displacement and can be negative, we cannot use
	 * MPID_Ensure_Aint_fits_in_pointer to ensure the sum fits in a pointer.
	 * Just let it truncate.
	 */
        if ((last_idx == paramp->length-1) &&
            (last_end != ((char *) bufp + rel_off)))
	{
	    /* we have used up all our entries, and this one doesn't fit on
	     * the end of the last one.
	     */
	    *blocks_p -= (blocks_left + (size / (int) el_size));
#ifdef MPID_SP_VERBOSE
	    MPIU_dbg_printf("\t[vector to vec exiting (1): next ind = %d, " MPI_AINT_FMT_DEC_SPEC " blocks processed.\n",
			    paramp->u.pack_vector.index,
			    *blocks_p);
#endif
	    return 1;
	}
        else if (last_idx >= 0 && (last_end == ((char *) bufp + rel_off)))
	{
	    /* add this size to the last vector rather than using up new one */
	    paramp->blklens[last_idx] += size;
	}
	else {
	    /* Since bufp can be a displacement and can be negative, we cannot use
	     * MPI_VOID_PTR_CAST_TO_MPI_AINT to cast the sum to a pointer.  Just let it
	     * sign extend.
	     */
            paramp->disps[last_idx+1]   = MPI_PTR_DISP_CAST_TO_MPI_AINT bufp + rel_off;
	    paramp->blklens[last_idx+1] = size;
	    paramp->index++;
	}

	rel_off += stride;
    }

#ifdef MPID_SP_VERBOSE
    MPIU_dbg_printf("\t[vector to vec exiting (2): next ind = %d, " MPI_AINT_FMT_DEC_SPEC " blocks processed.\n",
		    paramp->u.pack_vector.index,
		    *blocks_p);
#endif

    /* if we get here then we processed ALL the blocks; don't need to update
     * blocks_p
     */

    DLOOP_Assert(blocks_left == 0);
    return 0;
}

static int DLOOP_Segment_blkidx_mpi_flatten(DLOOP_Offset *blocks_p,
                                            DLOOP_Count count,
                                            DLOOP_Count blksz,
                                            DLOOP_Offset *offsetarray,
                                            DLOOP_Type el_type,
                                            DLOOP_Offset rel_off,
                                            void *bufp,
                                            void *v_paramp)
{
    int i, size, blocks_left;
    DLOOP_Offset el_size;
    struct PREPEND_PREFIX(mpi_flatten_params) *paramp = v_paramp;

    DLOOP_Handle_get_size_macro(el_type, el_size);
    blocks_left = *blocks_p;

    for (i=0; i < count && blocks_left > 0; i++) {
	int last_idx;
	char *last_end = NULL;

	if (blocks_left > blksz) {
	    size = blksz * (int) el_size;
	    blocks_left -= blksz;
	}
	else {
	    /* last pass */
	    size = blocks_left * (int) el_size;
	    blocks_left = 0;
	}

	last_idx = paramp->index - 1;
	if (last_idx >= 0) {
	    /* Since disps can be negative, we cannot use
	     * MPID_Ensure_Aint_fits_in_pointer to verify that disps +
	     * blklens fits in a pointer.  Nor can we use
	     * MPI_AINT_CAST_TO_VOID_PTR to cast the sum to a pointer.
	     * Just let it truncate, if the sizeof a pointer is less
	     * than the sizeof an MPI_Aint.
	     */
	    last_end = (char*) MPI_AINT_CAST_TO_VOID_PTR
		       (paramp->disps[last_idx] + ((DLOOP_Offset) paramp->blklens[last_idx]));
	}

	/* Since bufp can be a displacement and can be negative, we
	 * cannot use MPID_Ensure_Aint_fits_in_pointer to ensure the
	 * sum fits in a pointer.  Just let it truncate.
	 */
        if ((last_idx == paramp->length-1) &&
            (last_end != ((char *) bufp + rel_off)))
	{
	    /* we have used up all our entries, and this one doesn't fit on
	     * the end of the last one.
	     */
	    *blocks_p -= ((DLOOP_Offset) blocks_left + (((DLOOP_Offset) size) / el_size));
#ifdef MPID_SP_VERBOSE
	    MPIU_dbg_printf("\t[vector to vec exiting (1): next ind = %d, %d blocks processed.\n",
			    paramp->u.pack_vector.index,
			    (int) *blocks_p);
#endif
	    return 1;
	}
        else if (last_idx >= 0 && (last_end == ((char *) bufp + rel_off)))
	{
	    /* add this size to the last vector rather than using up new one */
	    paramp->blklens[last_idx] += size;
	}
	else {
	    /* Since bufp can be a displacement and can be negative, we cannot use
	     * MPI_VOID_PTR_CAST_TO_MPI_AINT to cast the sum to a pointer.  Just let it
	     * sign extend.
	     */
            paramp->disps[last_idx+1]   = MPI_PTR_DISP_CAST_TO_MPI_AINT bufp + rel_off + offsetarray[last_idx+1];
	    paramp->blklens[last_idx+1] = size;
	    paramp->index++;
	}

	rel_off += offsetarray[i];
    }

#ifdef MPID_SP_VERBOSE
    MPIU_dbg_printf("\t[vector to vec exiting (2): next ind = %d, " MPI_AINT_FMT_DEC_SPEC " blocks processed.\n",
		    paramp->u.pack_vector.index,
		    *blocks_p);
#endif

    /* if we get here then we processed ALL the blocks; don't need to update
     * blocks_p
     */

    DLOOP_Assert(blocks_left == 0);
    return 0;
}

static int DLOOP_Segment_index_mpi_flatten(DLOOP_Offset *blocks_p,
					   DLOOP_Count count,
					   DLOOP_Count *blockarray,
					   DLOOP_Offset *offsetarray,
					   DLOOP_Type el_type,
					   DLOOP_Offset rel_off,
					   void *bufp,
					   void *v_paramp)
{
    int i, size, blocks_left;
    DLOOP_Offset el_size;
    struct PREPEND_PREFIX(mpi_flatten_params) *paramp = v_paramp;

    DLOOP_Handle_get_size_macro(el_type, el_size);
    blocks_left = *blocks_p;

    for (i=0; i < count && blocks_left > 0; i++) {
	int last_idx;
	char *last_end = NULL;

	if (blocks_left > blockarray[i]) {
	    size = blockarray[i] * (int) el_size;
	    blocks_left -= blockarray[i];
	}
	else {
	    /* last pass */
	    size = blocks_left * (int) el_size;
	    blocks_left = 0;
	}

	last_idx = paramp->index - 1;
	if (last_idx >= 0) {
	    /* Since disps can be negative, we cannot use
	     * MPID_Ensure_Aint_fits_in_pointer to verify that disps +
	     * blklens fits in a pointer.  Nor can we use
	     * MPI_AINT_CAST_TO_VOID_PTR to cast the sum to a pointer.
	     * Just let it truncate, if the sizeof a pointer is less
	     * than the sizeof an MPI_Aint.
	     */
	    last_end = (char *) MPI_AINT_CAST_TO_VOID_PTR
		       (paramp->disps[last_idx] +
			(MPI_Aint)(paramp->blklens[last_idx]));
	}

	/* Since bufp can be a displacement and can be negative, we
	 * cannot use MPID_Ensure_Aint_fits_in_pointer to ensure the
	 * sum fits in a pointer.  Just let it truncate.
	 */
        if ((last_idx == paramp->length-1) &&
            (last_end != ((char *) bufp + rel_off)))
	{
	    /* we have used up all our entries, and this one doesn't fit on
	     * the end of the last one.
	     */
	    *blocks_p -= (blocks_left + (size / (int) el_size));
	    return 1;
	}
        else if (last_idx >= 0 && (last_end == ((char *) bufp + rel_off)))
	{
	    /* add this size to the last vector rather than using up new one */
	    paramp->blklens[last_idx] += size;
	}
	else {
	    /* Since bufp can be a displacement and can be negative, we cannot
	     * use MPI_VOID_PTR_CAST_TO_MPI_AINT to cast the sum to a pointer.
	     * Just let it sign extend.
	     */
            paramp->disps[last_idx+1]   = MPI_PTR_DISP_CAST_TO_MPI_AINT bufp +
		rel_off + offsetarray[last_idx+1];
	    paramp->blklens[last_idx+1] = size;
	    paramp->index++;
	}

	rel_off += offsetarray[i];
    }

    /* if we get here then we processed ALL the blocks; don't need to update
     * blocks_p
     */

    DLOOP_Assert(blocks_left == 0);
    return 0;
}

/* MPID_Segment_mpi_flatten - flatten a type into a representation
 *                            appropriate for passing to hindexed create.
 *
 * Parameters:
 * segp    - pointer to segment structure
 * first   - first byte in segment to pack
 * lastp   - in/out parameter describing last byte to pack (and afterwards
 *           the last byte _actually_ packed)
 *           NOTE: actually returns index of byte _after_ last one packed
 * blklens, disps - the usual blocklength and displacement arrays for MPI
 * lengthp - in/out parameter describing length of array (and afterwards
 *           the amount of the array that has actual data)
 */
void PREPEND_PREFIX(Segment_mpi_flatten)(DLOOP_Segment *segp,
					 DLOOP_Offset first,
					 DLOOP_Offset *lastp,
					 int *blklens,
					 MPI_Aint *disps,
					 int *lengthp)
{
    struct PREPEND_PREFIX(mpi_flatten_params) params;

    DLOOP_Assert(*lengthp > 0);

    params.index   = 0;
    params.length  = *lengthp;
    params.blklens = blklens;
    params.disps   = disps;

    PREPEND_PREFIX(Segment_manipulate)(segp,
				       first,
				       lastp,
				       DLOOP_Segment_contig_mpi_flatten,
				       DLOOP_Segment_vector_mpi_flatten,
				       DLOOP_Segment_blkidx_mpi_flatten,
				       DLOOP_Segment_index_mpi_flatten,
				       NULL,
				       &params);

    /* last value already handled by MPID_Segment_manipulate */
    *lengthp = params.index;
    return;
}

/*
 * Local variables:
 * c-indent-tabs-mode: nil
 * End:
 */
