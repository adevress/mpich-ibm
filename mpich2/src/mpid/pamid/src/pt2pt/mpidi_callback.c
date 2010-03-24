/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_callback.c
 * \brief The standard callback for a new message
 */
#include "mpidimpl.h"

/**
 * \brief The standard callback for a new message
 * \param[in]  clientdata Unused
 * \param[in]  msginfo    The 16-byte msginfo struct
 * \param[in]  count      The number of msginfo quads (1)
 * \param[in]  senderrank The sender's rank
 * \param[in]  sndlen     The length of the incoming data
 * \param[out] rcvlen     The amount we are willing to receive
 * \param[out] rcvbuf     Where we want to put the data
 */
void MPIDI_RecvCB(pami_context_t   context,
                  void          * _contextid,
                  void          * _msginfo,
                  size_t          msginfo_size,
                  void          * sndbuf,
                  size_t          sndlen,
                  pami_recv_t    * recv)
{
  MPID_assert((sndbuf == NULL) ^ (recv == NULL));

  MPID_assert(_msginfo != NULL);
  MPID_assert(msginfo_size == sizeof(MPIDI_MsgInfo));

  const MPIDI_MsgInfo *msginfo = (const MPIDI_MsgInfo *)_msginfo;
  size_t senderrank = msginfo->msginfo.peerrank;
  /* size_t               contextid = (size_t)_contextid; */

  MPID_Request * rreq = NULL;
  int found;
  unsigned rcvlen = sndlen;

  /* -------------------------- */
  /*      match request         */
  /* -------------------------- */
  MPIDI_Message_match match;
  match.rank              = msginfo->msginfo.MPIrank;
  match.tag               = msginfo->msginfo.MPItag;
  match.context_id        = msginfo->msginfo.MPIctxt;

  rreq = MPIDI_Recvq_FDP_or_AEU(match.rank, match.tag, match.context_id, &found);

  /* -------------------------------------- */
  /* Signal that the recv has been started. */
  /* -------------------------------------- */
  MPIDI_Progress_signal();

  /* ------------------------ */
  /* copy in information      */
  /* ------------------------ */
  rreq->status.MPI_SOURCE = match.rank;
  rreq->status.MPI_TAG    = match.tag;
  MPIDI_Request_setPeerRank   (rreq, senderrank);
  MPIDI_Request_setPeerRequest(rreq, msginfo->msginfo.req);
  MPIDI_Request_setSync       (rreq, msginfo->msginfo.isSync);
  MPIDI_Request_setRzv        (rreq, 0);

  if (recv)
    {
      /* -------------------------------------------------------- */
      /* we have enough information to fill in the callback info. */
      /* -------------------------------------------------------- */
      recv->local_fn = MPIDI_RecvDoneCB;
      recv->cookie   = (void *)rreq;
      recv->kind     = PAMI_AM_KIND_SIMPLE;
    }



  /* ----------------------------------------- */
  /* figure out target buffer for request data */
  /* ----------------------------------------- */
  MPIDI_Request_setCA(rreq, MPIDI_CA_COMPLETE);
  rreq->status.count = rcvlen;
  if (found)
    {
      /* --------------------------- */
      /* request was already posted. */
      /* if synchronized, post ack.  */
      /* --------------------------- */
      if (msginfo->msginfo.isSync)
        MPIDI_postSyncAck(context, rreq);

      /* -------------------------------------- */
      /* calculate message length for reception */
      /* calculate receive message "count"      */
      /* -------------------------------------- */
      unsigned dt_contig, dt_size;
      MPID_Datatype *dt_ptr;
      MPI_Aint dt_true_lb;
      MPIDI_Datatype_get_info(rreq->mpid.userbufcount,
                              rreq->mpid.datatype,
                              dt_contig,
                              dt_size,
                              dt_ptr,
                              dt_true_lb);

      /* -------------------------------------- */
      /* test for truncated message.            */
      /* -------------------------------------- */
      if (rcvlen > dt_size)
        {
          rcvlen = dt_size;
          rreq->status.MPI_ERROR = MPI_ERR_TRUNCATE;
          rreq->status.count = rcvlen;
        }

      /* -------------------------------------- */
      /* if buffer is contiguous, we are done.  */
      /* -------------------------------------- */
      if (dt_contig)
        {

          rreq->mpid.uebuf    = NULL;
          rreq->mpid.uebuflen = 0;
          void* rcvbuf = (char*)rreq->mpid.userbuf + dt_true_lb;

          if (recv)
            {
              recv->data.simple.addr  = rcvbuf;
              recv->data.simple.bytes = rcvlen;
            }
          else
            {
              memcpy(rcvbuf, sndbuf, rcvlen);
              MPIDI_RecvDoneCB(context, rreq, 0);
            }

          return;
        }

      /* --------------------------------------------- */
      /* buffer is non-contiguous. we need to allocate */
      /* a temporary buffer, and unpack later.         */
      /* --------------------------------------------- */
      else
        {
          MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE);

          /* --------------------------------------------- */
          /* buffer is non-contiguous. the data is already */
          /* available, so we can just unpack it now.      */
          /* --------------------------------------------- */
          if (!recv)
            {
              rreq->mpid.uebuflen = rcvlen;
              rreq->mpid.uebuf    = sndbuf ;
              MPIDI_RecvDoneCB(context, rreq, 0);
              return;
            }
        }
    }

  /* ------------------------------------------------------------- */
  /* fallback position: request was not posted or not contiguous   */
  /* We must allocate enough space to hold the message temporarily */
  /* the temporary buffer will be unpacked later.                  */
  /* ------------------------------------------------------------- */
  rreq->mpid.uebuflen = rcvlen;
  rreq->mpid.uebuf    = MPIU_Malloc(rcvlen);
  MPID_assert(rreq->mpid.uebuf != NULL);

  if (recv)
    {
      /* ------------------------------------------------ */
      /*  Let PAMI know where to put the rest of the data  */
      /* ------------------------------------------------ */
      recv->data.simple.addr  = rreq->mpid.uebuf;
      recv->data.simple.bytes = rcvlen;
    }
  else
    {
      /* ------------------------------------------------ */
      /*  We have the data; copy it and complete the msg  */
      /* ------------------------------------------------ */
      memcpy(rreq->mpid.uebuf, sndbuf, rcvlen);
      MPIDI_RecvDoneCB(context, rreq, 0);
    }

  return;
}
