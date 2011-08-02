/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file include/mpidi_constants.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpidi_constants_h__
#define __include_mpidi_constants_h__

enum
  {
    /* N is "number of operations" and P is number of ranks */
    MPIDI_VERBOSE_NONE        = 0, /**< Do not print any verbose information */
    MPIDI_VERBOSE_SUMMARY_0   = 1, /**< Print summary information on rank 0.     O(1) lines printed. */
    MPIDI_VERBOSE_SUMMARY_ALL = 2, /**< Print summary information on all ranks.  O(P) lines printed. */
    MPIDI_VERBOSE_DETAILS_0   = 2, /**< Print detailed information on rank 0.    O(N) lines printed. */
    MPIDI_VERBOSE_DETAILS_ALL = 3, /**< Print detailed information on all ranks. O(P*N) lines printed. */
  };


/**
 * \defgroup Allgather(v) optimization datatype info
 * \{
 */
enum
  {
    MPID_SEND_CONTIG     = 0, /**< Contiguous send buffer */
    MPID_RECV_CONTIG     = 1, /**< Contiguous recv buffer */
    MPID_RECV_CONTINUOUS = 2, /**< Continuous recv buffer */
    MPID_LARGECOUNT      = 3, /**< Total send count is "large" */
    MPID_MEDIUMCOUNT     = 4, /**< Total send count is "medium" */
    MPID_ALIGNEDBUFFER   = 5, /**< Buffers are 16b aligned */
  };

enum
  {
    MPID_ALLGATHER_PREALLREDUCE  = 0,
    MPID_ALLGATHERV_PREALLREDUCE = 1,
    MPID_ALLREDUCE_PREALLREDUCE  = 2,
    MPID_BCAST_PREALLREDUCE      = 3,
    MPID_SCATTERV_PREALLREDUCE   = 4,
    MPID_GATHER_PREALLREDUCE     = 5,
    MPID_NUM_PREALLREDUCES       = 6,
  };

enum
  {
    MPID_COLL_NOQUERY           = 0,
    MPID_COLL_QUERY             = 1,
    /* Can we cache stuff? If not set to ALWAYS_QUERY */
    MPID_COLL_ALWAYS_QUERY      = 2,
    MPID_COLL_CHECK_FN_REQUIRED = 3,
    MPID_COLL_USE_MPICH         = 4,
  };
/** \} */

#endif
