/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpidi_callback_util.c
 * \brief Utility functions to help uncommon cases in the new-message callbacks
 */
#include <mpidimpl.h>
#include "../mpid_recvq.h"


void
MPIDI_Callback_process_unexp(pami_context_t        context,
                             const MPIDI_MsgInfo * msginfo,
                             size_t                sndlen,
                             pami_endpoint_t       sender,
                             const void          * sndbuf,
                             pami_recv_t         * recv,
                             unsigned              isSync)
{
  MPID_Request *rreq = NULL;

  /* ---------------------------------------------------- */
  /*  Fallback position:                                  */
  /*     + Request was not posted, or                     */
  /*     + Request was long & not contiguous.             */
  /*  We must allocate enough space to hold the message.  */
  /*  The temporary buffer will be unpacked later.        */
  /* ---------------------------------------------------- */
  unsigned rank       = msginfo->MPIrank;
  unsigned tag        = msginfo->MPItag;
  unsigned context_id = msginfo->MPIctxt;
  rreq = MPIDI_Recvq_AEU(rank, tag, context_id);
  /* ---------------------- */
  /*  Copy in information.  */
  /* ---------------------- */
  rreq->status.MPI_SOURCE = rank;
  rreq->status.MPI_TAG    = tag;
  rreq->status.count      = sndlen;
  MPIDI_Request_setCA         (rreq, MPIDI_CA_COMPLETE);
  MPIDI_Request_cpyPeerRequest(rreq, msginfo);
  MPIDI_Request_setSync       (rreq, isSync);

  /* Set the rank of the sender if a sync msg. */
  if (isSync)
    MPIDI_Request_setPeerRank(rreq, PAMIX_Endpoint_query(sender));

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
MPIDI_Callback_process_trunc(pami_context_t  context,
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
MPIDI_Callback_process_userdefined_dt(pami_context_t      context,
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
  if (unlikely(sndlen > dt_size))
    {
#if ASSERT_LEVEL > 0
      MPIDI_Callback_process_trunc(context, rreq, NULL, sndbuf);
      return;
#else
      sndlen = dt_size;
#endif
    }

  /*
   * This is to test that the fields don't need to be
   * initialized.  Remove after this doesn't fail for a while.
   */
  if (likely (dt_contig))
    {
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
