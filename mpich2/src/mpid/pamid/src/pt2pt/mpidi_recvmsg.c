/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpidi_recvmsg.c
 * \brief ADI level implemenation of common recv code.
 */
#include <mpidimpl.h>
#include "../mpid_recvq.h"

void
MPIDI_RecvMsg_Unexp(MPID_Request  * rreq,
                    void          * buf,
                    int             count,
                    MPI_Datatype    datatype)
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
      MPIDI_SyncAck_post(MPIDI_Context_local(rreq), rreq, MPIDI_Request_getPeerRank(rreq));

  if (MPIDI_Request_isRzv(rreq))
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
