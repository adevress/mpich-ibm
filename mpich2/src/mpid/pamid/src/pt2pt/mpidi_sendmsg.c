/* begin_generated_IBM_copyright_prolog                             */
/*                                                                  */
/* This is an automatically generated copyright prolog.             */
/* After initializing,  DO NOT MODIFY OR MOVE                       */
/*  --------------------------------------------------------------- */
/* Licensed Materials - Property of IBM                             */
/* Blue Gene/Q 5765-PER 5765-PRP                                    */
/*                                                                  */
/* (C) Copyright IBM Corp. 2011, 2012 All Rights Reserved           */
/* US Government Users Restricted Rights -                          */
/* Use, duplication, or disclosure restricted                       */
/* by GSA ADP Schedule Contract with IBM Corp.                      */
/*                                                                  */
/*  --------------------------------------------------------------- */
/*                                                                  */
/* end_generated_IBM_copyright_prolog                               */
/*  (C)Copyright IBM Corp.  2007, 2011  */
/**
 * \file src/pt2pt/mpidi_sendmsg.c
 * \brief Funnel point for starting all MPI messages
 */
#include <mpidimpl.h>


static inline void
MPIDI_SendMsg_short(pami_context_t    context,
                    MPID_Request    * sreq,
                    pami_endpoint_t   dest,
                    void            * sndbuf,
                    unsigned          sndlen,
                    unsigned          isSync)
{
  MPIDI_MsgInfo * msginfo = &sreq->mpid.envelope.msginfo;

  pami_send_immediate_t params = {
    .dispatch = MPIDI_Protocols_Short,
    .dest     = dest,
    .header   = {
      .iov_base = msginfo,
      .iov_len  = sizeof(MPIDI_MsgInfo),
    },
    .data     = {
      .iov_base = sndbuf,
      .iov_len  = sndlen,
    },
  };
  if (isSync)
    params.dispatch = MPIDI_Protocols_ShortSync;

  pami_result_t rc;
  rc = PAMI_Send_immediate(context, &params);
#ifdef TRACE_ON
  if (rc)
    {
      TRACE_ERR("sizeof(msginfo)=%zu sizeof(data)=%u\n", sizeof(MPIDI_MsgInfo), sndlen);
    }
#endif
  MPID_assert(rc == PAMI_SUCCESS);

  MPIDI_SendDoneCB_inline(context, sreq, PAMI_SUCCESS);
#if (MPIDI_STATISTICS)
  MPID_NSTAT(mpid_statp->sendsComplete);
#endif
}

static void
MPIDI_SendMsg_eager(pami_context_t    context,
                    MPID_Request    * sreq,
                    pami_endpoint_t   dest,
                    void            * sndbuf,
                    unsigned          sndlen)
  __attribute__((__noinline__));
static void
MPIDI_SendMsg_eager(pami_context_t    context,
                    MPID_Request    * sreq,
                    pami_endpoint_t   dest,
                    void            * sndbuf,
                    unsigned          sndlen)
{
  MPIDI_MsgInfo * msginfo = &sreq->mpid.envelope.msginfo;

  pami_send_t params = {
    .send   = {
      .dispatch = MPIDI_Protocols_Eager,
      .dest     = dest,
      .header   = {
        .iov_base = msginfo,
        .iov_len  = sizeof(MPIDI_MsgInfo),
      },
      .data     = {
        .iov_base = sndbuf,
        .iov_len  = sndlen,
      },
    },
    .events = {
      .cookie   = sreq,
      .local_fn = MPIDI_SendDoneCB,
      .remote_fn= NULL,
    },
  };

  pami_result_t rc;
  rc = PAMI_Send(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);
}


static void
MPIDI_SendMsg_rzv(pami_context_t    context,
                  MPID_Request    * sreq,
                  pami_endpoint_t   dest,
                  void            * sndbuf,
                  unsigned          sndlen)
  __attribute__((__noinline__));
static void
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
  sreq->mpid.envelope.memregion_used = 0;
#ifdef OUT_OF_ORDER_HANDLING
  if ((!MPIDI_Process.mp_s_use_pami_get) && (!sreq->mpid.shm))
#else
  if (!MPIDI_Process.mp_s_use_pami_get)
#endif
    {
      size_t sndlen_out;
      rc = PAMI_Memregion_create(context,
				 sndbuf,
				 sndlen,
				 &sndlen_out,
				 &sreq->mpid.envelope.memregion);
      if(rc == PAMI_SUCCESS)
	{
	  MPID_assert(sndlen == sndlen_out);
	  TRACE_ERR("RZV send for mr=%#llx addr=%p *addr[0]=%#016llx *addr[1]=%#016llx bytes=%u\n",
		    *(unsigned long long*)&sreq->mpid.envelope.memregion,
		    sndbuf,
		    *(((unsigned long long*)sndbuf)+0),
		    *(((unsigned long long*)sndbuf)+1),
		    sndlen);
	  sreq->mpid.envelope.memregion_used = 1;
	}
        sreq->mpid.envelope.data   = sndbuf;
    } else {
      TRACE_ERR("RZV send (failed registration for sreq=%p addr=%p *addr[0]=%#016llx *addr[1]=%#016llx bytes=%u\n",
		sreq,sndbuf,
		*(((unsigned long long*)sndbuf)+0),
		*(((unsigned long long*)sndbuf)+1),
		sndlen);
      sreq->mpid.envelope.data   = sndbuf;
    }
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
    .dispatch = MPIDI_Protocols_RVZ,
    .dest     = dest,
    .header   = {
      .iov_base = &sreq->mpid.envelope,
      .iov_len  = sizeof(MPIDI_MsgEnvelope),
    },
    .data     = {
      .iov_base = NULL,
      .iov_len  = 0,
    },
  };

  rc = PAMI_Send_immediate(context, &params);
  MPID_assert(rc == PAMI_SUCCESS);
}


static void
MPIDI_SendMsg_process_userdefined_dt(MPID_Request      * sreq,
                                     void             ** sndbuf,
                                     size_t            * data_sz)
  __attribute__((__noinline__));
static void
MPIDI_SendMsg_process_userdefined_dt(MPID_Request      * sreq,
                                     void             ** _sndbuf,
                                     size_t            * _data_sz)
{
  size_t          data_sz;
  int             dt_contig;
  MPI_Aint        dt_true_lb;
  MPID_Datatype * dt_ptr;
  void          * sndbuf;

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

  *_sndbuf = sndbuf;
  *_data_sz = data_sz;
}


static inline void
MPIDI_SendMsg(pami_context_t   context,
              MPID_Request   * sreq,
              unsigned         isSync)
{
  /* ------------------------------ */
  /* special case: NULL destination */
  /* ------------------------------ */
  int rank = MPIDI_Request_getPeerRank_comm(sreq);
  if (unlikely(rank == MPI_PROC_NULL))
    {
      if (isSync)
        MPIDI_Request_complete(sreq);
      MPIDI_Request_complete(sreq);
      return;
    }
  else
    {
      MPIDI_Request_setPeerRank_pami(sreq, MPID_VCR_GET_LPID(sreq->comm->vcr, rank));
    }

  MPIDI_Request_setSync(sreq, isSync);
  MPIDI_Request_setPeerRequestH(sreq);

  /*
   * Create the destination endpoint
   */
  pami_endpoint_t dest;
  MPIDI_Context_endpoint(sreq, &dest);
#if (MPIDI_STATISTICS)
  MPID_NSTAT(mpid_statp->sends);
#endif
#ifdef OUT_OF_ORDER_HANDLING
  MPIDI_Out_cntr_t *out_cntr;
  pami_task_t desttid;

  desttid = PAMIX_Endpoint_query(dest);
  MPIU_THREAD_CS_ENTER(MSGQUEUE,0);
  out_cntr = &MPIDI_Out_cntr[desttid];
  out_cntr->nMsgs++;
  MPIU_THREAD_CS_EXIT(MSGQUEUE,0);
  MPIDI_Request_setMatchSeq(sreq, out_cntr->nMsgs);
#endif

  size_t   data_sz;
  void   * sndbuf;
  if (likely(HANDLE_GET_KIND(sreq->mpid.datatype) == HANDLE_KIND_BUILTIN))
    {
      sndbuf   = sreq->mpid.userbuf;
      data_sz  = sreq->mpid.userbufcount * MPID_Datatype_get_basic_size(sreq->mpid.datatype);
    }
  else
    {
      MPIDI_SendMsg_process_userdefined_dt(sreq, &sndbuf, &data_sz);
    }


#ifdef OUT_OF_ORDER_HANDLING
       sreq->mpid.shm=0;
#endif
  if (unlikely(PAMIX_Task_is_local(rank) != 0))
    {
      /*
       * Always use the short protocol when data_sz is small.
       */
      if (likely(data_sz < MPIDI_Process.short_limit))
        {
          TRACE_ERR("Sending(short,intranode) bytes=%u (short_limit=%u)\n", data_sz, MPIDI_Process.short_limit);
          MPIDI_SendMsg_short(context,
                              sreq,
                              dest,
                              sndbuf,
                              data_sz,
                              isSync);
        }
      /*
       * Use the eager protocol when data_sz is less than the 'local' eager limit.
       */
      else if (data_sz < MPIDI_Process.eager_limit_local)
        {
          TRACE_ERR("Sending(eager,intranode) bytes=%u (eager_limit_local=%u)\n", data_sz, MPIDI_Process.eager_limit_local);
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
          TRACE_ERR("Sending(RZV,intranode) bytes=%u (eager_limit=%u)\n", data_sz, MPIDI_Process.eager_limit);
#ifdef OUT_OF_ORDER_HANDLING
          sreq->mpid.shm=1;
#endif
          MPIDI_SendMsg_rzv(context,
                            sreq,
                            dest,
                            sndbuf,
                            data_sz);
      }
    }
  /*
   * Always use the short protocol when data_sz is small.
   */
  else if (likely(data_sz < MPIDI_Process.short_limit))
    {
      TRACE_ERR("Sending(short) bytes=%u (eager_limit=%u)\n", data_sz, MPIDI_Process.eager_limit);
      MPIDI_SendMsg_short(context,
                          sreq,
                          dest,
                          sndbuf,
                          data_sz,
                          isSync);
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
    #ifdef MPIDI_STATISTICS
      if (MPID_cc_is_complete(&sreq->cc)) {
          MPID_NSTAT(mpid_statp->sendsComplete);
      }
    #endif
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
      #ifdef MPIDI_STATISTICS
         if (MPID_cc_is_complete(&sreq->cc)) {
             MPID_NSTAT(mpid_statp->sendsComplete);
       }
     #endif
    }
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
MPIDI_Send_handoff(pami_context_t   context,
                   void           * _sreq)
{
  MPID_Request * sreq = (MPID_Request*)_sreq;
  MPID_assert(sreq != NULL);

  MPIDI_SendMsg(context, sreq, 0);
  return PAMI_SUCCESS;
}


pami_result_t
MPIDI_Ssend_handoff(pami_context_t   context,
                   void           * _sreq)
{
  MPID_Request * sreq = (MPID_Request*)_sreq;
  MPID_assert(sreq != NULL);

  MPIDI_SendMsg(context, sreq, 1);
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

  /* This initializes all the fields not set in MPI_Isend() */
  MPIDI_Request_initialize(sreq);

  /* Since this is only called from MPI_Isend(), it is not synchronous */
  MPIDI_SendMsg(context, sreq, 0);
  return PAMI_SUCCESS;
}
