/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_recvmsg.c
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
MPIDI_RecvMsg(void          * buf,
              int             count,
              MPI_Datatype    datatype,
              int             rank,
              int             tag,
              MPID_Comm     * comm,
              int             context_offset,
              MPI_Status    * status,
              MPID_Request ** request)
{
  int found;
  MPID_Request * rreq;

  MPIU_THREAD_CS_ENTER(RECVQ,0);
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
      if (unlikely(MPIDI_Request_isSync(rreq)))
        {
          MPID_assert(!MPIDI_Request_isSelf(rreq));
          MPIDI_SyncAck_post(MPIDI_Context_local(rreq), rreq);
        }

      if (MPIDI_Request_isSelf(rreq))
        {
          /* ---------------------- */
          /* "SELF" request         */
          /* ---------------------- */
          MPID_Request * const sreq = rreq->partner_request;
          MPID_assert(sreq != NULL);
          MPIDI_msg_sz_t _count=0;
          MPIDI_Buffer_copy(sreq->mpid.userbuf,
                            sreq->mpid.userbufcount,
                            sreq->mpid.datatype,
                            &sreq->status.MPI_ERROR,
                            buf,
                            count,
                            datatype,
                            &_count,
                            &rreq->status.MPI_ERROR);
          rreq->status.count = _count;
          MPIDI_Request_complete(sreq);
          /* no other thread can possibly be waiting on rreq,
             so it is safe to reset ref_count and cc */
          MPID_cc_set(&rreq->cc, 0);
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

          MPIDI_RendezvousTransfer(MPIDI_Context_local(rreq), rreq);
        }
      else if (MPID_cc_is_complete(&rreq->cc))
        {
          /* -------------------------------- */
          /* request is complete              */
          /* -------------------------------- */
          if (rreq->mpid.uebuf != NULL)
            {
              if (likely(rreq->status.cancelled == FALSE))
                {
                  MPIDI_msg_sz_t _count=0;
                  MPIDI_Buffer_copy(rreq->mpid.uebuf,
                                    rreq->mpid.uebuflen,
                                    MPI_CHAR,
                                    &rreq->status.MPI_ERROR,
                                    buf,
                                    count,
                                    datatype,
                                    &_count,
                                    &rreq->status.MPI_ERROR);
                  rreq->status.count = _count;
                }
              MPIU_Free(rreq->mpid.uebuf);
              rreq->mpid.uebuf = NULL;
            }
          else
            {
              MPID_assert(rreq->mpid.uebuflen == 0);
              rreq->status.count = 0;
            }
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
  if (status != MPI_STATUS_IGNORE)
    *status = rreq->status;

  MPIU_THREAD_CS_EXIT(RECVQ,0);
  return rreq->status.MPI_ERROR;
}
