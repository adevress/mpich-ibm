/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpidi_callback_rzv.c
 * \brief The callback for a new RZV RTS
 */
#include <mpidimpl.h>


/**
 * \brief The callback for a new RZV RTS
 * \note  Because this is a short message, the data is already received
 * \param[in]  context      The context on which the message is being received.
 * \param[in]  cookie       Unused
 * \param[in]  _msginfo     The extended header information
 * \param[in]  msginfo_size The size of the extended header information
 * \param[in]  sndbuf       Unused
 * \param[in]  sndlen       Unused
 * \param[out] recv         Unused
 */
void
MPIDI_RecvRzvCB(pami_context_t    context,
                void            * cookie,
                const void      * _msginfo,
                size_t            msginfo_size,
                const void      * sndbuf,
                size_t            sndlen,
                pami_endpoint_t   sender,
                pami_recv_t     * recv)
{
  MPID_assert(recv == NULL);
  MPID_assert(sndlen == 0);
  MPID_assert(_msginfo != NULL);
  MPID_assert(msginfo_size == sizeof(MPIDI_MsgEnvelope));
  const MPIDI_MsgEnvelope * envelope = (const MPIDI_MsgEnvelope *)_msginfo;
  const MPIDI_MsgInfo * msginfo = (const MPIDI_MsgInfo *)&envelope->msginfo;

  MPID_Request * rreq = NULL;
  int found;

  /* -------------------- */
  /*  Match the request.  */
  /* -------------------- */
  unsigned rank       = msginfo->MPIrank;
  unsigned tag        = msginfo->MPItag;
  unsigned context_id = msginfo->MPIctxt;

  MPIU_THREAD_CS_ENTER(MSGQUEUE,0);
#ifndef OUT_OF_ORDER_HANDLING
  rreq = MPIDI_Recvq_FDP_or_AEU(rank, tag, context_id, &found);
#else
  pami_task_t source = PAMIX_Endpoint_query(sender);
  rreq = MPIDI_Recvq_FDP_or_AEU(rank, source, tag, context_id, msginfo->MPIseqno, &found);
#endif
  TRACE_ERR("RZV CB for req=%p remote-mr=0x%llx bytes=%zu (%sfound)\n",
            rreq,
            *(unsigned long long*)&envelope->envelope.memregion,
            envelope->envelope.length,
            found?"":"not ");

  /* ---------------------- */
  /*  Copy in information.  */
  /* ---------------------- */
  rreq->status.MPI_SOURCE = rank;
  rreq->status.MPI_TAG    = tag;
  rreq->status.count      = envelope->length;
  MPIDI_Request_setPeerRank_comm(rreq, rank);
  MPIDI_Request_setPeerRank_pami(rreq, PAMIX_Endpoint_query(sender));
  MPIDI_Request_cpyPeerRequestH (rreq, msginfo);
  MPIDI_Request_setSync         (rreq, msginfo->isSync);
  MPIDI_Request_setRzv          (rreq, 1);

  /* ----------------------------------------------------- */
  /* Save the rendezvous information for when the target   */
  /* node calls a receive function and the data is         */
  /* retreived from the origin node.                       */
  /* ----------------------------------------------------- */
#ifdef USE_PAMI_RDMA
  memcpy(&rreq->mpid.envelope.memregion,
	 &envelope->memregion,
	 sizeof(pami_memregion_t));
#else
  rreq->mpid.envelope.memregion_used = envelope->memregion_used;
  if(envelope->memregion_used) {
    memcpy(&rreq->mpid.envelope.memregion,
           &envelope->memregion,
           sizeof(pami_memregion_t));
  }
  rreq->mpid.envelope.data   = envelope->data;
#endif
  rreq->mpid.envelope.length = envelope->length;

  /* ----------------------------------------- */
  /* figure out target buffer for request data */
  /* ----------------------------------------- */
  if (found)
    {
      /* --------------------------- */
      /* if synchronized, post ack.  */
      /* --------------------------- */
      if (unlikely(MPIDI_Request_isSync(rreq)))
        MPIDI_SyncAck_post(context, rreq, MPIDI_Request_getPeerRank_pami(rreq));

      MPIDI_RendezvousTransfer(context, rreq);
    }

  /* ------------------------------------------------------------- */
  /* Request was not posted. */
  /* ------------------------------------------------------------- */
  else
    {
      /*
       * This is to test that the fields don't need to be
       * initialized.  Remove after this doesn't fail for a while.
       */
      MPID_assert(rreq->mpid.uebuf    == NULL);
      MPID_assert(rreq->mpid.uebuflen == 0);
      /* rreq->mpid.uebuf = NULL; */
      /* rreq->mpid.uebuflen = 0; */
    }

  /* ---------------------------------------- */
  /*  Signal that the recv has been started.  */
  /* ---------------------------------------- */
  MPIDI_Progress_signal();

  MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
}
