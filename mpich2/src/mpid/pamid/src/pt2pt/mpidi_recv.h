/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_recv.h
 * \brief ADI level implemenation of MPI_Irecv()
 */
#include "mpidimpl.h"

#ifndef __src_pt2pt_mpidi_recv_h__
#define __src_pt2pt_mpidi_recv_h__

/**
 * \brief ADI level implemenation of MPI_Irecv()
 *
 * \param[in]  buf            The buffer to receive into
 * \param[in]  count          Number of expected elements in the buffer
 * \param[in]  datatype       The datatype of each element
 * \param[in]  rank           The sending rank
 * \param[in]  tag            The message tag
 * \param[in]  comm           Pointer to the communicator
 * \param[in]  context_offset Offset from the communicator context ID
 *
 * \param[out] request        Return a pointer to the new request object
 *
 * \returns An MPI Error code
 */
static inline int
MPIDI_Recv(void          * buf,
           int             count,
           MPI_Datatype    datatype,
           int             rank,
           int             tag,
           MPID_Comm     * comm,
           int             context_offset,
           unsigned        is_blocking,
           MPI_Status    * status,
           MPID_Request ** request)
{
  int mpi_errno;

  /* ---------------------------------------- */
  /* NULL rank means empty request            */
  /* ---------------------------------------- */
  if (unlikely(rank == MPI_PROC_NULL))
    {
      if (is_blocking)
        {
          MPIR_Status_set_procnull(status);
          *request = NULL;
        }
      else
        {
          MPID_Request * rreq;
          rreq = MPIDI_Request_create2();
          MPIR_Status_set_procnull(&rreq->status);
          rreq->kind = MPID_REQUEST_RECV;
          rreq->comm = comm;
          MPIR_Comm_add_ref(comm);
          MPIDI_Request_complete(rreq);
          *request = rreq;
        }
      return MPI_SUCCESS;
    }

  mpi_errno = MPIDI_RecvMsg(buf,
                            count,
                            datatype,
                            rank,
                            tag,
                            comm,
                            context_offset,
                            status,
                            request);

   return mpi_errno;
}

#endif
