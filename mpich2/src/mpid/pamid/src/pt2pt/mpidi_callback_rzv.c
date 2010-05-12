/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_callback_rzv.c
 * \brief The callback for a new RZV RTS
 */
#include "mpidimpl.h"

/**
 * \brief The callback for a new RZV RTS
 * \note  Because this is a short message, the data is already received
 * \param[in]  clientdata   Unused
 * \param[in]  envelope     The 16-byte msginfo struct
 * \param[in]  count        The number of msginfo quads (1)
 * \param[in]  senderrank   The sender's rank
 * \param[in]  sndlen       The length of the incoming data
 * \param[in]  sndbuf       Where the data is stored
 */
void MPIDI_RecvRzvCB(pami_context_t   context,
                     void          * _contextid,
                     void          * _msginfo,
                     size_t          msginfo_size,
                     void          * sndbuf,
                     size_t          sndlen,
                     pami_recv_t   * recv)
{
  MPID_assert(recv == NULL);
  MPID_assert(sndlen == 0);
  MPID_assert(_msginfo != NULL);
  MPID_assert(msginfo_size == sizeof(MPIDI_MsgEnvelope));
  const MPIDI_MsgEnvelope * envelope = (const MPIDI_MsgEnvelope *)_msginfo;
  const MPIDI_MsgInfo * msginfo = (const MPIDI_MsgInfo *)&envelope->envelope.msginfo;
  pami_task_t senderrank = msginfo->msginfo.sender;
  /* size_t               contextid = (size_t)_contextid; */

  MPID_Request * rreq = NULL;
  int found;


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
  MPIDI_Request_cpyPeerRequest(rreq, msginfo);
  MPIDI_Request_setSync       (rreq, msginfo->msginfo.isSync);
  MPIDI_Request_setRzv        (rreq, 1);

  /* ----------------------------------------------------- */
  /* Save the rendezvous information for when the target   */
  /* node calls a receive function and the data is         */
  /* retreived from the origin node.                       */
  /* ----------------------------------------------------- */
  rreq->status.count                  = envelope->envelope.length;
#ifdef USE_PAMI_RDMA
  memcpy(&rreq->mpid.envelope.envelope.memregion,
	 &envelope->envelope.memregion,
	 sizeof(pami_memregion_t));
#else
  rreq->mpid.envelope.envelope.data   = envelope->envelope.data;
#endif
  rreq->mpid.envelope.envelope.length = envelope->envelope.length;

  /* ----------------------------------------- */
  /* figure out target buffer for request data */
  /* ----------------------------------------- */
  if (found)
    {
      /* --------------------------- */
      /* if synchronized, post ack.  */
      /* --------------------------- */
      if (unlikely(MPIDI_Request_isSync(rreq)))
        MPIDI_postSyncAck(context, rreq);

      MPIDI_RendezvousTransfer(context, rreq);
    }

  /* ------------------------------------------------------------- */
  /* Request was not posted. */
  /* ------------------------------------------------------------- */
  else
    {
      rreq->mpid.uebuf = NULL;
      rreq->mpid.uebuflen = 0;
    }
}
