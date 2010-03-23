/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file include/mpid_constants.h
 * \brief ???
 */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */


#ifndef __include_mpid_constants_h__
#define __include_mpid_constants_h__


/**
 * \defgroup MPID_EPOTYPE MPID One-sided Epoch Types
 * \{
 */
#define MPID_EPOTYPE_NONE       0       /**< No epoch in affect */
#define MPID_EPOTYPE_LOCK       1       /**< MPI_Win_lock access epoch */
#define MPID_EPOTYPE_START      2       /**< MPI_Win_start access epoch */
#define MPID_EPOTYPE_POST       3       /**< MPI_Win_post exposure epoch */
#define MPID_EPOTYPE_POSTSTART  4       /**< MPI_Win_post+MPI_Win_start access/exposure epoch */
#define MPID_EPOTYPE_FENCE      5       /**< MPI_Win_fence access/exposure epoch */
#define MPID_EPOTYPE_REFENCE    6       /**< MPI_Win_fence possible access/exposure epoch */
/** \} */

/**
 * \defgroup MPID_MSGTYPE MPID One-sided Message Types
 * \{
 */
#define MPID_MSGTYPE_NONE       0       /**< Not a valid message */
#define MPID_MSGTYPE_LOCK       1       /**< lock window */
#define MPID_MSGTYPE_UNLOCK     2       /**< (try) unlock window */
#define MPID_MSGTYPE_POST       3       /**< begin POST epoch */
#define MPID_MSGTYPE_START      4       /**< (not used) */
#define MPID_MSGTYPE_COMPLETE   5       /**< end a START epoch */
#define MPID_MSGTYPE_WAIT       6       /**< (not used) */
#define MPID_MSGTYPE_FENCE      7       /**< (not used) */
#define MPID_MSGTYPE_UNFENCE    8       /**< (not used) */
#define MPID_MSGTYPE_PUT        9       /**< PUT RMA operation */
#define MPID_MSGTYPE_GET        10      /**< GET RMA operation */
#define MPID_MSGTYPE_ACC        11      /**< ACCUMULATE RMA operation */
#define MPID_MSGTYPE_DT_MAP     12      /**< Datatype map payload */
#define MPID_MSGTYPE_DT_IOV     13      /**< Datatype iov payload */
#define MPID_MSGTYPE_LOCKACK    14      /**< lock acknowledge */
#define MPID_MSGTYPE_UNLOCKACK  15      /**< unlock acknowledge, with status */
/** \} */


#endif
