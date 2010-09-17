/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_send.h
 *
 * This is a generic inline send operator.  It groups code previous
 * found in mpid_{,i}{,s}send.c.  The hope is that the common code
 * will be easier to maintain, and that the inline nature of the flags
 * will ensure high performance.
 */
#include "mpidimpl.h"

#ifndef __src_pt2pt_mpidi_send_h__
#define __src_pt2pt_mpidi_send_h__

/**
 * \brief This is a generic inline verion of the various send functions.
 *
 * This is the same function prototype as MPID_{,I}{,S}send.c, but it
 * inludes two inline flags: is_blocking and is_sync.  These are not
 * meant to be truely variable, but are expected to be known at
 * compile time.  That *should* allow the compiler to remove any extra
 * code from this function that won't be used in the specific inlined
 * version.
 *
 * The MPIDI_SendMsg function may queue this send operation for later
 * handling and then return to the user.  Therefore, this function has
 * only two goals:
 *
 *   + Fill in the request oject with all relevant information, and
 *   + Do any reference counting that must be done before the function returns.
 *
 *
 * \param[in]  buf            The buffer to send
 * \param[in]  count          Number of elements in the buffer
 * \param[in]  datatype       The datatype of each element
 * \param[in]  rank           The destination rank
 * \param[in]  tag            The message tag
 * \param[in]  comm           Pointer to the communicator
 * \param[in]  context_offset Offset from the communicator context ID
 * \param[in]  is_blocking    Is this a blocking request? (i.e. one of MPI_Send or MPI_Ssend)
 * \param[in]  is_sync        Is this a synchronous request? (i.e. of of MPI_Ssend or MPI_Issend)
 * \param[out] request        Return a pointer to the new request object
 *
 * \returns An MPI Error code
 */
static inline int
MPIDI_Send(const void    * buf,
           int             count,
           MPI_Datatype    datatype,
           int             rank,
           int             tag,
           MPID_Comm     * comm,
           int             context_offset,
           unsigned        is_blocking,
           unsigned        is_sync,
           MPID_Request ** request)
{
  MPID_Request * sreq = NULL;

  /* --------------------------- */
  /* special case: send-to-self  */
  /* --------------------------- */

  if (unlikely( (rank == comm->rank) &&
                (comm->comm_kind != MPID_INTERCOMM) ) )
    {
      /* I'm sending to myself! */
      int mpi_errno = MPIDI_Isend_self(buf,
                                       count,
                                       datatype,
                                       rank,
                                       tag,
                                       comm,
                                       context_offset,
                                       request);
      if (is_blocking)
        if ( (MPIR_ThreadInfo.thread_provided <= MPI_THREAD_FUNNELED) &&
             (sreq != NULL && !MPID_cc_is_complete(&sreq->cc)) )
          {
            *request = NULL;
            mpi_errno = MPIR_Err_create_code(MPI_SUCCESS,
                                             MPIR_ERR_FATAL,
                                             __FUNCTION__,
                                             __LINE__,
                                             MPI_ERR_OTHER,
                                             "**dev|selfsenddeadlock", 0);
            return mpi_errno;
          }
      return mpi_errno;
    }

  /* --------------------- */
  /* create a send request */
  /* --------------------- */

  sreq = MPID_Request_create();
  if (unlikely(sreq == NULL))
    {
      *request = NULL;
      int mpi_errno = MPIR_Err_create_code(MPI_SUCCESS,
                                           MPIR_ERR_FATAL,
                                           __FUNCTION__,
                                           __LINE__,
                                           MPI_ERR_OTHER,
                                           "**nomem", 0);
      return mpi_errno;
    }

  /* match info */
  MPIDI_Request_setMatch(sreq, tag, comm->rank, comm->context_id+context_offset);

  /* data buffer info */
  sreq->mpid.userbuf      = (void*)buf;
  sreq->mpid.userbufcount = count;
  sreq->mpid.datatype     = datatype;

  /* communicator & destination info */
  sreq->comm              = comm;  MPIR_Comm_add_ref(comm);
  if (likely(rank != MPI_PROC_NULL))
    MPIDI_Request_setPeerRank(sreq, comm->vcr[rank]);
  MPIDI_Request_setPeerRequest(sreq, sreq);

  /* message type info */
  sreq->kind = MPID_REQUEST_SEND;
  if (is_sync)
    MPIDI_Request_setSync(sreq, 1);

  /* ------------------------------ */
  /* special case: NULL destination */
  /* ------------------------------ */
  if (unlikely(rank == MPI_PROC_NULL))
    {
      if (is_blocking)
        {
          *request = NULL;
        }
      else
        {
          //sreq->cc = 0;
	  MPID_cc_set (&sreq->cc, 0);
          *request = sreq;
        }
      return MPI_SUCCESS;
    }

  /* ----------------------------------------- */
  /*      start the message                    */
  /* ----------------------------------------- */

  *request = sreq;
  MPIDI_SendMsg(sreq);
  return MPI_SUCCESS;
}

#endif
