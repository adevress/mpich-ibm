/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpidi_callback.c
 * \brief The callback for a new eager message
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
void
MPIDI_RecvCB(pami_context_t    context,
             void            * cookie,
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
      MPIDI_Callback_process_unexp(context, msginfo, sndlen, sender, sndbuf, recv, msginfo->flags.isSync);
      MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
      goto fn_exit_eager;
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
  MPIDI_Request_cpyPeerRequest(rreq, msginfo);
  MPIDI_Request_setSync       (rreq, msginfo->flags.isSync);
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

  if (unlikely(msginfo->flags.isSync))
    MPIDI_SyncAck_post(context, rreq, PAMIX_Endpoint_query(sender));

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
      MPIDI_Callback_process_trunc(context, rreq, recv, sndbuf);
      goto fn_exit_eager;
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
      if (sndlen)
        {
          rreq->mpid.uebuf    = MPIU_Malloc(sndlen);
          MPID_assert(rreq->mpid.uebuf != NULL);
        }
      /* -------------------------------------------------- */
      /*  Let PAMI know where to put the rest of the data.  */
      /* -------------------------------------------------- */
      recv->addr = rreq->mpid.uebuf;
    }

 fn_exit_eager:
  /* ---------------------------------------- */
  /*  Signal that the recv has been started.  */
  /* ---------------------------------------- */
  MPIDI_Progress_signal();
}
