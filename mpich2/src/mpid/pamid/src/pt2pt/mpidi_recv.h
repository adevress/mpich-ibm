/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpidi_recv.h
 * \brief ADI level implemenation of MPI_Irecv()
 */
#include <mpidimpl.h>

#ifndef __src_pt2pt_mpidi_recv_h__
#define __src_pt2pt_mpidi_recv_h__

#include "../mpid_recvq.h"


/**
 * \brief ADI level implemenation of MPI_(I)Recv()
 *
 * \param[in]  buf            The buffer to receive into
 * \param[in]  count          Number of expected elements in the buffer
 * \param[in]  datatype       The datatype of each element
 * \param[in]  rank           The sending rank
 * \param[in]  tag            The message tag
 * \param[in]  comm           Pointer to the communicator
 * \param[in]  context_offset Offset from the communicator context ID
 * \param[out] status         Update the status structure
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
  MPID_Request * rreq;
  int found;
  int mpi_errno = MPI_SUCCESS;

  /* ---------------------------------------- */
  /* NULL rank means empty request            */
  /* ---------------------------------------- */
  if (unlikely(rank == MPI_PROC_NULL))
    {
      MPIDI_RecvMsg_procnull(comm, is_blocking, status, request);
      return MPI_SUCCESS;
    }

  MPIR_Comm_add_ref(comm);

  MPIU_THREAD_CS_ENTER(MSGQUEUE,0);
  /* ---------------------------------------- */
  /* find our request in the unexpected queue */
  /* or allocate one in the posted queue      */
  /* ---------------------------------------- */
  rreq = MPIDI_Recvq_FDU_or_AEP(rank,
                                tag,
                                comm->recvcontext_id + context_offset,
                                &found);

  /* ----------------------------------------------------------------- */
  /* populate request with our data                                    */
  /* We can do this because this is not a multithreaded implementation */
  /* ----------------------------------------------------------------- */

  rreq->comm              = comm;
  rreq->mpid.userbuf      = buf;
  rreq->mpid.userbufcount = count;
  rreq->mpid.datatype     = datatype;
  /* We don't need this because MPIDI_CA_COMPLETE is the initialized default */
  /* MPIDI_Request_setCA(rreq, MPIDI_CA_COMPLETE); */

  if (found)
    {
      MPIDI_RecvMsg_Unexp(rreq, buf, count, datatype);
      mpi_errno = rreq->status.MPI_ERROR;
    }
  else
    {
      /* ----------------------------------------------------------- */
      /* request not found in unexpected queue, allocated and posted */
      /* ----------------------------------------------------------- */
      if (HANDLE_GET_KIND(datatype) != HANDLE_KIND_BUILTIN)
        {
          MPID_Datatype_get_ptr(datatype, rreq->mpid.datatype_ptr);
          MPID_Datatype_add_ref(rreq->mpid.datatype_ptr);
        }
    }

  *request = rreq;
  if (status != MPI_STATUS_IGNORE)
    *status = rreq->status;

  MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
  return mpi_errno;
}

#endif
