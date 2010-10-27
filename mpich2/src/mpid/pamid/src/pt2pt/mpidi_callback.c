/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_callback.c
 * \brief The standard callback for a new message
 */
#include "mpidimpl.h"

/* This is used to effectively zero-out the recv hints in the done callback */
static const pami_recv_hint_t null_recv_hint={0};

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
  MPID_assert((sndbuf == NULL) ^ (recv == NULL));

  MPID_assert(_msginfo != NULL);
  MPID_assert(msginfo_size == sizeof(MPIDI_MsgInfo));

  const MPIDI_MsgInfo *msginfo = (const MPIDI_MsgInfo *)_msginfo;
  pami_task_t senderrank;
  size_t      sendercontext;
  PAMI_Endpoint_query(sender, &senderrank, &sendercontext);
  /* size_t               contextid = (size_t)_contextid; */

  MPID_Request * rreq = NULL;
  int found;

  /* -------------------- */
  /*  Match the request.  */
  /* -------------------- */
  MPIDI_Message_match match;
  match.rank       = msginfo->MPIrank;
  match.tag        = msginfo->MPItag;
  match.context_id = msginfo->MPIctxt;

  MPIU_THREAD_CS_ENTER(MSGQUEUE,0);
  rreq = MPIDI_Recvq_FDP_or_AEU(match.rank, match.tag, match.context_id, &found);

  /* ---------------------------------------- */
  /*  Signal that the recv has been started.  */
  /* ---------------------------------------- */
  MPIDI_Progress_signal();

  /* ---------------------- */
  /*  Copy in information.  */
  /* ---------------------- */
  rreq->status.MPI_SOURCE = match.rank;
  rreq->status.MPI_TAG    = match.tag;
  rreq->status.count      = sndlen;
  MPIDI_Request_setCA         (rreq, MPIDI_CA_COMPLETE);
  MPIDI_Request_setPeerRank   (rreq, senderrank);
  MPIDI_Request_cpyPeerRequest(rreq, msginfo);
  MPIDI_Request_setSync       (rreq, msginfo->isSync);
  MPIDI_Request_setRzv        (rreq, 0);

  /* --------------------------------------- */
  /*  We have to fill in the callback info.  */
  /* --------------------------------------- */
  if (unlikely(recv != NULL))
    {
      recv->hints    = null_recv_hint;
      recv->local_fn = MPIDI_RecvDoneCB;
      recv->cookie   = rreq;
    }


  /* -------------------------------------------- */
  /*  Figure out target buffer for request data.  */
  /* -------------------------------------------- */
  if (found)
    {
      /* ----------------------------- */
      /*  Request was already posted.  */
      /* ----------------------------- */

      if (unlikely(msginfo->isSync))
        MPIDI_SyncAck_post(context, rreq);

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
          rreq->status.MPI_ERROR = MPI_ERR_TRUNCATE;

          /* -------------------------------------------------------------- */
          /*  The data is already available, so we can just unpack it now.  */
          /* -------------------------------------------------------------- */
          if (recv)
            {
              MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE);
              rreq->mpid.uebuflen = sndlen;
              rreq->mpid.uebuf    = MPIU_Malloc(sndlen);
              MPID_assert(rreq->mpid.uebuf != NULL);

              recv->data_fn = PAMI_DATA_COPY;
              recv->type = PAMI_BYTE;
              recv->addr = rreq->mpid.uebuf;
            }
          else
            {
              MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE_NOFREE);
              rreq->mpid.uebuflen = sndlen;
              rreq->mpid.uebuf    = (void*)sndbuf;
              MPIDI_RecvDoneCB(context, rreq, PAMI_SUCCESS);
            }
	  MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
          return;
        }

      /* --------------------------------------- */
      /*  If buffer is contiguous, we are done.  */
      /* --------------------------------------- */
      if (likely(dt_contig))
        {
	  MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
          /*
           * This is to test that the fields don't need to be
           * initialized.  Remove after this doesn't fail for a while.
           */
          MPID_assert(rreq->mpid.uebuf    == NULL);
          MPID_assert(rreq->mpid.uebuflen == 0);
          /* rreq->mpid.uebuf    = NULL; */
          /* rreq->mpid.uebuflen = 0; */
          void* rcvbuf = rreq->mpid.userbuf + dt_true_lb;

          if (unlikely(recv != NULL))
            {
              recv->data_fn = PAMI_DATA_COPY;
              recv->type = PAMI_BYTE;
              recv->addr = rcvbuf;
            }
          else
            {
              memcpy(rcvbuf, sndbuf, sndlen);
              MPIDI_RecvDoneCB(context, rreq, PAMI_SUCCESS);
            }
          return;
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
          if (unlikely(recv != NULL))
            {
              MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE);
            }
          else
            {
              MPIDI_Request_setCA(rreq, MPIDI_CA_UNPACK_UEBUF_AND_COMPLETE_NOFREE);
              rreq->mpid.uebuflen = sndlen;
              rreq->mpid.uebuf    = (void*)sndbuf;
              MPIDI_RecvDoneCB(context, rreq, PAMI_SUCCESS);
	      MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
              return;
            }
        }
    }

  /* ---------------------------------------------------- */
  /*  Fallback position:                                  */
  /*     + Request was not posted, or                     */
  /*     + Request was long & not contiguous.             */
  /*  We must allocate enough space to hold the message.  */
  /*  The temporary buffer will be unpacked later.        */
  /* ---------------------------------------------------- */
  rreq->mpid.uebuflen = sndlen;
  if (sndlen)
    {
      rreq->mpid.uebuf    = MPIU_Malloc(sndlen);
      MPID_assert(rreq->mpid.uebuf != NULL);
    }

  if (unlikely(recv != NULL))
    {
      /* -------------------------------------------------- */
      /*  Let PAMI know where to put the rest of the data.  */
      /* -------------------------------------------------- */
      recv->data_fn = PAMI_DATA_COPY;
      recv->type = PAMI_BYTE;
      recv->addr = rreq->mpid.uebuf;
    }
  else
    {
      /* ------------------------------------------------- */
      /*  We have the data; copy it and complete the msg.  */
      /* ------------------------------------------------- */
      memcpy(rreq->mpid.uebuf, sndbuf, sndlen);
      MPIDI_RecvDoneCB(context, rreq, PAMI_SUCCESS);
    }

  MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
}
