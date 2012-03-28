/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpidi_callback_short.c
 * \brief The callback for a new short message
 */
#include <mpidimpl.h>


/**
 * \brief The standard callback for a new message
 *
 * \param[in]  context      The context on which the message is being received.
 * \param[in]  cookie       Unused
 * \param[in]  _msginfo     The header information
 * \param[in]  msginfo_size The size of the header information
 * \param[in]  sndbuf       If the message is short, this is the data
 * \param[in]  sndlen       The size of the incoming data
 * \param[out] recv         If the message is long, this tells the message layer how to handle the data.
 */
static inline void
MPIDI_RecvShortCB(pami_context_t    context,
                  const void      * _msginfo,
                  const void      * sndbuf,
                  size_t            sndlen,
                  pami_endpoint_t   sender,
                  unsigned          isSync)
  __attribute__((__always_inline__));
static inline void
MPIDI_RecvShortCB(pami_context_t    context,
                  const void      * _msginfo,
                  const void      * sndbuf,
                  size_t            sndlen,
                  pami_endpoint_t   sender,
                  unsigned          isSync)
{
  MPID_assert(_msginfo != NULL);

  const MPIDI_MsgInfo *msginfo = (const MPIDI_MsgInfo *)_msginfo;
  MPID_Request * rreq = NULL;

  /* -------------------- */
  /*  Match the request.  */
  /* -------------------- */
  unsigned rank       = msginfo->MPIrank;
  unsigned tag        = msginfo->MPItag;
  unsigned context_id = msginfo->MPIctxt;

  MPIU_THREAD_CS_ENTER(MSGQUEUE,0);
#ifndef OUT_OF_ORDER_HANDLING
  rreq = MPIDI_Recvq_FDP(rank, tag, context_id);
#else
  pami_task_t source = PAMIX_Endpoint_query(sender);
  rreq = MPIDI_Recvq_FDP(rank, source, tag, context_id, msginfo->MPIseqno);
#endif

  /* Match not found */
  if (unlikely(rreq == NULL))
    {
      void *uebuf = NULL;
      if (sndlen)
      {
        MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
        uebuf = MPIU_Malloc(sndlen);
        MPID_assert(uebuf != NULL);
        MPIU_THREAD_CS_ENTER(MSGQUEUE,0);
#ifndef OUT_OF_ORDER_HANDLING
        rreq = MPIDI_Recvq_FDP(rank, tag, context_id);
#else
        rreq = MPIDI_Recvq_FDP(rank, PAMIX_Endpoint_query(sender), tag, context_id, msginfo->MPIseqno);
#endif
      }
      
      if (unlikely(rreq == NULL))
      {
        MPIDI_Callback_process_unexp(context, msginfo, sndlen, sender, sndbuf, NULL, isSync, uebuf);
        MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
        goto fn_exit_short;
      }
      else
      {       
        MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
        if (sndlen) MPIU_Free(uebuf);
      }         
    }
  else
    {
      MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
    }

  /* the receive queue processing has been completed and we found match*/

  /* ---------------------- */
  /*  Copy in information.  */
  /* ---------------------- */
  rreq->status.MPI_SOURCE = rank;
  rreq->status.MPI_TAG    = tag;
  rreq->status.count      = sndlen;
  MPIDI_Request_setCA          (rreq, MPIDI_CA_COMPLETE);
  MPIDI_Request_cpyPeerRequestH(rreq, msginfo);
  MPIDI_Request_setSync        (rreq, isSync);
  MPIDI_Request_setRzv         (rreq, 0);

  /* ----------------------------- */
  /*  Request was already posted.  */
  /* ----------------------------- */
  if (unlikely(isSync))
    MPIDI_SyncAck_post(context, rreq, PAMIX_Endpoint_query(sender));

  if (unlikely(HANDLE_GET_KIND(rreq->mpid.datatype) != HANDLE_KIND_BUILTIN))
    {
      MPIDI_Callback_process_userdefined_dt(context, sndbuf, sndlen, rreq);
      goto fn_exit_short;
    }

  size_t dt_size = rreq->mpid.userbufcount * MPID_Datatype_get_basic_size(rreq->mpid.datatype);

  /* ----------------------------- */
  /*  Test for truncated message.  */
  /* ----------------------------- */
  if (unlikely(sndlen > dt_size))
    {
#if ASSERT_LEVEL > 0
      MPIDI_Callback_process_trunc(context, rreq, NULL, sndbuf);
      goto fn_exit_short;
#else
      sndlen = dt_size;
#endif
    }

  MPID_assert(rreq->mpid.uebuf    == NULL);
  MPID_assert(rreq->mpid.uebuflen == 0);
  void* rcvbuf = rreq->mpid.userbuf;

  if (sndlen > 0)
    memcpy(rcvbuf, sndbuf, sndlen);
  MPIDI_Request_complete(rreq);

#ifdef OUT_OF_ORDER_HANDLING
  MPIU_THREAD_CS_ENTER(MSGQUEUE,0);
  if ((rank != MPI_ANY_SOURCE) && (MPIDI_In_cntr[source].n_OutOfOrderMsgs>0))  {
    MPIDI_Recvq_process_out_of_order_msgs(source, context);
  }
  MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
#endif

 fn_exit_short:
  /* ---------------------------------------- */
  /*  Signal that the recv has been started.  */
  /* ---------------------------------------- */
  MPIDI_Progress_signal();
}


void
MPIDI_RecvShortAsyncCB(pami_context_t    context,
                       void            * cookie,
                       const void      * _msginfo,
                       size_t            msginfo_size,
                       const void      * sndbuf,
                       size_t            sndlen,
                       pami_endpoint_t   sender,
                       pami_recv_t     * recv)
{
  MPID_assert(recv == NULL);
  MPID_assert(msginfo_size == sizeof(MPIDI_MsgInfo));
  MPIDI_RecvShortCB(context,
                    _msginfo,
                    sndbuf,
                    sndlen,
                    sender,
                    0);
}


void
MPIDI_RecvShortSyncCB(pami_context_t    context,
                      void            * cookie,
                      const void      * _msginfo,
                      size_t            msginfo_size,
                      const void      * sndbuf,
                      size_t            sndlen,
                      pami_endpoint_t   sender,
                      pami_recv_t     * recv)
{
  MPID_assert(recv == NULL);
  MPID_assert(msginfo_size == sizeof(MPIDI_MsgInfo));
  MPIDI_RecvShortCB(context,
                    _msginfo,
                    sndbuf,
                    sndlen,
                    sender,
                    1);
}
