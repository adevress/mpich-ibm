/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_callback.c
 * \brief The standard callback for a new message
 */
#include <mpidimpl.h>
#include "../mpid_recvq.h"

void
MPIDI_Recv_process_unexp(pami_context_t        context,
                         const MPIDI_MsgInfo * msginfo,
                         size_t                sndlen,
                         pami_endpoint_t       senderendpoint,
                         const void          * sndbuf,
                         pami_recv_t         * recv)
  __attribute__((__noinline__));
void
MPIDI_Recv_process_unexp(pami_context_t        context,
                         const MPIDI_MsgInfo * msginfo,
                         size_t                sndlen,
                         pami_endpoint_t       senderendpoint,
                         const void          * sndbuf,
                         pami_recv_t         * recv)
{
  pami_task_t senderrank = (unsigned)-1;
  size_t      sendercontext;
  MPID_Request *rreq = NULL;

  //Set the rank of the sender if a sync msg.
  if (msginfo->isSync) {
    PAMI_Endpoint_query(senderendpoint, &senderrank, &sendercontext);
    MPIDI_Request_setPeerRank   (rreq, senderrank);
  }

  /* ---------------------------------------------------- */
  /*  Fallback position:                                  */
  /*     + Request was not posted, or                     */
  /*     + Request was long & not contiguous.             */
  /*  We must allocate enough space to hold the message.  */
  /*  The temporary buffer will be unpacked later.        */
  /* ---------------------------------------------------- */
  rreq = MPIDI_Recvq_AEU(sndlen, senderrank, msginfo);

  rreq->mpid.uebuflen = sndlen;
  if (sndlen)
    {
      rreq->mpid.uebuf    = MPIU_Malloc(sndlen);
      MPID_assert(rreq->mpid.uebuf != NULL);
    }

  if (recv != NULL)
    {
      recv->local_fn = MPIDI_RecvDoneCB;
      recv->cookie   = rreq;
      /** \todo Remove these two lines when trac #256 is approved, implemented, and integrated into MPICH2 */
      recv->data_fn  = PAMI_DATA_COPY;
      recv->type     = PAMI_TYPE_CONTIGUOUS;
      /* -------------------------------------------------- */
      /*  Let PAMI know where to put the rest of the data.  */
      /* -------------------------------------------------- */
      recv->addr = rreq->mpid.uebuf;
    }
  else
    {
      /* ------------------------------------------------- */
      /*  We have the data; copy it and complete the msg.  */
      /* ------------------------------------------------- */
      memcpy(rreq->mpid.uebuf, sndbuf,   sndlen);
      MPIDI_RecvDoneCB(context, rreq, PAMI_SUCCESS);
    }
}


void
MPIDI_Recv_process_trunc(pami_context_t  context,
                         MPID_Request   *rreq,
                         pami_recv_t    *recv,
                         const void     *sndbuf)
  __attribute__((__noinline__));
void
MPIDI_Recv_process_trunc(pami_context_t  context,
                         MPID_Request   *rreq,
                         pami_recv_t    *recv,
                         const void     *sndbuf)
{
  rreq->status.MPI_ERROR = MPI_ERR_TRUNCATE;

  /* -------------------------------------------------------------- */
  /*  The data is already available, so we can just unpack it now.  */
  /* -------------------------------------------------------------- */
  if (recv)
    {
      MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE);
      rreq->mpid.uebuflen = rreq->status.count;
      rreq->mpid.uebuf    = MPIU_Malloc(rreq->status.count);
      MPID_assert(rreq->mpid.uebuf != NULL);

      recv->addr = rreq->mpid.uebuf;
    }
  else
    {
      MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE_NOFREE);
      rreq->mpid.uebuflen = rreq->status.count;
      rreq->mpid.uebuf    = (void*)sndbuf;
      MPIDI_RecvDoneCB(context, rreq, PAMI_SUCCESS);
    }
}

void
MPIDI_Recv_process_userdefined_dt(pami_context_t      context,
                                  const void        * sndbuf,
                                  size_t              sndlen,
                                  MPID_Request      * rreq)
  __attribute__((__noinline__));

void
MPIDI_Recv_process_userdefined_dt(pami_context_t      context,
                                  const void        * sndbuf,
                                  size_t              sndlen,
                                  MPID_Request      * rreq)
{
  unsigned dt_contig, dt_size;
  MPID_Datatype *dt_ptr;
  MPI_Aint dt_true_lb;
  MPIDI_Datatype_get_info(rreq->mpid.userbufcount,
                          rreq->mpid.datatype,
                          dt_contig,
                          dt_size,
                          dt_ptr,
                          dt_true_lb);

  /* ----------------------------- */
  /*  Test for truncated message.  */
  /* ----------------------------- */
  if (unlikely(sndlen > dt_size)) {
#if ASSERT_LEVEL > 0
    MPIDI_Recv_process_trunc(context, rreq, NULL, sndbuf);
    return;
#else
    sndlen = dt_size;
#endif
  }

  /*
   * This is to test that the fields don't need to be
   * initialized.  Remove after this doesn't fail for a while.
   */
  if (likely (dt_contig)) {
    MPID_assert(rreq->mpid.uebuf    == NULL);
    MPID_assert(rreq->mpid.uebuflen == 0);
    void* rcvbuf = rreq->mpid.userbuf +  dt_true_lb;;

    memcpy(rcvbuf, sndbuf, sndlen);
    MPIDI_Request_complete(rreq);
    return;
  }

  MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE_NOFREE);
  rreq->mpid.uebuflen = sndlen;
  rreq->mpid.uebuf    = (void*)sndbuf;
  MPIDI_RecvDoneCB(context, rreq, PAMI_SUCCESS);
}


/**
 * \brief The standard callback for a new message
 *
 * \param[in]   context     The context on which the message is being received.
 * \param[in]  _contextid   The numerical index of the context
 * \param[in]  _msginfo     The header information
 * \param[in]  msginfo_size The size of the header information
 * \param[in]  sndbuf       If the message is short, this is the data
 * \param[in]  sndlen       The size of the incoming data
 * \param[out] recv         If the message is long, this tells the message layer how to handle the data.
 */
void MPIDI_RecvShortCB(pami_context_t    context,
                       void            * _contextid,
                       const void      * _msginfo,
                       size_t            msginfo_size,
                       const void      * sndbuf,
                       size_t            sndlen,
                       pami_endpoint_t   sender,
                       pami_recv_t     * recv)
{
  MPID_assert(recv == NULL);
  MPID_assert(_msginfo != NULL);
  MPID_assert(msginfo_size == sizeof(MPIDI_MsgInfo));

  const MPIDI_MsgInfo *msginfo = (const MPIDI_MsgInfo *)_msginfo;
  pami_task_t senderrank = (unsigned) -1;
  size_t      sendercontext;

  MPID_Request * rreq = NULL;

  /* -------------------- */
  /*  Match the request.  */
  /* -------------------- */
  unsigned rank       = msginfo->MPIrank;
  unsigned tag        = msginfo->MPItag;
  unsigned context_id = msginfo->MPIctxt;

  MPIU_THREAD_CS_ENTER(MSGQUEUE,0);
  /* ---------------------------------------- */
  /*  Signal that the recv has been started.  */
  /* ---------------------------------------- */
  MPIDI_Progress_signal();

  rreq = MPIDI_Recvq_FDP(rank, tag, context_id);

  //Match not found
  if (unlikely(rreq == NULL)) {
    MPIDI_Recv_process_unexp(context, msginfo, sndlen, sender, sndbuf, NULL);
    MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
    return;
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
  MPIDI_Request_setPeerRank   (rreq, senderrank);
  MPIDI_Request_cpyPeerRequest(rreq, msginfo);
  MPIDI_Request_setSync       (rreq, msginfo->isSync);
  MPIDI_Request_setRzv        (rreq, 0);

  /* ----------------------------- */
  /*  Request was already posted.  */
  /* ----------------------------- */
  if (unlikely(msginfo->isSync)) {
    PAMI_Endpoint_query(sender, &senderrank, &sendercontext);
    MPIDI_Request_setPeerRank   (rreq, senderrank);
    MPIDI_SyncAck_post(context, rreq);
  }

  if (unlikely(HANDLE_GET_KIND(rreq->mpid.datatype) != HANDLE_KIND_BUILTIN)) {
    MPIDI_Recv_process_userdefined_dt(context, sndbuf, sndlen, rreq);
    return;
  }

  unsigned dt_size = rreq->mpid.userbufcount * MPID_Datatype_get_basic_size(rreq->mpid.datatype);

  /* ----------------------------- */
  /*  Test for truncated message.  */
  /* ----------------------------- */
  if (unlikely(sndlen > dt_size)) {
#if ASSERT_LEVEL > 0
    MPIDI_Recv_process_trunc(context, rreq, NULL, sndbuf);
    return;
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
}


/**
 * \brief The standard callback for a new message
 *
 * \param[in]   context     The context on which the message is being received.
 * \param[in]  _contextid   The numerical index of the context
 * \param[in]  _msginfo     The header information
 * \param[in]  msginfo_size The size of the header information
 * \param[in]  sndbuf       If the message is short, this is the data
 * \param[in]  sndlen       The size of the incoming data
 * \param[out] recv         If the message is long, this tells the message layer how to handle the data.
 */
void MPIDI_RecvCB(pami_context_t    context,
                  void            * _contextid,
                  const void      * _msginfo,
                  size_t            msginfo_size,
                  const void      * sndbuf,
                  size_t            sndlen,
                  pami_endpoint_t   sender,
                  pami_recv_t     * recv)
{
  MPID_assert(sndbuf == NULL);
  MPID_assert(recv != NULL);
  MPID_assert(_msginfo != NULL);
  MPID_assert(msginfo_size == sizeof(MPIDI_MsgInfo));

  const MPIDI_MsgInfo *msginfo = (const MPIDI_MsgInfo *)_msginfo;
  pami_task_t senderrank = (unsigned)-1;
  size_t      sendercontext;

  MPID_Request * rreq = NULL;

  /* -------------------- */
  /*  Match the request.  */
  /* -------------------- */
  unsigned rank       = msginfo->MPIrank;
  unsigned tag        = msginfo->MPItag;
  unsigned context_id = msginfo->MPIctxt;

  MPIU_THREAD_CS_ENTER(MSGQUEUE,0);
  /* ---------------------------------------- */
  /*  Signal that the recv has been started.  */
  /* ---------------------------------------- */
  MPIDI_Progress_signal();

  rreq = MPIDI_Recvq_FDP(rank, tag, context_id);

  //Match not found
  if (unlikely(rreq == NULL)) {
    MPIDI_Recv_process_unexp(context, msginfo, sndlen, sender, sndbuf, recv);
    MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
    return;
  }

  /* -------------------------------------------- */
  /*  Figure out target buffer for request data.  */
  /* -------------------------------------------- */
  MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
  /* ---------------------- */
  /*  Copy in information.  */
  /* ---------------------- */
  rreq->status.MPI_SOURCE = rank;
  rreq->status.MPI_TAG    = tag;
  rreq->status.count      = sndlen;
  MPIDI_Request_setCA         (rreq, MPIDI_CA_COMPLETE);
  MPIDI_Request_setPeerRank   (rreq, senderrank);
  MPIDI_Request_cpyPeerRequest(rreq, msginfo);
  MPIDI_Request_setSync       (rreq, msginfo->isSync);
  MPIDI_Request_setRzv        (rreq, 0);

  /* --------------------------------------- */
  /*  We have to fill in the callback info.  */
  /* --------------------------------------- */
  recv->local_fn = MPIDI_RecvDoneCB;
  recv->cookie   = rreq;
#if ASSERT_LEVEL > 0
  /* This ensures that the value is set, even to something impossible */
  recv->addr = NULL;
#endif
  /** \todo Remove these two lines when trac #256 is approved, implemented, and integrated into MPICH2 */
  recv->data_fn  = PAMI_DATA_COPY;
  recv->type     = PAMI_TYPE_CONTIGUOUS;

  /* ----------------------------- */
  /*  Request was already posted.  */
  /* ----------------------------- */

  if (unlikely(msginfo->isSync)) {
    PAMI_Endpoint_query(sender, &senderrank, &sendercontext);
    MPIDI_Request_setPeerRank   (rreq, senderrank);
    MPIDI_SyncAck_post(context, rreq);
  }

  /* ----------------------------------------- */
  /*  Calculate message length for reception.  */
  /* ----------------------------------------- */
  unsigned dt_contig, dt_size;
  MPID_Datatype *dt_ptr;
  MPI_Aint dt_true_lb;
  MPIDI_Datatype_get_info(rreq->mpid.userbufcount,
                          rreq->mpid.datatype,
                          dt_contig,
                          dt_size,
                          dt_ptr,
                          dt_true_lb);

  /* ----------------------------- */
  /*  Test for truncated message.  */
  /* ----------------------------- */
  if (unlikely(sndlen > dt_size))
    {
      MPIDI_Recv_process_trunc(context, rreq, recv, sndbuf);
      return;
    }

  /* --------------------------------------- */
  /*  If buffer is contiguous, we are done.  */
  /* --------------------------------------- */
  if (likely(dt_contig))
    {
      /*
       * This is to test that the fields don't need to be
       * initialized.  Remove after this doesn't fail for a while.
       */
      MPID_assert(rreq->mpid.uebuf    == NULL);
      MPID_assert(rreq->mpid.uebuflen == 0);
      /* rreq->mpid.uebuf    = NULL; */
      /* rreq->mpid.uebuflen = 0; */
      void* rcvbuf = rreq->mpid.userbuf + dt_true_lb;

      recv->addr = rcvbuf;
    }

  /* ----------------------------------------------- */
  /*  Buffer is non-contiguous. we need to allocate  */
  /*  a temporary buffer, and unpack later.          */
  /* ----------------------------------------------- */
  else
    {
      /* ----------------------------------------------- */
      /*  Buffer is non-contiguous. the data is already  */
      /*  available, so we can just unpack it now.       */
      /* ----------------------------------------------- */
      MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE);
      rreq->mpid.uebuflen = sndlen;
      if (sndlen) {
        rreq->mpid.uebuf    = MPIU_Malloc(sndlen);
        MPID_assert(rreq->mpid.uebuf != NULL);
      }
      /* -------------------------------------------------- */
      /*  Let PAMI know where to put the rest of the data.  */
      /* -------------------------------------------------- */
      recv->addr = rreq->mpid.uebuf;
    }
}
