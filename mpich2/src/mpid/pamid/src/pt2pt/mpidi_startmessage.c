/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_startmessage.c
 * \brief Funnel point for starting all MPI messages
 */
#include "mpidimpl.h"

/* ----------------------------------------------------------------------- */
/*          Helper function: gets the message underway once the request    */
/* and the buffers have been allocated.                                    */
/* ----------------------------------------------------------------------- */

static inline void
MPIDI_Send_zero(pami_context_t   context,
                MPID_Request   * sreq,
                pami_endpoint_t  dest)
{
  MPIDI_MsgInfo * msginfo = &sreq->mpid.envelope.envelope.msginfo;

  pami_send_immediate_t params = {
  dispatch : MPIDI_Protocols.Send,
  dest     : dest,
  header   : {
    iov_base : msginfo,
    iov_len  : sizeof(MPIDI_MsgInfo),
    },
  data     : {
    iov_base : NULL,
    iov_len  : 0,
  },
  };

  pami_result_t rc;
  rc = PAMI_Send_immediate(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);

  MPIDI_SendDoneCB(context, sreq, PAMI_SUCCESS);
}


static inline void
MPIDI_Send_eager(pami_context_t   context,
                 MPID_Request   * sreq,
                 pami_endpoint_t  dest,
                 char           * sndbuf,
                 unsigned         sndlen)
{
  MPIDI_MsgInfo * msginfo = &sreq->mpid.envelope.envelope.msginfo;

  pami_send_t params = {
  send   : {
    dispatch : MPIDI_Protocols.Send,
    dest     : dest,
    header   : {
      iov_base : msginfo,
      iov_len  : sizeof(MPIDI_MsgInfo),
      },
    data     : {
      iov_base : sndbuf,
      iov_len  : sndlen,
    },
  },
  events : {
    cookie   : sreq,
    local_fn : MPIDI_SendDoneCB,
  },
  };

  pami_result_t rc;
  rc = PAMI_Send(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);
}


static inline void
MPIDI_Send_rzv(pami_context_t   context,
               MPID_Request   * sreq,
               pami_endpoint_t  dest,
               char           * sndbuf,
               unsigned         sndlen)
{
  pami_result_t rc;

  /* Set the isRzv bit in the SEND request. This is important for
   * canceling requests.
   */
  MPIDI_Request_setRzv(sreq, 1);

  /* The rendezvous information, such as the origin/local/sender
   * node's send buffer and the number of bytes the origin node wishes
   * to send, is sent as the payload of the request-to-send (RTS)
   * message.
   */
#ifdef USE_PAMI_RDMA
  size_t sndlen_out;
  rc = PAMI_Memregion_create(context,
                             sndbuf,
                             sndlen,
                             &sndlen_out,
                             &sreq->mpid.envelope.envelope.memregion);
  MPID_assert(rc == PAMI_SUCCESS);
  MPID_assert(sndlen == sndlen_out);
#else
  sreq->mpid.envelope.envelope.data   = sndbuf;
#endif
  sreq->mpid.envelope.envelope.length = sndlen;

  /* Do not specify a callback function to be invoked when the RTS
   * message has been sent. The MPI_Send is completed only when the
   * target/remote/receiver node has completed an PAMI_Get from the
   * origin node and has then sent a rendezvous acknowledgement (ACK)
   * to the origin node to signify the end of the transfer.  When the
   * ACK message is received by the origin node the same callback
   * function is used to complete the MPI_Send as the non-rendezvous
   * case.
   */
  pami_send_immediate_t params = {
  dispatch : MPIDI_Protocols.RTS,
  dest     : dest,
  header   : {
    iov_base : &sreq->mpid.envelope,
    iov_len  : sizeof(MPIDI_MsgEnvelope),
    },
  data     : {
    iov_base : NULL,
    iov_len  : 0,
  },
  };

  rc = PAMI_Send_immediate(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);
}


static inline void
MPIDI_Send(pami_context_t  context,
           MPID_Request  * sreq,
           char          * sndbuf,
           unsigned        sndlen)
{
  pami_endpoint_t dest    = MPIDI_Context_endpoint(sreq);

  pami_task_t old_peer = MPIDI_Request_getPeerRank(sreq);
  MPIDI_Request_setPeerRank(sreq, MPIDI_Process.global.rank);


  /* Always use the short protocol when sndlen is zero.
   */
  if (likely(sndlen==0))
    {
      MPIDI_Send_zero(context,
                      sreq,
                      dest);
    }
  /* Use the eager protocol when sndlen is less than the eager limit.
   */
  else if (sndlen < MPIDI_Process.eager_limit)
    {
      MPIDI_Send_eager(context,
                       sreq,
                       dest,
                       sndbuf,
                       sndlen);
    }
  /* Use the default rendezvous protocol (glue implementation that
   * guarantees no unexpected data).
   */
  else
    {
      MPIDI_Send_rzv(context,
                     sreq,
                     dest,
                     sndbuf,
                     sndlen);
    }


  MPIDI_Request_setPeerRank(sreq, old_peer);
}


/*
 * \brief Central function for all sends.
 * \param [in,out] sreq Structure containing all relevant info about the message.
 */
pami_result_t
MPIDI_StartMsg_handoff(pami_context_t   context,
                       void           * _sreq)
{
  MPID_Request * sreq = (MPID_Request *) _sreq;
  MPID_assert(sreq != NULL);

  int data_sz, dt_contig;
  MPID_Datatype *dt_ptr;
  MPI_Aint dt_true_lb;

  /* ----------------------------------------- */
  /* prerequisites: not sending to a NULL rank */
  /* request already allocated                 */
  /* not sending to self                       */
  /* ----------------------------------------- */
  MPID_assert(sreq != NULL);

  /* ----------------------------------------- */
  /*   get the datatype info                   */
  /* ----------------------------------------- */
  MPIDI_Datatype_get_info(sreq->mpid.userbufcount,
                          sreq->mpid.datatype,
                          dt_contig,
                          data_sz,
                          dt_ptr,
                          dt_true_lb);

  /* ----------------------------------------- */
  /* contiguous data type                      */
  /* ----------------------------------------- */
  if (dt_contig)
    {
      MPID_assert(sreq->mpid.uebuf == NULL);
      MPIDI_Send(context, sreq, (char *)sreq->mpid.userbuf + dt_true_lb, data_sz);
      return PAMI_SUCCESS;
    }

  /* ------------------------------------------- */
  /* allocate and populate temporary send buffer */
  /* ------------------------------------------- */
  if (sreq->mpid.uebuf == NULL)
    {
      MPID_Segment              segment;

      sreq->mpid.uebuf = MPIU_Malloc(data_sz);
      if (sreq->mpid.uebuf == NULL)
        {
          sreq->status.MPI_ERROR = MPI_ERR_NO_SPACE;
          sreq->status.count = 0;
          MPID_Abort(NULL, MPI_ERR_NO_SPACE, -1,
                     "Unable to allocate non-contiguous buffer");
        }

      DLOOP_Offset last = data_sz;
      MPID_Segment_init(sreq->mpid.userbuf,
                        sreq->mpid.userbufcount,
                        sreq->mpid.datatype,
                        &segment,0);
      MPID_Segment_pack(&segment, 0, &last, sreq->mpid.uebuf);
      MPID_assert(last == data_sz);
    }

  MPIDI_Send(context, sreq, sreq->mpid.uebuf, data_sz);
  return PAMI_SUCCESS;
}
