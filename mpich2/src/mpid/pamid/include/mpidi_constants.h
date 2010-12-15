/*  (C)Copyright IBM Corp.  2007, 2008  */
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


/**
 * \defgroup Allgather(v) optimization datatype info
 * \{
 */
#define MPID_SEND_CONTIG        0 /**< Contiguous send buffer */
#define MPID_RECV_CONTIG        1 /**< Contiguous recv buffer */
#define MPID_RECV_CONTINUOUS    2 /**< Continuous recv buffer */
#define MPID_LARGECOUNT         3 /**< Total send count is "large" */
#define MPID_MEDIUMCOUNT        4 /**< Total send count is "medium" */
#define MPID_ALIGNEDBUFFER      5 /**< Buffers are 16b aligned */

#define MPID_ALLGATHER_PREALLREDUCE 0
#define MPID_ALLGATHERV_PREALLREDUCE 1
#define MPID_ALLREDUCE_PREALLREDUCE 2
#define MPID_BCAST_PREALLREDUCE 3
#define MPID_SCATTERV_PREALLREDUCE 4
#define MPID_NUM_PREALLREDUCES 5

/** \} */

#endif
