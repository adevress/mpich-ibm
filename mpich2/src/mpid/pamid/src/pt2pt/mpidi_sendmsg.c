/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_sendmsg.c
 * \brief Funnel point for starting all MPI messages
 */
#include "mpidimpl.h"


static inline void
MPIDI_SendMsg_short(pami_context_t    context,
                    MPID_Request    * sreq,
                    pami_endpoint_t   dest,
                    void            * sndbuf,
                    unsigned          sndlen)
{
  MPIDI_MsgInfo * msginfo = &sreq->mpid.envelope.msginfo;

  pami_send_immediate_t params = {
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
  };

  MPIU_THREAD_CS_ENTER(PAMI,);
  pami_result_t rc;
  rc = PAMI_Send_immediate(context, &params);
  MPIU_THREAD_CS_EXIT(PAMI,);
#ifdef TRACE_ON
  if (rc)
    {
      TRACE_ERR("sizeof(msginfo)=%zu sizeof(data)=%u\n", sizeof(MPIDI_MsgInfo), sndlen);
    }
#endif
  MPID_assert(rc == PAMI_SUCCESS);

  MPIDI_SendDoneCB(context, sreq, PAMI_SUCCESS);
}


static inline void
MPIDI_SendMsg_eager(pami_context_t    context,
                    MPID_Request    * sreq,
                    pami_endpoint_t   dest,
                    void            * sndbuf,
                    unsigned          sndlen)
{
  MPIDI_MsgInfo * msginfo = &sreq->mpid.envelope.msginfo;

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

  MPIU_THREAD_CS_ENTER(PAMI,);
  pami_result_t rc;
  rc = PAMI_Send(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);
  MPIU_THREAD_CS_EXIT(PAMI,);
}


static inline void
MPIDI_SendMsg_rzv(pami_context_t    context,
                  MPID_Request    * sreq,
                  pami_endpoint_t   dest,
                  void            * sndbuf,
                  unsigned          sndlen)
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
                             &sreq->mpid.envelope.memregion);
  MPID_assert(rc == PAMI_SUCCESS);
  MPID_assert(sndlen == sndlen_out);
  TRACE_ERR("RZV send for mr=%#llx addr=%p *addr[0]=%#016llx *addr[1]=%#016llx bytes=%u\n",
            *(unsigned long long*)&sreq->mpid.envelope.memregion,
            sndbuf,
            *(((unsigned long long*)sndbuf)+0),
            *(((unsigned long long*)sndbuf)+1),
            sndlen);
#else
  sreq->mpid.envelope.data   = sndbuf;
#endif
  sreq->mpid.envelope.length = sndlen;

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

  MPIU_THREAD_CS_ENTER(PAMI,);
  rc = PAMI_Send_immediate(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);
  MPIU_THREAD_CS_EXIT(PAMI,);
}


/*
 * \brief Central function for all low-level sends.
 *
 * This is assumed to have been posted to a context, and is now being
 * called from inside advance.  This has (unspecified) locking
 * implications.
 *
 * Prerequisites:
 *    + Not sending to a NULL rank
 *    + Request already allocated
 *
 * \param[in]     context The PAMI context on which to do the send operation
 * \param[in,out] sreq    Structure containing all relevant info about the message.
 */
pami_result_t
MPIDI_SendMsg_handoff(pami_context_t   context,
                      void           * _sreq)
{
  MPID_Request * sreq = (MPID_Request*)_sreq;
  MPID_assert(sreq != NULL);

  int             data_sz;
  int             dt_contig;
  MPI_Aint        dt_true_lb;
  MPID_Datatype * dt_ptr;
  void          * sndbuf;

  MPIDI_Request_setPeerRequest(sreq, sreq);

  /* ------------------------------ */
  /* special case: NULL destination */
  /* ------------------------------ */
  if (unlikely(MPIDI_Request_getPeerRank(sreq) == MPI_PROC_NULL))
    {
      MPID_cc_set(&sreq->cc, 0);
      MPIDI_Progress_signal();
      return MPI_SUCCESS;
    }

  /*
   * Create the destination endpoint
   */
  pami_endpoint_t dest;
  MPIDI_Context_endpoint(sreq, &dest);

  /*
   * Get the datatype info
   */
  MPIDI_Datatype_get_info(sreq->mpid.userbufcount,
                          sreq->mpid.datatype,
                          dt_contig,
                          data_sz,
                          dt_ptr,
                          dt_true_lb);

  MPID_assert(sreq->mpid.uebuf == NULL);

  /*
   * Contiguous data type
   */
  if (likely(dt_contig))
    {
      sndbuf = sreq->mpid.userbuf + dt_true_lb;
    }

  /*
   * Non-contiguous data type; allocate and populate temporary send
   * buffer
   */
  else
    {
      MPID_Segment segment;

      sreq->mpid.uebuf = sndbuf = MPIU_Malloc(data_sz);
      if (unlikely(sndbuf == NULL))
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
                        &segment,
                        0);
      MPID_Segment_pack(&segment, 0, &last, sndbuf);
      MPID_assert(last == data_sz);
    }


  /*
   * Always use the short protocol when data_sz is small.
   */
  if (likely(data_sz <= MPIDI_Process.short_limit))
    {
      TRACE_ERR("Sending(short) bytes=%u (eager_limit=%u)\n", data_sz, MPIDI_Process.eager_limit);
      MPIDI_SendMsg_short(context,
                          sreq,
                          dest,
                          sndbuf,
                          data_sz);
    }
  /*
   * Use the eager protocol when data_sz is less than the eager limit.
   */
  else if (data_sz < MPIDI_Process.eager_limit)
    {
      TRACE_ERR("Sending(eager) bytes=%u (eager_limit=%u)\n", data_sz, MPIDI_Process.eager_limit);
      MPIDI_SendMsg_eager(context,
                          sreq,
                          dest,
                          sndbuf,
                          data_sz);
    }
  /*
   * Use the default rendezvous protocol (glue implementation that
   * guarantees no unexpected data).
   */
  else
    {
      TRACE_ERR("Sending(RZV) bytes=%u (eager_limit=%u)\n", data_sz, MPIDI_Process.eager_limit);
      MPIDI_SendMsg_rzv(context,
                        sreq,
                        dest,
                        sndbuf,
                        data_sz);
    }

  return PAMI_SUCCESS;
}


/*
 * \brief Central function for all low-level sends.
 *
 * This is assumed to have been posted to a context, and is now being
 * called from inside advance.  This has (unspecified) locking
 * implications.
 *
 * Prerequisites:
 *    + Not sending to a NULL rank
 *    + Request already allocated
 *
 * \param[in]     context The PAMI context on which to do the send operation
 * \param[in,out] sreq    Structure containing all relevant info about the message.
 */
pami_result_t
MPIDI_Isend_handoff(pami_context_t   context,
                    void           * _sreq)
{
  MPID_Request * sreq = (MPID_Request*)_sreq;
  MPID_assert(sreq != NULL);

  MPID_Request_initialize(sreq);
  MPIDI_Request_setSync(sreq, 0);

  int rank = MPIDI_Request_getPeerRank(sreq);
  if (likely(rank != MPI_PROC_NULL))
    MPIDI_Request_setPeerRank(sreq, MPID_VCR_GET_LPID(sreq->comm->vcr, rank));

  return MPIDI_SendMsg_handoff(context, sreq);
}
