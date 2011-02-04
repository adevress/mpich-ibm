/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpidi_callback_short.c
 * \brief The callback for a new short message
 */
#include <mpidimpl.h>
#include "../mpid_recvq.h"


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
  rreq = MPIDI_Recvq_FDP(rank, tag, context_id);

  //Match not found
  if (unlikely(rreq == NULL))
    {
      MPIDI_Callback_process_unexp(context, msginfo, sndlen, sender, sndbuf, NULL, isSync);
      MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
      goto fn_exit_short;
    }

  /* the receive queue processing has been completed and we found match*/
  MPIU_THREAD_CS_EXIT(MSGQUEUE,0);

  /* ---------------------- */
  /*  Copy in information.  */
  /* ---------------------- */
  rreq->status.MPI_SOURCE = rank;
  rreq->status.MPI_TAG    = tag;
  rreq->status.count      = sndlen;
  MPIDI_Request_setCA         (rreq, MPIDI_CA_COMPLETE);
  MPIDI_Request_cpyPeerRequest(rreq, msginfo);
  MPIDI_Request_setSync       (rreq, isSync);
  MPIDI_Request_setRzv        (rreq, 0);

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
