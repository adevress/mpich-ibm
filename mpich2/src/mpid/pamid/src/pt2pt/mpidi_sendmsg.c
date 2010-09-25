/*  (C)Copyright IBM Corp.  2007, 2008  */
/**
 * \file src/pt2pt/mpidi_sendmsg.c
 * \brief Funnel point for starting all MPI messages
 */
#include "mpidimpl.h"

/**
 * \brief This is a generic inline verion of the various send functions.
 *
 * This is the same function prototype as MPID_{,I}{,S}send.c, but it
 * inludes two inline flags: is_blocking and is_sync.  These are not
 * meant to be truely variable, but are expected to be known at
 * compile time.  That *should* allow the compiler to remove any extra
 * code from this function that won't be used in the specific inlined
 * version.
 *
 * The MPIDI_SendMsg function may queue this send operation for later
 * handling and then return to the user.  Therefore, this function has
 * only two goals:
 *
 *   + Fill in the request oject with all relevant information, and
 *   + Do any reference counting that must be done before the function returns.
 *
 *
 * \param[in]  buf            The buffer to send
 * \param[in]  count          Number of elements in the buffer
 * \param[in]  datatype       The datatype of each element
 * \param[in]  rank           The destination rank
 * \param[in]  tag            The message tag
 * \param[in]  comm           Pointer to the communicator
 * \param[in]  context_offset Offset from the communicator context ID
 * \param[in]  is_blocking    Is this a blocking request? (i.e. one of MPI_Send or MPI_Ssend)
 * \param[in]  is_sync        Is this a synchronous request? (i.e. of of MPI_Ssend or MPI_Issend)
 * \param[out] request        Return a pointer to the new request object
 *
 * \returns An MPI Error code
 */
int
MPIDI_Send(const void    * buf,
           int             count,
           MPI_Datatype    datatype,
           int             rank,
           int             tag,
           MPID_Comm     * comm,
           int             context_offset,
           unsigned        is_blocking,
           unsigned        is_sync,
           MPID_Request ** request)
{
  MPID_Request * sreq = NULL;

  /* --------------------------- */
  /* special case: send-to-self  */
  /* --------------------------- */

  if (unlikely( (rank == comm->rank) &&
                (comm->comm_kind != MPID_INTERCOMM) ) )
    {
      /* I'm sending to myself! */
      int mpi_errno = MPIDI_Isend_self(buf,
                                       count,
                                       datatype,
                                       rank,
                                       tag,
                                       comm,
                                       context_offset,
                                       request);
      if (is_blocking)
        if ( (MPIR_ThreadInfo.thread_provided <= MPI_THREAD_FUNNELED) &&
             (sreq != NULL && !MPID_cc_is_complete(&sreq->cc)) )
          {
            *request = NULL;
            mpi_errno = MPIR_Err_create_code(MPI_SUCCESS,
                                             MPIR_ERR_FATAL,
                                             __FUNCTION__,
                                             __LINE__,
                                             MPI_ERR_OTHER,
                                             "**dev|selfsenddeadlock", 0);
            return mpi_errno;
          }
      return mpi_errno;
    }

  /* --------------------- */
  /* create a send request */
  /* --------------------- */

  sreq = MPID_Request_create();
  if (unlikely(sreq == NULL))
    {
      *request = NULL;
      int mpi_errno = MPIR_Err_create_code(MPI_SUCCESS,
                                           MPIR_ERR_FATAL,
                                           __FUNCTION__,
                                           __LINE__,
                                           MPI_ERR_OTHER,
                                           "**nomem", 0);
      return mpi_errno;
    }

  /* match info */
  MPIDI_Request_setMatch(sreq, tag, comm->rank, comm->context_id+context_offset);

  /* data buffer info */
  sreq->mpid.userbuf      = (void*)buf;
  sreq->mpid.userbufcount = count;
  sreq->mpid.datatype     = datatype;

  /* communicator & destination info */
  sreq->comm              = comm;  MPIR_Comm_add_ref(comm);
  if (likely(rank != MPI_PROC_NULL))
    MPIDI_Request_setPeerRank(sreq, comm->vcr[rank]);
  MPIDI_Request_setPeerRequest(sreq, sreq);

  /* message type info */
  sreq->kind = MPID_REQUEST_SEND;
  if (is_sync)
    MPIDI_Request_setSync(sreq, 1);

  /* ------------------------------ */
  /* special case: NULL destination */
  /* ------------------------------ */
  if (unlikely(rank == MPI_PROC_NULL))
    {
      if (is_blocking)
        {
          *request = NULL;
        }
      else
        {
          MPID_cc_set(&sreq->cc, 0);
          *request = sreq;
        }
      return MPI_SUCCESS;
    }

  /* ----------------------------------------- */
  /*      start the message                    */
  /* ----------------------------------------- */

  *request = sreq;
  MPIDI_SendMsg(sreq);
  return MPI_SUCCESS;
}

static inline void
MPIDI_SendMsg_short(pami_context_t    context,
                    MPID_Request    * sreq,
                    pami_endpoint_t   dest,
                    void            * sndbuf,
                    unsigned          sndlen)
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
      TRACE_ERR("msginfo=%zu data=%zu\n", sizeof(MPIDI_MsgInfo), sndlen);
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
 *    + Not sending to self
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

  /** /todo Remove this assert.  It shouldn't be required */
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
      MPIDI_SendMsg_rzv(context,
                        sreq,
                        dest,
                        sndbuf,
                        data_sz);
    }

  return PAMI_SUCCESS;
}
