/* begin_generated_IBM_copyright_prolog                             */
/*                                                                  */
/* This is an automatically generated copyright prolog.             */
/* After initializing,  DO NOT MODIFY OR MOVE                       */
/*  --------------------------------------------------------------- */
/* Licensed Materials - Property of IBM                             */
/* Blue Gene/Q 5765-PER 5765-PRP                                    */
/*                                                                  */
/* (C) Copyright IBM Corp. 2011, 2012 All Rights Reserved           */
/* US Government Users Restricted Rights -                          */
/* Use, duplication, or disclosure restricted                       */
/* by GSA ADP Schedule Contract with IBM Corp.                      */
/*                                                                  */
/*  --------------------------------------------------------------- */
/*                                                                  */
/* end_generated_IBM_copyright_prolog                               */
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
    MPID_SEND_CONTINUOUS = 1, /**< Continuous send buffer */
    MPID_RECV_CONTIG     = 2, /**< Contiguous recv buffer */
    MPID_RECV_CONTINUOUS = 3, /**< Continuous recv buffer */
    MPID_LARGECOUNT      = 4, /**< Total send count is "large" */
    MPID_MEDIUMCOUNT     = 5, /**< Total send count is "medium" */
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

enum /* The type of protocol selected */
  {
    /* User (env var) or mpid_optcoll selected algorithms                   */
    /* (numeric order matters-low values are 'optimized', high are defaults */
    MPID_COLL_NOQUERY           = 0, /* always works algorithm list         */
    MPID_COLL_QUERY             = 1, /* must query algorithm list           */
    MPID_COLL_USE_MPICH         = 2, /* MPICH (non-PAMI) default algorithm  */
    /* Collectives 'default' to first available query/no-query protocol     */
    MPID_COLL_DEFAULT           = 3, /* No user selection or override       */
    MPID_COLL_DEFAULT_QUERY     = 4, /* No user selection or override       */
  } ;

enum
 {
   MPID_COLL_OFF = 0,
   MPID_COLL_ON  = 1,
   MPID_COLL_FCA = 2, /* Selecting these is fairly easy so special case */
 };
/** \} */


enum
{
MPID_EPOTYPE_NONE      = 0,       /**< No epoch in affect */
MPID_EPOTYPE_LOCK      = 1,       /**< MPI_Win_lock access epoch */
MPID_EPOTYPE_START     = 2,       /**< MPI_Win_start access epoch */
MPID_EPOTYPE_POST      = 3,       /**< MPI_Win_post exposure epoch */
MPID_EPOTYPE_FENCE     = 4,       /**< MPI_Win_fence access/exposure epoch */
MPID_EPOTYPE_REFENCE   = 5,       /**< MPI_Win_fence possible access/exposure epoch */
};

enum /* PAMID_COLLECTIVES_MEMORY_OPTIMIZED levels */
 
{
  MPID_OPT_LVL_IRREG     = 1,       /**< Do not optimize irregular communicators */
  MPID_OPT_LVL_NONCONTIG = 2,       /**< Disable some non-contig collectives     */
  MPID_OPT_LVL_GLUE      = 4,       /**< Disable memory-intensive glue collectives*/
};
#endif
