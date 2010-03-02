/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_irecv.c
 * \brief ADI level implemenation of common recv code.
 */
#include "mpidimpl.h"

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
 * \param[out] status         Update the status structure
 * \param[out] request        Return a pointer to the new request object
 *
 * \returns An MPI Error code
 */
int
MPIDI_Irecv(void          * buf,
            int             count,
            MPI_Datatype    datatype,
            int             rank,
            int             tag,
            MPID_Comm     * comm,
            int             context_offset,
            MPI_Status    * status,
            MPID_Request ** request,
            char          * func)
{
  int mpi_errno = MPI_SUCCESS;
  int found;
  MPID_Request * rreq;

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

  MPIR_Comm_add_ref(comm);
  rreq->comm              = comm;
  rreq->mpid.userbuf      = buf;
  rreq->mpid.userbufcount = count;
  rreq->mpid.datatype     = datatype;
  MPIDI_Request_setCA(rreq, MPIDI_CA_COMPLETE);

  if (found)
    {
      /* ------------------------------------------------------------ */
      /* message was found in unexpected queue                        */
      /* ------------------------------------------------------------ */
      /* We must acknowledge synchronous send requests                */
      /* The recvnew callback will acknowledge the posted messages    */
      /* Recv functions will ack the messages that are unexpected     */
      /* ------------------------------------------------------------ */

      /* -------------------------------- */
      /* request is complete              */
      /* if sync request, need to ack it. */
      /* -------------------------------- */
      if (MPIDI_Request_isSync(rreq))
        {
          MPID_assert(!MPIDI_Request_isSelf(rreq));
          MPIDI_postSyncAck(MPIDI_Context[0], rreq);
        }

      if (MPIDI_Request_isSelf(rreq))
        {
          /* ---------------------- */
          /* "SELF" request         */
          /* ---------------------- */
          MPID_Request * const sreq = rreq->partner_request;
          MPID_assert(sreq != NULL);
          MPIDI_Buffer_copy(sreq->mpid.userbuf,
                            sreq->mpid.userbufcount,
                            sreq->mpid.datatype,
                            &sreq->status.MPI_ERROR,
                            buf,
                            count,
                            datatype,
                            (MPIDI_msg_sz_t*)&rreq->status.count,
                            &rreq->status.MPI_ERROR);
          MPIDI_Request_complete(sreq);
          /* no other thread can possibly be waiting on rreq,
             so it is safe to reset ref_count and cc */
          rreq->cc = 0;
          if (status != MPI_STATUS_IGNORE)
            *status = rreq->status;
          mpi_errno = rreq->status.MPI_ERROR;
        }
      else if (MPIDI_Request_isRzv(rreq))
        {
          /* -------------------------------------------------------- */
          /* Received an expected flow-control rendezvous RTS.        */
          /*     This is very similar to the found/incomplete case    */
          /* -------------------------------------------------------- */
          if (HANDLE_GET_KIND(datatype) != HANDLE_KIND_BUILTIN)
            {
              MPID_Datatype_get_ptr(datatype, rreq->mpid.datatype_ptr);
              MPID_Datatype_add_ref(rreq->mpid.datatype_ptr);
            }

          MPIDI_RendezvousTransfer(MPIDI_Context[0], rreq);
        }
      else if (*rreq->cc_ptr == 0)
        {
          /* -------------------------------- */
          /* request is complete              */
          /* -------------------------------- */
          MPID_assert(rreq->mpid.uebuf != NULL);
          if(rreq->status.cancelled == FALSE)
            MPIDI_Buffer_copy(rreq->mpid.uebuf,
                              rreq->mpid.uebuflen,
                              MPI_CHAR,
                              &mpi_errno,
                              buf,
                              count,
                              datatype,
                              (MPIDI_msg_sz_t*)&rreq->status.count,
                              &rreq->status.MPI_ERROR);
          mpi_errno = rreq->status.MPI_ERROR;
          MPIU_Free(rreq->mpid.uebuf);
          rreq->mpid.uebuf = NULL;

          if (status != MPI_STATUS_IGNORE)
            *status = rreq->status;
        }

      else
        {
          /* -------------------------------- */
          /* request is incomplete            */
          /* -------------------------------- */
          if(rreq->status.cancelled == FALSE)
            {
              MPID_assert(rreq->mpid.uebuf != NULL);
              MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE);
            }
          if (HANDLE_GET_KIND(datatype) != HANDLE_KIND_BUILTIN)
            {
              MPID_Datatype_get_ptr(datatype, rreq->mpid.datatype_ptr);
              MPID_Datatype_add_ref(rreq->mpid.datatype_ptr);
            }
        }
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
  return mpi_errno;
}
